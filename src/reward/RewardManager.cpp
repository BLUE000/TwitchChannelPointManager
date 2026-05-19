#include "RewardManager.hpp"
#include "../core/Logger.hpp"
#include "../database/Database.hpp"

RewardManager::RewardManager(Database* database, QObject* parent)
    : QObject(parent)
    , m_database(database)
{
}

bool RewardManager::loadAllRewards()
{
    if (!m_database) return false;

    QList<Reward> rewards;
    if (!m_database->loadRewards(rewards)) {
        LOG_ERROR("Failed to load rewards from database into cache.");
        return false;
    }

    m_rewardsCache.clear();
    for (const auto& r : rewards) {
        m_rewardsCache.insert(r.id, r);
    }

    LOG_INFO(QString("RewardManager successfully cached %1 rewards.").arg(m_rewardsCache.size()));
    return true;
}

QList<Reward> RewardManager::getAllRewards() const
{
    return m_rewardsCache.values();
}

bool RewardManager::getReward(const QString& rewardId, Reward& reward) const
{
    if (m_rewardsCache.contains(rewardId)) {
        reward = m_rewardsCache.value(rewardId);
        return true;
    }
    return false;
}

bool RewardManager::saveReward(const Reward& reward)
{
    if (!m_database) return false;

    if (m_database->saveReward(reward)) {
        m_rewardsCache.insert(reward.id, reward);
        LOG_INFO("Successfully saved reward and updated cache: " + reward.name);
        return true;
    }
    return false;
}

bool RewardManager::deleteReward(const QString& rewardId)
{
    if (!m_database) return false;

    if (m_database->deleteReward(rewardId)) {
        m_rewardsCache.remove(rewardId);
        m_cooldowns.remove(rewardId);
        LOG_INFO("Successfully deleted reward and flushed cache: " + rewardId);
        return true;
    }
    return false;
}

bool RewardManager::validateRedemption(const QString& rewardId, const QString& username, QString& outReason)
{
    // キャッシュに存在するか
    if (!m_rewardsCache.contains(rewardId)) {
        outReason = "設定されていない報酬IDです。";
        return false;
    }

    const Reward& r = m_rewardsCache.value(rewardId);

    // 有効化されているか
    if (!r.enabled) {
        outReason = QString("報酬 '%1' は無効化されています。").arg(r.name);
        return false;
    }

    // クールダウンチェック
    QDateTime now = QDateTime::currentDateTime();
    if (m_cooldowns.contains(rewardId)) {
        QDateTime nextAvailable = m_cooldowns.value(rewardId);
        if (now < nextAvailable) {
            qint64 diffSec = now.secsTo(nextAvailable);
            outReason = QString("クールダウン中です（残り %1 秒）。").arg(diffSec);
            return false;
        }
    }

    return true;
}

void RewardManager::triggerCooldown(const QString& rewardId)
{
    if (!m_rewardsCache.contains(rewardId)) return;

    const Reward& r = m_rewardsCache.value(rewardId);
    if (r.cooldown > 0) {
        QDateTime nextAvailable = QDateTime::currentDateTime().addSecs(r.cooldown);
        m_cooldowns.insert(rewardId, nextAvailable);
        LOG_INFO(QString("Triggered cooldown for reward '%1' until %2").arg(r.name).arg(nextAvailable.toString("yyyy-MM-dd hh:mm:ss")));
    }
}
