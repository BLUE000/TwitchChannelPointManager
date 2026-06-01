#pragma once

#include <QObject>
#include <memory>
#include <QString>

#include <QTranslator>

class Database;
class Config;
class RewardManager;
class QueueManager;
class FileManager;
class OverlayServer;
class TwitchAuth;
class TwitchEventSub;
class QTimer;

class Application : public QObject {
    Q_OBJECT
private:
    std::unique_ptr<Database> m_database;
    std::unique_ptr<Config> m_config;
    std::unique_ptr<RewardManager> m_rewardManager;
    std::unique_ptr<QueueManager> m_queueManager;
    std::unique_ptr<FileManager> m_fileManager;
    std::unique_ptr<OverlayServer> m_overlayServer;
    
    std::unique_ptr<TwitchAuth> m_twitchAuth;
    std::unique_ptr<TwitchEventSub> m_twitchEventSub;

    QTranslator m_translator; // 動的翻訳用のロードメンバ

    QTimer* m_tokenRefreshTimer;
    QTimer* m_retryTimer;
    int m_retryCount;

    bool m_isInitialized;

public:
    explicit Application(QObject* parent = nullptr);
    ~Application();

    // システム全体のセットアップ
    bool initialize(const QString& dbPath = "data.db", const QString& configPath = "config.json");
    
    // 動的にUI言語をロード・切り替える
    bool loadLanguage(const QString& langCode);
    
    // システムの正常終了
    void shutdown();

    // 各主要モジュールへのアクセサー
    Database* database() const { return m_database.get(); }
    Config* config() const { return m_config.get(); }
    RewardManager* rewardManager() const { return m_rewardManager.get(); }
    QueueManager* queueManager() const { return m_queueManager.get(); }
    OverlayServer* overlayServer() const { return m_overlayServer.get(); }
    TwitchAuth* twitchAuth() const { return m_twitchAuth.get(); }
    TwitchEventSub* twitchEventSub() const { return m_twitchEventSub.get(); }

signals:
    void authFailedFatal(const QString& errorMessage);

private slots:
    // Twitch のイベント受信時のディスパッチスロット
    void onTwitchPointRedeemed(const QString& rewardId, const QString& username, const QDateTime& timestamp);
    void onTwitchTokenExpired();
    void onPeriodicTokenRefreshTriggered();
    void onTwitchAuthSuccess(const QString& access, const QString& refresh, const QString& broadcasterId);
    void onTwitchAuthFailed(const QString& errorMessage, bool isFatal);

private:
    void setupSignalConnections();
    void loadSettingsAndBootOverlay();
};
