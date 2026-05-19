#include "QueueManager.hpp"
#include "../core/Logger.hpp"
#include "../database/Database.hpp"
#include <QUuid>
#include <QRandomGenerator>

QueueManager::QueueManager(Database* database, QObject* parent)
    : QObject(parent)
    , m_isPlaying(false)
    , m_database(database)
{
}

void QueueManager::enqueueRedemption(const QString& rewardId, const QString& username, const QDateTime& timestamp)
{
    LOG_INFO(QString("Enqueuing redemption for Reward: %1, User: %2").arg(rewardId).arg(username));

    // データベースから報酬の情報をロード
    Reward reward;
    bool found = false;
    
    if (m_database) {
        QList<Reward> rewards;
        if (m_database->loadRewards(rewards)) {
            for (const auto& r : rewards) {
                if (r.id == rewardId) {
                    reward = r;
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found) {
        LOG_WARN(QString("Reward with ID %1 was not found in database. Using empty fallback reward.").arg(rewardId));
        reward.id = rewardId;
        reward.name = "Unknown Reward";
        reward.mode = "sequential";
    }

    if (reward.effects.isEmpty()) {
        LOG_WARN(QString("Reward '%1' has no configured effects. Skipping playback.").arg(reward.name));
        return;
    }

    QueueItem item;
    item.queueId = QUuid::createUuid().toString(QUuid::IdHeaderType::WithoutBraces);
    item.rewardId = rewardId;
    item.username = username;
    item.timestamp = timestamp;

    // 再生モードに応じてエフェクトをロード
    if (reward.mode == "random") {
        // ランダムモード: エフェクトリストからランダムで1つだけ選択
        int index = QRandomGenerator::global()->bounded(reward.effects.size());
        item.effects.append(reward.effects.at(index));
        LOG_INFO(QString("Random playback mode selected effect index: %1").arg(index));
    } else {
        // 通常モード: すべてのエフェクトを順番に再生
        item.effects = reward.effects;
    }

    // データベースに使用ログを書き込み
    if (m_database) {
        m_database->logUsage(rewardId, username, timestamp);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_queue.enqueue(item);
    }

    LOG_INFO(QString("Added to queue. Current queue length: %1").arg(pendingCount()));
    emit queueUpdated(pendingCount());

    // 現在再生中でなければ再生を開始
    if (!m_isPlaying) {
        m_isPlaying = true;
        processNext();
    }
}

void QueueManager::clearQueue()
{
    LOG_INFO("Clearing all pending events from the queue.");
    {
        QMutexLocker locker(&m_mutex);
        m_queue.clear();
        m_isPlaying = false;
    }
    emit queueUpdated(0);
    emit clearQueueRequested();
}

void QueueManager::stopAllEffects()
{
    LOG_WARN("Emergency Panic Button triggered! Stopping all active overlay effects.");
    {
        QMutexLocker locker(&m_mutex);
        m_queue.clear();
        m_isPlaying = false;
    }
    emit queueUpdated(0);
    emit stopAllRequested();
}

int QueueManager::pendingCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
}

void QueueManager::processNext()
{
    QueueItem currentItem;
    Effect currentEffect;
    bool hasEffect = false;

    {
        QMutexLocker locker(&m_mutex);
        
        if (m_queue.isEmpty()) {
            m_isPlaying = false;
            LOG_INFO("Queue is now empty. Idle state.");
            return;
        }

        // 先頭のキューアイテムを取得
        currentItem = m_queue.head();

        if (currentItem.currentEffectIndex < currentItem.effects.size()) {
            currentEffect = currentItem.effects.at(currentItem.currentEffectIndex);
            hasEffect = true;
        } else {
            // このアイテムの全てのエフェクトが終了したのでキューから取り除く
            m_queue.dequeue();
            LOG_INFO(QString("Completed all effects for Queue ID: %1").arg(currentItem.queueId));
        }
    }

    emit queueUpdated(pendingCount());

    if (hasEffect) {
        LOG_INFO(QString("Playing effect %1/%2 for Queue ID: %3 (%4)")
            .arg(currentItem.currentEffectIndex + 1)
            .arg(currentItem.effects.size())
            .arg(currentItem.queueId)
            .arg(currentEffect.type));

        emit playEffectRequested(currentItem, currentEffect);
    } else {
        // 次のキューアイテムの処理へ再帰的に移行
        processNext();
    }
}

void QueueManager::onEffectCompleted(const QString& queueId)
{
    LOG_INFO(QString("Received effect completed confirmation from OBS for Queue ID: %1").arg(queueId));
    
    bool valid = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_queue.isEmpty() && m_queue.head().queueId == queueId) {
            // インデックスを進める
            m_queue.head().currentEffectIndex++;
            valid = true;
        }
    }

    if (valid) {
        processNext();
    } else {
        LOG_WARN(QString("Discarding stale or invalid effect completed response for Queue ID: %1").arg(queueId));
    }
}
