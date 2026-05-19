#include "Database.hpp"
#include "../core/Logger.hpp"
#include "../core/Constants.hpp"
#include <QSqlError>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

Database::Database(QObject* parent)
    : QObject(parent)
{
    m_connectionName = QString("TwitchOverlayDb_%1").arg(QUuid::createUuid().toString().replace("{", "").replace("}", ""));
}

Database::~Database()
{
    close();
}

bool Database::open(const QString& dbPath)
{
    LOG_INFO("Opening SQLite database at path: " + dbPath);

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        LOG_ERROR("Failed to open database file: " + m_db.lastError().text());
        return false;
    }

    LOG_INFO("Database successfully opened.");
    return createTables();
}

void Database::close()
{
    if (m_db.isOpen()) {
        m_db.close();
        LOG_INFO("Database closed.");
    }
    // 静的接続の削除
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::createTables()
{
    QSqlQuery query(m_db);

    // 1. rewards テーブルの作成
    QString createRewardsSql = R"(
        CREATE TABLE IF NOT EXISTS rewards (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            cost INTEGER NOT NULL,
            cooldown INTEGER NOT NULL,
            allowed_roles TEXT NOT NULL,
            enabled BOOLEAN NOT NULL DEFAULT 1,
            mode TEXT NOT NULL DEFAULT 'sequential',
            effects TEXT NOT NULL
        )
    )";
    if (!query.exec(createRewardsSql)) {
        LOG_ERROR("Failed to create rewards table: " + query.lastError().text());
        return false;
    }

    // 2. usage_logs テーブルの作成
    QString createLogsSql = R"(
        CREATE TABLE IF NOT EXISTS usage_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            reward_id TEXT NOT NULL,
            username TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            FOREIGN KEY(reward_id) REFERENCES rewards(id) ON DELETE CASCADE
        )
    )";
    if (!query.exec(createLogsSql)) {
        LOG_ERROR("Failed to create usage_logs table: " + query.lastError().text());
        return false;
    }

    // 3. settings テーブルの作成
    QString createSettingsSql = R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )";
    if (!query.exec(createSettingsSql)) {
        LOG_ERROR("Failed to create settings table: " + query.lastError().text());
        return false;
    }

    LOG_INFO("Database tables successfully validated.");
    return true;
}

bool Database::saveReward(const Reward& reward)
{
    LOG_INFO("Saving custom reward to database: " + reward.name + " (" + reward.id + ")");

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO rewards (id, name, cost, cooldown, allowed_roles, enabled, mode, effects)
        VALUES (:id, :name, :cost, :cooldown, :allowed_roles, :enabled, :mode, :effects)
    )");

    // ロール許可リストのJSON化
    QJsonArray rolesArr;
    for (const auto& role : reward.allowedRoles) {
        rolesArr.append(role);
    }
    QString rolesJson = QJsonDocument(rolesArr).toJson(QJsonDocument::Compact);

    // 演出効果のJSON化
    QJsonArray effectsArr;
    for (const auto& eff : reward.effects) {
        effectsArr.append(eff.toJson());
    }
    QString effectsJson = QJsonDocument(effectsArr).toJson(QJsonDocument::Compact);

    query.bindValue(":id", reward.id);
    query.bindValue(":name", reward.name);
    query.bindValue(":cost", reward.cost);
    query.bindValue(":cooldown", reward.cooldown);
    query.bindValue(":allowed_roles", rolesJson);
    query.bindValue(":enabled", reward.enabled ? 1 : 0);
    query.bindValue(":mode", reward.mode);
    query.bindValue(":effects", effectsJson);

    if (!query.exec()) {
        LOG_ERROR("Failed to save reward to DB: " + query.lastError().text());
        return false;
    }

    return true;
}

