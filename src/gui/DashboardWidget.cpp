#include "DashboardWidget.hpp"
#include "../core/Application.hpp"
#include "../core/Logger.hpp"
#include "../core/Config.hpp"
#include "../database/Database.hpp"
#include "../overlay/OverlayServer.hpp"
#include "../twitch/TwitchEventSub.hpp"
#include "../reward/QueueManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QEvent>

DashboardWidget::DashboardWidget(Application* app, QWidget* parent)
    : QWidget(parent)
    , m_app(app)
{
    setupUi();

    // シグナルの結線
    connect(m_app->overlayServer(), &OverlayServer::clientCountChanged,
            this, &DashboardWidget::updateClientCount);
            
    connect(Logger::instance(), &Logger::newLogMessage,
            this, &DashboardWidget::onNewLogMessage);

    connect(m_app->twitchEventSub(), &TwitchEventSub::connected,
            this, &DashboardWidget::refreshStats);
    connect(m_app->twitchEventSub(), &TwitchEventSub::disconnected,
            this, &DashboardWidget::refreshStats);

    refreshStats();
}

void DashboardWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 1. ステータス表示グループ
    m_infoGroup = new QGroupBox(tr("システム稼働状況"), this);
    auto* gridLayout = new QGridLayout(m_infoGroup);

    m_statusLabel = new QLabel(tr("未接続"), this);
    m_statusLabel->setStyleSheet("font-weight: bold; color: #F44336;");

    m_clientsLabel = new QLabel(tr("0 台接続中"), this);
    m_clientsLabel->setStyleSheet("font-weight: bold;");

    m_todayCountLabel = new QLabel(tr("0 回"), this);
    m_todayCountLabel->setStyleSheet("font-weight: bold; color: #2196F3;");

    m_esLabel = new QLabel("Twitch EventSub:", this);
    m_obsLabel = new QLabel("OBS オーバーレイ接続:", this);
    m_todayLabel = new QLabel("今日の総演出回数:", this);

    gridLayout->addWidget(m_esLabel, 0, 0);
    gridLayout->addWidget(m_statusLabel, 0, 1);
    gridLayout->addWidget(m_obsLabel, 1, 0);
    gridLayout->addWidget(m_clientsLabel, 1, 1);
    gridLayout->addWidget(m_todayLabel, 2, 0);
    gridLayout->addWidget(m_todayCountLabel, 2, 1);

    mainLayout->addWidget(m_infoGroup);

    // 2. コントロールボタン
    auto* btnLayout = new QHBoxLayout();
    m_toggleConnButton = new QPushButton(tr("Twitch接続を開始"), this);
    m_toggleConnButton->setStyleSheet("background-color: #6441A5; color: white; font-weight: bold; padding: 8px;");
    connect(m_toggleConnButton, &QPushButton::clicked, this, &DashboardWidget::onToggleConnection);

    m_panicButton = new QPushButton(tr("🚨 パニックボタン (緊急停止)"), this);
    m_panicButton->setStyleSheet("background-color: #D32F2F; color: white; font-weight: bold; padding: 8px;");
    connect(m_panicButton, &QPushButton::clicked, this, &DashboardWidget::onPanicClicked);

    btnLayout->addWidget(m_toggleConnButton);
    btnLayout->addWidget(m_panicButton);
    mainLayout->addLayout(btnLayout);

    // 3. リアルタイム運行ログコンソール
    m_logGroup = new QGroupBox(tr("運行ログ（リアルタイム）"), this);
    auto* logLayout = new QVBoxLayout(m_logGroup);

    m_logListWidget = new QListWidget(this);
    m_logListWidget->setStyleSheet("background-color: #1E1E1E; color: #D4D4D4; font-family: 'Consolas', monospace; font-size: 11px;");
    logLayout->addWidget(m_logListWidget);

    mainLayout->addWidget(m_logGroup);

    retranslateUi();
}

void DashboardWidget::refreshStats()
{
    // 今日の演出カウント数の表示
    if (m_app->database()) {
        int count = m_app->database()->getTodayUsageCount();
        m_todayCountLabel->setText(QString(tr("%1 回")).arg(count));
    }

    // Twitch WebSocket の接続状態表示
    if (m_app->twitchEventSub()->isConnected()) {
        m_statusLabel->setText(tr("接続中 (配信イベント監視中)"));
        m_statusLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
        m_toggleConnButton->setText(tr("Twitch接続を切断"));
    } else {
        m_statusLabel->setText(tr("未接続"));
        m_statusLabel->setStyleSheet("font-weight: bold; color: #F44336;");
        m_toggleConnButton->setText(tr("Twitch接続を開始"));
    }
}

void DashboardWidget::onToggleConnection()
{
    if (m_app->twitchEventSub()->isConnected()) {
        m_app->twitchEventSub()->disconnectFromServer();
    } else {
        QString secretKey = "twitch_overlay_secret_key_2026";
        QString accessToken = m_app->config()->loadSecureString("twitch_access_token", secretKey);
        QString clientId = m_app->config()->get("twitch_client_id", TWITCH_GLOBAL_CLIENT_ID).toString();
        QString broadcasterId = m_app->config()->get("twitch_broadcaster_id").toString();

        if (accessToken.isEmpty() || clientId.isEmpty() || broadcasterId.isEmpty()) {
            LOG_WARN("Cannot connect to Twitch EventSub: Missing OAuth settings.");
            onNewLogMessage(LogLevel::Warning, tr("[警告] 接続に必要な認証情報がありません。設定タブから認証を行ってください。"));
            return;
        }

        m_app->twitchEventSub()->connectToServer(accessToken, clientId, broadcasterId);
    }
    
    QMetaObject::invokeMethod(this, "refreshStats", Qt::QueuedConnection);
}

void DashboardWidget::onPanicClicked()
{
    m_app->queueManager()->stopAllEffects();
    onNewLogMessage(LogLevel::Warning, tr("[🚨システム] パニック停止が呼び出されました。現在のキューおよびOBS表示を一括消去しました。"));
    refreshStats();
}

void DashboardWidget::onNewLogMessage(LogLevel level, const QString& message)
{
    m_logListWidget->addItem(message);
    while (m_logListWidget->count() > 300) {
        delete m_logListWidget->takeItem(0);
    }
    m_logListWidget->scrollToBottom();

    if (message.contains("Channel Point Redeemed") || message.contains("Completed all effects")) {
        refreshStats();
    }
}

void DashboardWidget::updateClientCount(int count)
{
    m_clientsLabel->setText(QString(tr("%1 台接続中")).arg(count));
}

void DashboardWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void DashboardWidget::retranslateUi()
{
    m_infoGroup->setTitle(tr("システム稼働状況"));
    m_esLabel->setText(tr("Twitch EventSub:"));
    m_obsLabel->setText(tr("OBS オーバーレイ接続:"));
    m_todayLabel->setText(tr("今日の総演出回数:"));

    m_logGroup->setTitle(tr("運行ログ（リアルタイム）"));
    m_panicButton->setText(tr("🚨 パニックボタン (緊急停止)"));

    // 動的な文言のリフレッシュ
    refreshStats();
}
