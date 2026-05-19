#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>
#include <QDateTime>
#include "../reward/Reward.hpp"

struct UserUsageStat {
    QString username;
    QString rewardName;
    int count;
};

class Database : public QObject {
    Q_OBJECT
private:
    QSqlDatabase m_db;
    QString m_connectionName;

public:
    explicit Database(QObject* parent = nullptr);
    ~Database();

    // データベース接続のオープン・クローズ
    bool open(const QString& dbPath);
    void close();

    // 報酬データのCRUD操作
    bool saveReward(const Reward& reward);
    bool loadRewards(QList<Reward>& rewards);
    bool deleteReward(const QString& rewardId);

    // 演出履歴（使用ログ）の書き込み・集計
    bool logUsage(const QString& rewardId, const QString& username, const QDateTime& timestamp);
    int getTodayUsageCount();
    QList<QPair<QString, int>> getRanking(int periodIndex = 0);
    QList<UserUsageStat> getUserUsageStatistics(int periodIndex = 0);
    bool clearUsageLogs();

    // 汎用設定（Settings）テーブルの読み書き
    bool saveSetting(const QString& key, const QString& value);
    QString getSetting(const QString& key, const QString& defaultValue = "") const;

private:
    bool createTables();
};
