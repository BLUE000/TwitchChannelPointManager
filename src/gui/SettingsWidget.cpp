#include "SettingsWidget.hpp"
#include "../core/Application.hpp"
#include "../core/Config.hpp"
#include "../database/Database.hpp"
#include "../twitch/TwitchAuth.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

SettingsWidget::SettingsWidget(Application* app, QWidget* parent)
    : QWidget(parent)
    , m_app(app)
{
    setupUi();
    loadCurrentSettings();
}

void SettingsWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 1. 接続ポートの設定
    auto* portGroup = new QGroupBox("ネットワークポート設定", this);
    auto* portLayout = new QFormLayout(portGroup);

    m_wsPortSpin = new QSpinBox(this);
    m_wsPortSpin->setRange(1024, 65535);
    m_wsPortSpin->setValue(28080);
    portLayout->addRow("WebSocket サーバーポート:", m_wsPortSpin);

    m_httpPortSpin = new QSpinBox(this);
    m_httpPortSpin->setRange(1024, 65535);
    m_httpPortSpin->setValue(28081);
    portLayout->addRow("HTTP アセット配信ポート:", m_httpPortSpin);

    m_savePortsBtn = new QPushButton("ポート設定を保存", this);
    m_savePortsBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    connect(m_savePortsBtn, &QPushButton::clicked, this, &SettingsWidget::onSavePortsClicked);
    portLayout->addRow(m_savePortsBtn);

    mainLayout->addWidget(portGroup);

    // 2. Twitch OAuth連携の設定
    auto* twitchGroup = new QGroupBox("Twitch 連携認証設定", this);
    auto* twitchLayout = new QFormLayout(twitchGroup);

    m_clientIdEdit = new QLineEdit(this);
    m_clientIdEdit->setPlaceholderText("Twitch Developer Consoleの Client ID を入力");
    twitchLayout->addRow("Client ID:", m_clientIdEdit);

    m_clientSecretEdit = new QLineEdit(this);
    m_clientSecretEdit->setEchoMode(QLineEdit::Password);
    m_clientSecretEdit->setPlaceholderText("Twitch Developer Consoleの Client Secret を入力");
    twitchLayout->addRow("Client Secret:", m_clientSecretEdit);

    m_broadcasterIdEdit = new QLineEdit(this);
    m_broadcasterIdEdit->setPlaceholderText("あなたの Twitch Broadcaster User ID を入力");
    twitchLayout->addRow("Broadcaster ID:", m_broadcasterIdEdit);

    m_authBtn = new QPushButton("Twitch アプリ認可（OAuth）を開始する", this);
    m_authBtn->setStyleSheet("background-color: #6441A5; color: white; font-weight: bold; padding: 8px;");
    connect(m_authBtn, &QPushButton::clicked, this, &SettingsWidget::onAuthClicked);
    twitchLayout->addRow(m_authBtn);

    mainLayout->addWidget(twitchGroup);

    mainLayout->addStretch();
}

void SettingsWidget::loadCurrentSettings()
{
    // ポート設定ロード
    if (m_app->database()) {
        int wsPort = m_app->database()->getSetting("websocket_port", "28080").toInt();
        int httpPort = m_app->database()->getSetting("asset_server_port", "28081").toInt();
        m_wsPortSpin->setValue(wsPort);
        m_httpPortSpin->setValue(httpPort);
    }

    // OAuth情報ロード
    QString secretKey = "twitch_overlay_secret_key_2026";
    QString clientId = m_app->config()->get("twitch_client_id").toString();
    QString broadcasterId = m_app->config()->get("twitch_broadcaster_id").toString();
    
    // 暗号化されているSecretを復号して表示
    QString encryptedSecret = m_app->config()->get("twitch_client_secret").toString();
    QString decryptedSecret = "";
    if (!encryptedSecret.isEmpty()) {
        decryptedSecret = m_app->config()->loadSecureString("twitch_client_secret", secretKey);
    }

    m_clientIdEdit->setText(clientId);
    m_clientSecretEdit->setText(decryptedSecret);
    m_broadcasterIdEdit->setText(broadcasterId);
}

void SettingsWidget::onSavePortsClicked()
{
    if (!m_app->database()) return;

    QString wsPort = QString::number(m_wsPortSpin->value());
    QString httpPort = QString::number(m_httpPortSpin->value());

    m_app->database()->saveSetting("websocket_port", wsPort);
    m_app->database()->saveSetting("asset_server_port", httpPort);

    QMessageBox::information(this, "成功", "ポート設定を保存しました。設定の反映にはアプリの再起動が必要です。");
}

void SettingsWidget::onAuthClicked()
{
    QString clientId = m_clientIdEdit->text().trimmed();
    QString clientSecret = m_clientSecretEdit->text().trimmed();
    QString broadcasterId = m_broadcasterIdEdit->text().trimmed();

    if (clientId.isEmpty() || clientSecret.isEmpty() || broadcasterId.isEmpty()) {
        QMessageBox::warning(this, "入力エラー", "認可を開始するには、すべての項目を入力してください。");
        return;
    }

    // 設定情報の設定と保存
    QString secretKey = "twitch_overlay_secret_key_2026";
    m_app->config()->set("twitch_client_id", clientId);
    m_app->config()->set("twitch_broadcaster_id", broadcasterId);
    
    // クライアントシークレットを強力に暗号化して保存
    m_app->config()->saveSecureString("twitch_client_secret", clientSecret, secretKey);
    m_app->config()->save();

    // TwitchAuth に最新のクライアント情報を登録
    // setCredentials helper or similar through direct modification
    // Let's implement dynamic auth flow
    QMessageBox::information(this, "認証開始", "これより外部ブラウザを開いて Twitch 連携認可を開始します。");
    
    // 認可フロー起動
    m_app->twitchAuth()->startAuthFlow();
}
