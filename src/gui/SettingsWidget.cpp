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
#include <QCheckBox>
#include <QFileDialog>
#include <QDir>
#include <QLabel>

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

    // 1b. 外部スクリプト設定
    m_enableScriptIntegrationCb = new QCheckBox("外部スクリプト連携機能 (Perl/PHP) を有効にする", this);
    m_enableScriptIntegrationCb->setStyleSheet("margin-top: 10px; font-weight: bold;");
    mainLayout->addWidget(m_enableScriptIntegrationCb);

    m_scriptGroup = new QGroupBox("外部スクリプト連携詳細設定", this);
    auto* scriptLayout = new QFormLayout(m_scriptGroup);

    // 免責・警告メッセージ
    auto* disclaimerLabel = new QLabel(
        "⚠️ <b>【重要】外部スクリプトに関する警告と免責事項</b><br/>"
        "第三者が作成したスクリプトや中身のわからないプログラムを実行する場合は、十分に注意してください。<br/>"
        "第三者作成のスクリプトによる<b>いかなる損害（データ消失・漏洩・システム破壊等）に関しても、本ツールの作者は一切の責任および保証を行えません。</b><br/>"
        "必ず実行されるコードの安全性を一行ずつ手動で確認した上で、<b>すべて自己責任</b>においてご利用ください。",
        this
    );
    disclaimerLabel->setWordWrap(true);
    disclaimerLabel->setStyleSheet(R"(
        QLabel {
            color: #FF5555;
            background-color: #2A1C1C;
            border: 1px solid #5A2A2A;
            border-radius: 4px;
            padding: 10px;
            font-size: 12px;
            line-height: 1.4;
            margin-bottom: 10px;
        }
    )");
    scriptLayout->addRow(disclaimerLabel);

    // PHP パス
    auto* phpLayout = new QHBoxLayout();
    m_phpPathEdit = new QLineEdit(this);
    m_phpPathEdit->setPlaceholderText("例: C:/php/php.exe");
    auto* phpBrowseBtn = new QPushButton("参照...", this);
    phpLayout->addWidget(m_phpPathEdit);
    phpLayout->addWidget(phpBrowseBtn);
    scriptLayout->addRow("PHP 実行ファイルパス:", phpLayout);
    connect(phpBrowseBtn, &QPushButton::clicked, this, &SettingsWidget::onBrowsePhpPath);

    // Perl パス
    auto* perlLayout = new QHBoxLayout();
    m_perlPathEdit = new QLineEdit(this);
    m_perlPathEdit->setPlaceholderText("例: C:/strawberry/perl/bin/perl.exe");
    auto* perlBrowseBtn = new QPushButton("参照...", this);
    perlLayout->addWidget(m_perlPathEdit);
    perlLayout->addWidget(perlBrowseBtn);
    scriptLayout->addRow("Perl 実行ファイルパス:", perlLayout);
    connect(perlBrowseBtn, &QPushButton::clicked, this, &SettingsWidget::onBrowsePerlPath);

    m_saveScriptBtn = new QPushButton("スクリプト設定を保存", this);
    m_saveScriptBtn->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold;");
    connect(m_saveScriptBtn, &QPushButton::clicked, this, &SettingsWidget::onSaveScriptClicked);
    scriptLayout->addRow(m_saveScriptBtn);

    m_scriptGroup->setVisible(false);
    connect(m_enableScriptIntegrationCb, &QCheckBox::toggled, m_scriptGroup, &QGroupBox::setVisible);
    connect(m_enableScriptIntegrationCb, &QCheckBox::toggled, this, &SettingsWidget::autoSaveScriptSettings);
    connect(m_phpPathEdit, &QLineEdit::textChanged, this, &SettingsWidget::autoSaveScriptSettings);
    connect(m_perlPathEdit, &QLineEdit::textChanged, this, &SettingsWidget::autoSaveScriptSettings);

    mainLayout->addWidget(m_scriptGroup);

    // 2. Twitch OAuth連携の設定
    auto* twitchGroup = new QGroupBox("Twitch 連携認証設定", this);
    auto* twitchLayout = new QVBoxLayout(twitchGroup);

    // 連携開始ボタン（通常時はこれだけ見えればOK！）
    m_authBtn = new QPushButton("Twitch アカウント連携（OAuth）を開始する", this);
    m_authBtn->setStyleSheet("background-color: #6441A5; color: white; font-weight: bold; padding: 12px; font-size: 14px; border-radius: 4px;");
    connect(m_authBtn, &QPushButton::clicked, this, &SettingsWidget::onAuthClicked);
    twitchLayout->addWidget(m_authBtn);

    // カスタム設定チェックボックス
    m_useCustomCredentialsCb = new QCheckBox("カスタムの認可情報を使用する（開発者・パワーユーザー向け）", this);
    m_useCustomCredentialsCb->setStyleSheet("margin-top: 10px; color: #aaa;");
    twitchLayout->addWidget(m_useCustomCredentialsCb);

    // 詳細設定用コンテナグループボックス
    m_customCredentialsGroup = new QGroupBox("詳細開発者設定", this);
    auto* customLayout = new QFormLayout(m_customCredentialsGroup);

    m_clientIdEdit = new QLineEdit(this);
    m_clientIdEdit->setPlaceholderText("Twitch Developer Consoleの Client ID を入力");
    customLayout->addRow("Client ID:", m_clientIdEdit);

    m_clientSecretEdit = new QLineEdit(this);
    m_clientSecretEdit->setEchoMode(QLineEdit::Password);
    m_clientSecretEdit->setPlaceholderText("Twitch Developer Console of Client Secret を入力");
    customLayout->addRow("Client Secret:", m_clientSecretEdit);

    m_broadcasterIdEdit = new QLineEdit(this);
    m_broadcasterIdEdit->setPlaceholderText("あなたの Twitch Broadcaster User ID を入力（通常は自動取得されます）");
    customLayout->addRow("Broadcaster ID:", m_broadcasterIdEdit);

    twitchLayout->addWidget(m_customCredentialsGroup);

    // チェックボックスのトグルで詳細設定を出し入れする
    m_customCredentialsGroup->setVisible(false);
    connect(m_useCustomCredentialsCb, &QCheckBox::toggled, m_customCredentialsGroup, &QGroupBox::setVisible);

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

        m_enableScriptIntegrationCb->blockSignals(true);
        m_phpPathEdit->blockSignals(true);
        m_perlPathEdit->blockSignals(true);

        QString enabled = m_app->database()->getSetting("script_integration_enabled", "0");
        m_enableScriptIntegrationCb->setChecked(enabled == "1");
        m_scriptGroup->setVisible(enabled == "1");

        QString phpPath = m_app->database()->getSetting("php_interpreter_path", "");
        QString perlPath = m_app->database()->getSetting("perl_interpreter_path", "");
        m_phpPathEdit->setText(QDir::toNativeSeparators(phpPath));
        m_perlPathEdit->setText(QDir::toNativeSeparators(perlPath));

        m_enableScriptIntegrationCb->blockSignals(false);
        m_phpPathEdit->blockSignals(false);
        m_perlPathEdit->blockSignals(false);
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

    // カスタム設定がある場合はチェックボックスをONにし、無ければOFF（非表示）にする
    // ※ グローバル（デフォルト）の認証情報と同一である場合は、カスタム設定としては扱わない
    if (!clientId.isEmpty() && clientId != TWITCH_GLOBAL_CLIENT_ID) {
        m_useCustomCredentialsCb->setChecked(true);
        m_customCredentialsGroup->setVisible(true);
    } else {
        m_useCustomCredentialsCb->setChecked(false);
        m_customCredentialsGroup->setVisible(false);
        m_clientIdEdit->clear();
        m_clientSecretEdit->clear();
    }
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
    QString secretKey = "twitch_overlay_secret_key_2026";

    if (m_useCustomCredentialsCb->isChecked()) {
        // カスタム設定がONの場合は入力を検証
        QString clientId = m_clientIdEdit->text().trimmed();
        QString clientSecret = m_clientSecretEdit->text().trimmed();
        QString broadcasterId = m_broadcasterIdEdit->text().trimmed();

        if (clientId.isEmpty() || clientSecret.isEmpty()) {
            QMessageBox::warning(this, "入力エラー", "カスタム設定を使用する場合、Client ID と Client Secret は必須です。");
            return;
        }

        // カスタム設定をConfigへ保存
        m_app->config()->set("twitch_client_id", clientId);
        m_app->config()->saveSecureString("twitch_client_secret", clientSecret, secretKey);
        if (!broadcasterId.isEmpty()) {
            m_app->config()->set("twitch_broadcaster_id", broadcasterId);
        } else {
            m_app->config()->remove("twitch_broadcaster_id");
        }
        m_app->config()->save();

        // 実行中の TwitchAuth オブジェクトにカスタム認証情報をセット
        m_app->twitchAuth()->setCredentials(clientId, clientSecret);
    } else {
        // 通常（全自動）連携の場合はローカルのカスタム設定をクリアし、ビルド時定数へフォールバック
        m_app->config()->remove("twitch_client_id");
        m_app->config()->remove("twitch_client_secret");
        // ※ broadcasterId は認可フローの中で自動取得され、自動的に保存されます！
        m_app->config()->save();

        // 実行中の TwitchAuth オブジェクトにグローバル共通認証情報をセット
        m_app->twitchAuth()->setCredentials(TWITCH_GLOBAL_CLIENT_ID, TWITCH_GLOBAL_CLIENT_SECRET);
    }

    QMessageBox::information(this, "認証開始", "これより外部ブラウザを開いて Twitch 連携認可を開始します。");
    
    // 認可フロー起動
    m_app->twitchAuth()->startAuthFlow();
}

