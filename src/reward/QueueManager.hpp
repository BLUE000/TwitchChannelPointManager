#pragma once

#include <QObject>
#include <QQueue>
#include <QDateTime>
#include <QMutex>
#include "Reward.hpp"

class Database;

struct QueueItem {
    QString queueId;
    QString rewardId;
    QString username;
    QDateTime timestamp;
    QQueue<Effect> effects; // インデックスのズレを完全に防ぐキュー構造
};

class QueueManager : public QObject {
    Q_OBJECT
private:
    QQueue<QueueItem> m_queue;
    bool m_isPlaying;
    Database* m_database;
    mutable QMutex m_mutex;

public:
    explicit QueueManager(Database* database, QObject* parent = nullptr);
    ~QueueManager() = default;

    // キューへ追加 (EventSubなどからのトリガー)
    void enqueueRedemption(const QString& rewardId, const QString& username, const QDateTime& timestamp);
    
    // キュー管理操作
    void clearQueue();
    void stopAllEffects(); // パニックボタン用

    // デバッグ・GUI用情報取得
    int pendingCount() const;
    bool isPlaying() const { return m_isPlaying; }

signals:
    void queueUpdated(int pendingCount);
    
    // OverlayServer へ演出指示を送るシグナル
    void playEffectRequested(const QueueItem& item, const Effect& effect);
    
    // パニック・全消去の指示を OBS へ送るシグナル
    void stopAllRequested();
    void clearQueueRequested();

public slots:
    // OBS が一つの演出の描画再生を完了した際に呼ばれる
    void onEffectCompleted(const QString& queueId);

private:
    void processNext();
};