bool Database::loadRewards(QList<Reward>& rewards)
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, name, cost, cooldown, allowed_roles, enabled, mode, effects FROM rewards")) {
        LOG_ERROR("Failed to query rewards from DB: " + query.lastError().text());
        return false;
    }

    rewards.clear();
    while (query.next()) {
        Reward r;
        r.id = query.value(Constants::COL_REWARD_ID).toString();
        r.name = query.value(Constants::COL_REWARD_NAME).toString();
        r.cost = query.value(Constants::COL_REWARD_COST).toInt();
        r.cooldown = query.value(Constants::COL_REWARD_COOLDOWN).toInt();
        
        // ロールのパース
        QJsonDocument rolesDoc = QJsonDocument::fromJson(query.value(Constants::COL_REWARD_ALLOWED_ROLES).toString().toUtf8());
        if (rolesDoc.isArray()) {
            QJsonArray rolesArr = rolesDoc.array();
            for (const auto& val : rolesArr) {
                r.allowedRoles.append(val.toString());
            }
        }

        r.enabled = query.value(Constants::COL_REWARD_ENABLED).toInt() != 0;
        r.mode = query.value(Constants::COL_REWARD_MODE).toString();

        // 演出のパース
        QJsonDocument effectsDoc = QJsonDocument::fromJson(query.value(Constants::COL_REWARD_EFFECTS).toString().toUtf8());
        if (effectsDoc.isArray()) {
            QJsonArray effectsArr = effectsDoc.array();
            for (const auto& val : effectsArr) {
                r.effects.append(Effect::fromJson(val.toObject()));
            }
        }

        rewards.append(r);
    }

    return true;
}

bool Database::deleteReward(const QString& rewardId)
{
    LOG_INFO("Deleting reward from database: " + rewardId);

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM rewards WHERE id = :id");
    query.bindValue(":id", rewardId);

    if (!query.exec()) {
        LOG_ERROR("Failed to delete reward from DB: " + query.lastError().text());
        return false;
    }
    return true;
}

bool Database::logUsage(const QString& rewardId, const QString& username, const QDateTime& timestamp)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO usage_logs (reward_id, username, timestamp) VALUES (:reward_id, :username, :timestamp)");
    query.bindValue(":reward_id", rewardId);
    query.bindValue(":username", username);
    query.bindValue(":timestamp", timestamp.toString("yyyy-MM-dd hh:mm:ss"));

    if (!query.exec()) {
        LOG_ERROR("Failed to write usage log to DB: " + query.lastError().text());
        return false;
    }
    return true;
}

int Database::getTodayUsageCount()
{
    QSqlQuery query(m_db);
    // 今日の日付(ローカル時間)に合致するログ件数をカウント
    QString sql = "SELECT COUNT(*) FROM usage_logs WHERE date(timestamp) = date('now', 'localtime')";
    
    if (!query.exec(sql) || !query.next()) {
        qWarning() << "Failed to count today's usage logs:" << query.lastError().text();
        return 0;
    }
    return query.value(0).toInt();
}

QList<QPair<QString, int>> Database::getTodayRanking()
{
    QList<QPair<QString, int>> ranking;
    QSqlQuery query(m_db);
    
    QString sql = R"(
        SELECT r.name, COUNT(l.id) AS cnt 
        FROM usage_logs l 
        JOIN rewards r ON l.reward_id = r.id 
        WHERE date(l.timestamp) = date('now', 'localtime') 
        GROUP BY l.reward_id 
        ORDER BY cnt DESC
    )";

    if (!query.exec(sql)) {
        LOG_ERROR("Failed to query today's ranking: " + query.lastError().text());
        return ranking;
    }

    while (query.next()) {
        ranking.append(qMakePair(query.value(0).toString(), query.value(1).toInt()));
    }
    return ranking;
}

QList<UserUsageStat> Database::getUserUsageStatistics()
{
    QList<UserUsageStat> stats;
    QSqlQuery query(m_db);
    
    // 全期間におけるユーザごと・報酬ごとの利用回数を集計
    QString sql = R"(
        SELECT l.username, r.name, COUNT(l.id) AS cnt 
        FROM usage_logs l 
        JOIN rewards r ON l.reward_id = r.id 
        GROUP BY l.username, l.reward_id 
        ORDER BY l.username ASC, cnt DESC
    )";

    if (!query.exec(sql)) {
        LOG_ERROR("Failed to query user usage statistics: " + query.lastError().text());
        return stats;
    }

    while (query.next()) {
        stats.append({
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toInt()
        });
    }

    return stats;
}

bool Database::saveSetting(const QString& key, const QString& value)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (:key, :value)");
    query.bindValue(":key", key);
    query.bindValue(":value", value);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to save setting '%1': %2").arg(key).arg(query.lastError().text()));
        return false;
    }
    return true;
}

QString Database::getSetting(const QString& key, const QString& defaultValue) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = :key");
    query.bindValue(":key", key);

    if (!query.exec() || !query.next()) {
        return defaultValue;
    }
    return query.value(0).toString();
}
