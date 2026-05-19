#pragma once

#include <QObject>
#include <QMap>
#include <QDateTime>
#include "Reward.hpp"

class Database;

class RewardManager : public QObject {
    Q_OBJECT
private:
    Database* m_database;
    QMap<QString, Reward> m_rewardsCache; // RewardId -> Reward
    QMap<QString, QDateTime> m_cooldowns; // RewardId -> 次に使用可能になる日時

public:
    explicit RewardManager(Database* database, QObject* parent = nullptr);
    ~RewardManager() = default;

    // DBから全報酬データをロード
    bool loadAllRewards();

    // 報酬データの取得・一覧
    QList<Reward> getAllRewards() const;
    bool getReward(const QString& rewardId, Reward& reward) const;

    // 編集・保存・削除 (DBとキャッシュを同期)
    bool saveReward(const Reward& reward);
    bool deleteReward(const QString& rewardId);

    // バリデーションチェック (EventSub受信時に実行)
    // 戻り値: 利用可能なら true、NG（無効、クールダウン中、ロール制限）なら false
    bool validateRedemption(const QString& rewardId, const QString& username, QString& outReason);

    // クールダウン設定
    void triggerCooldown(const QString& rewardId);
};
