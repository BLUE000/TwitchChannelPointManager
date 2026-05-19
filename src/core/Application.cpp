#include "Application.hpp"
#include "Logger.hpp"
#include "Config.hpp"
#include "../database/Database.hpp"
#include "../reward/RewardManager.hpp"
#include "../reward/QueueManager.hpp"
#include "../overlay/FileManager.hpp"
#include "../overlay/OverlayServer.hpp"
#include "../twitch/TwitchAuth.hpp"
#include "../twitch/TwitchEventSub.hpp"

Application::Application(QObject* parent)
    : QObject(parent)
    , m_isInitialized(false)
{
}

Application::~Application()
{
    shutdown();
}

bool Application::initialize(const QString& dbPath, const QString& configPath)
{
    if (m_isInitialized) return true;

    // 1. シングルトンロガーの初期化
    Logger::instance()->initialize("twitch_overlay.log");
    LOG_INFO("====================================================================");
    LOG_INFO("         Twitch Channel Point Overlay Manager - Starting Core       ");
    LOG_INFO("====================================================================");

    // 2. データベース接続の初期化
    m_database = std::make_unique<Database>(this);
    if (!m_database->open(dbPath)) {
        LOG_ERROR("Fatal: Database initialization failed.");
        return false;
    }

    // 3. 設定マネージャーの初期化
    m_config = std::make_unique<Config>(configPath, this);
    m_config->load(); // 初回起動時などでファイルが無ければ空ロード

    // 4. 報酬キャッシュ・バリデーションマネージャーの初期化
    m_rewardManager = std::make_unique<RewardManager>(m_database.get(), this);
    m_rewardManager->loadAllRewards();

    // 5. 演出再生キューの初期化
    m_queueManager = std::make_unique<QueueManager>(m_database.get(), this);

    // 6. アセットファイルマネージャーの初期化
    // デフォルトポートで初期作成（後ほどDB値があればロード）
    m_fileManager = std::make_unique<FileManager>(28081, this);

    // 7. OBS連携 WebSocket & HTTP サーバーの初期化
    m_overlayServer = std::make_unique<OverlayServer>(m_fileManager.get(), this);

    // 8. DBから設定ポート等を読み込んで各種サーバーを起動
    loadSettingsAndBootOverlay();

    // 9. Twitch 認証 & EventSub 受信機の初期化
    // シークレット情報はローカル設定、または暗号化ストレージから読み出し
    QString secretKey = "twitch_overlay_secret_key_2026";
    QString clientId = m_config->get("twitch_client_id", "MOCK_CLIENT_ID_XYZ123").toString();
    QString encryptedSecret = m_config->get("twitch_client_secret", "").toString();
    QString decryptedSecret = "";
    
    if (!encryptedSecret.isEmpty()) {
        decryptedSecret = m_config->loadSecureString("twitch_client_secret", secretKey);
    } else {
        decryptedSecret = "MOCK_CLIENT_SECRET_ABC789";
    }

    m_twitchAuth = std::make_unique<TwitchAuth>(clientId, decryptedSecret, 28082, this);
    m_twitchEventSub = std::make_unique<TwitchEventSub>(this);

    // 10. 全マネージャー間のシグナル・スロット接続の確立
    setupSignalConnections();

    m_isInitialized = true;
    LOG_INFO("Core Application system successfully initialized and online.");
    return true;
}

void Application::shutdown()
{
    if (!m_isInitialized) return;

    LOG_INFO("Shutting down C++ core application services.");
    
    if (m_twitchEventSub) m_twitchEventSub->disconnectFromServer();
    if (m_overlayServer) m_overlayServer->stop();
    if (m_database) m_database->close();
    
    Logger::instance()->shutdown();
    m_isInitialized = false;
}

void Application::loadSettingsAndBootOverlay()
{
    // DBまたはConfigからポート等の設定を解決
    int wsPort = m_database->getSetting("websocket_port", "28080").toInt();
    int httpPort = m_database->getSetting("asset_server_port", "28081").toInt();

    LOG_INFO(QString("Loading network ports from settings: WS=%1, HTTP=%2").arg(wsPort).arg(httpPort));

    // 各種サーバーの起動
    if (m_overlayServer->start(wsPort, httpPort)) {
        LOG_INFO("Overlay Communication Server successfully boot-up.");
    } else {
        LOG_ERROR("Overlay Communication Server boot failed!");
    }
}

void Application::setupSignalConnections()
{
    // 1. Twitch イベント受信 -> アプリケーション側でチェックしてキューへ振分け
    connect(m_twitchEventSub.get(), &TwitchEventSub::channelPointRedeemed,
            this, &Application::onTwitchPointRedeemed);

    // 2. キューマネージャー -> OverlayServer (演出再生要求)
    connect(m_queueManager.get(), &QueueManager::playEffectRequested,
            m_overlayServer.get(), &OverlayServer::sendEffect);

    // 3. キューマネージャー -> OverlayServer (一斉停止・全消去の一斉配信)
    connect(m_queueManager.get(), &QueueManager::stopAllRequested,
            m_overlayServer.get(), &OverlayServer::broadcastStopAll);
    connect(m_queueManager.get(), &QueueManager::clearQueueRequested,
            m_overlayServer.get(), &OverlayServer::broadcastClearQueue);

    // 4. OverlayServer -> キューマネージャー (OBS側の描画完了コールバック接続)
    connect(m_overlayServer.get(), &OverlayServer::effectFinished,
            m_queueManager.get(), &QueueManager::onEffectCompleted);

    // 5. Twitch 認証成功 -> 暗号化して設定ファイルに自動保管するセキュア処理
    connect(m_twitchAuth.get(), &TwitchAuth::authSuccess, [this](const QString& access, const QString& refresh) {
        LOG_INFO("Received successful OAuth token exchange. Saving securely using TransCipher-Dist...");
        QString secretKey = "twitch_overlay_secret_key_2026";
        
        m_config->saveSecureString("twitch_access_token", access, secretKey);
        m_config->saveSecureString("twitch_refresh_token", refresh, secretKey);
        m_config->save(); // ファイル書き込み保存

        LOG_INFO("Credentials stored safely. Automatically establishing EventSub WebSocket connection.");
        
        // トークン情報を用いてEventSubへ自動接続
        QString clientId = m_config->get("twitch_client_id", "MOCK_CLIENT_ID_XYZ123").toString();
        QString broadcasterId = m_config->get("twitch_broadcaster_id", "MOCK_BROADCASTER_ID_777").toString();
        
        m_twitchEventSub->connectToServer(access, clientId, broadcasterId);
    });
}

void Application::onTwitchPointRedeemed(const QString& rewardId, const QString& username, const QDateTime& timestamp)
{
    QString rejectReason;
    
    // クールダウン中や無効化などのチェックを実施
    if (m_rewardManager->validateRedemption(rewardId, username, rejectReason)) {
        LOG_INFO(QString("Accepting custom reward redemption: %1 for user %2").arg(rewardId).arg(username));
        
        // 使用可能であればクールタイムをスタートさせ、キューに投入
        m_rewardManager->triggerCooldown(rewardId);
        m_queueManager->enqueueRedemption(rewardId, username, timestamp);
    } else {
        LOG_WARN(QString("Rejecting redemption for reward '%1' from user '%2'. Reason: %3")
            .arg(rewardId).arg(username).arg(rejectReason));
    }
}