void SettingsWidget::onBrowsePhpPath()
{
    QString path = QFileDialog::getOpenFileName(this, "PHP実行ファイル (php.exe) を選択", "", "Executables (*.exe);;All Files (*)");
    if (!path.isEmpty()) {
        m_phpPathEdit->setText(QDir::toNativeSeparators(path));
    }
}

void SettingsWidget::onBrowsePerlPath()
{
    QString path = QFileDialog::getOpenFileName(this, "Perl実行ファイル (perl.exe) を選択", "", "Executables (*.exe);;All Files (*)");
    if (!path.isEmpty()) {
        m_perlPathEdit->setText(QDir::toNativeSeparators(path));
    }
}

void SettingsWidget::onSaveScriptClicked()
{
    autoSaveScriptSettings();
    QMessageBox::information(this, "成功", "外部スクリプトの連携設定を保存しました。");
}

void SettingsWidget::autoSaveScriptSettings()
{
    if (!m_app->database()) return;

    QString enabled = m_enableScriptIntegrationCb->isChecked() ? "1" : "0";
    QString phpPath = m_phpPathEdit->text().trimmed();
    QString perlPath = m_perlPathEdit->text().trimmed();

    m_app->database()->saveSetting("script_integration_enabled", enabled);
    m_app->database()->saveSetting("php_interpreter_path", phpPath);
    m_app->database()->saveSetting("perl_interpreter_path", perlPath);
}
