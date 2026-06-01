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
#include <QEvent>

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

    // 1. 表示言語の設定
    m_langGroup = new QGroupBox(tr("表示言語設定"), this);
    auto* langLayout = new QFormLayout(m_langGroup);

    m_languageCombo = new QComboBox(this);
    // ユーザー指定の昇順ソート：Deutsch, English, Español, Français, Português, 日本語
    // 0番目にはシステムデフォルトを設定
    m_languageCombo->addItem(tr("システムデフォルト (System Default)"), "auto");
    m_languageCombo->addItem("Deutsch", "de");
    m_languageCombo->addItem("English", "en");
    m_languageCombo->addItem("Español", "es");
    m_languageCombo->addItem("Français", "fr");
    m_languageCombo->addItem("Português", "pt");
    m_languageCombo->addItem("日本語", "ja");
    m_languageCombo->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    langLayout->addRow(tr("UI表示言語:"), m_languageCombo);

    m_saveLangBtn = new QPushButton(tr("言語設定を保存"), this);
    m_saveLangBtn->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 6px 12px;");
    connect(m_saveLangBtn, &QPushButton::clicked, this, &SettingsWidget::onSaveLanguageClicked);
    langLayout->addRow(m_saveLangBtn);

    mainLayout->addWidget(m_langGroup);

    // 2. 接続ポートの設定
    m_portGroup = new QGroupBox(tr("ネットワークポート設定"), this);
    auto* portLayout = new QFormLayout(m_portGroup);

    m_wsPortSpin = new QSpinBox(this);
    m_wsPortSpin->setRange(1024, 65535);
    m_wsPortSpin->setValue(28080);
    m_wsPortSpin->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    portLayout->addRow(tr("WebSocket サーバーポート:"), m_wsPortSpin);

    m_httpPortSpin = new QSpinBox(this);
    m_httpPortSpin->setRange(1024, 65535);
    m_httpPortSpin->setValue(28081);
    m_httpPortSpin->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    portLayout->addRow(tr("HTTP アセット配信ポート:"), m_httpPortSpin);

    m_savePortsBtn = new QPushButton(tr("ポート設定を保存"), this);
    m_savePortsBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 6px 12px;");
    connect(m_savePortsBtn, &QPushButton::clicked, this, &SettingsWidget::onSavePortsClicked);
    portLayout->addRow(m_savePortsBtn);

    mainLayout->addWidget(m_portGroup);

    // 3. Twitch OAuth連携の設定
    m_twitchGroup = new QGroupBox(tr("Twitch 連携認証設定"), this);
    auto* twitchLayout = new QVBoxLayout(m_twitchGroup);

    m_authBtn = new QPushButton(tr("Twitch アカウント連携（OAuth）を開始する"), this);
    m_authBtn->setStyleSheet("background-color: #6441A5; color: white; font-weight: bold; padding: 12px; font-size: 14px; border-radius: 4px;");
    connect(m_authBtn, &QPushButton::clicked, this, &SettingsWidget::onAuthClicked);
    twitchLayout->addWidget(m_authBtn);

    m_useCustomCredentialsCb = new QCheckBox(tr("カスタムの認可情報を使用する（開発者・パワーユーザー向け）"), this);
    m_useCustomCredentialsCb->setStyleSheet("margin-top: 10px; color: #aaa;");
    twitchLayout->addWidget(m_useCustomCredentialsCb);

    m_customCredentialsGroup = new QGroupBox(tr("詳細開発者設定"), this);
    auto* customLayout = new QFormLayout(m_customCredentialsGroup);

    m_clientIdEdit = new QLineEdit(this);
    m_clientIdEdit->setPlaceholderText(tr("Twitch Developer Consoleの Client ID を入力"));
    m_clientIdEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    customLayout->addRow("Client ID:", m_clientIdEdit);

    m_clientSecretEdit = new QLineEdit(this);
    m_clientSecretEdit->setEchoMode(QLineEdit::Password);
    m_clientSecretEdit->setPlaceholderText(tr("Twitch Developer Console of Client Secret を入力"));
    m_clientSecretEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    customLayout->addRow("Client Secret:", m_clientSecretEdit);

    m_broadcasterIdEdit = new QLineEdit(this);
    m_broadcasterIdEdit->setPlaceholderText(tr("あなたの Twitch Broadcaster User ID を入力（通常は自動取得されます）"));
    m_broadcasterIdEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    customLayout->addRow("Broadcaster ID:", m_broadcasterIdEdit);

    twitchLayout->addWidget(m_customCredentialsGroup);

    m_customCredentialsGroup->setVisible(false);
    connect(m_useCustomCredentialsCb, &QCheckBox::toggled, m_customCredentialsGroup, &QGroupBox::setVisible);

    mainLayout->addWidget(m_twitchGroup);
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

    // 表示言語設定ロード
    QString uiLang = m_app->config()->get("ui_language", "auto").toString();
    int langIdx = m_languageCombo->findData(uiLang);
    if (langIdx != -1) {
        m_languageCombo->setCurrentIndex(langIdx);
    } else {
        m_languageCombo->setCurrentIndex(0); // auto
    }

    // OAuth情報ロード
    QString secretKey = "twitch_overlay_secret_key_2026";
    QString clientId = m_app->config()->get("twitch_client_id").toString();
    QString broadcasterId = m_app->config()->get("twitch_broadcaster_id").toString();
    
    QString encryptedSecret = m_app->config()->get("twitch_client_secret").toString();
    QString decryptedSecret = "";
    if (!encryptedSecret.isEmpty()) {
        decryptedSecret = m_app->config()->loadSecureString("twitch_client_secret", secretKey);
    }

    m_clientIdEdit->setText(clientId);
    m_clientSecretEdit->setText(decryptedSecret);
    m_broadcasterIdEdit->setText(broadcasterId);

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

void SettingsWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SettingsWidget::retranslateUi()
{
    m_langGroup->setTitle(tr("表示言語設定"));
    auto* langLayout = qobject_cast<QFormLayout*>(m_langGroup->layout());
    if (langLayout) {
        auto* label = qobject_cast<QLabel*>(langLayout->labelForField(m_languageCombo));
        if (label) label->setText(tr("UI表示言語:"));
    }
    m_languageCombo->setItemText(0, tr("システムデフォルト (System Default)"));
    m_saveLangBtn->setText(tr("言語設定を保存"));

    m_portGroup->setTitle(tr("ネットワークポート設定"));
    auto* portLayout = qobject_cast<QFormLayout*>(m_portGroup->layout());
    if (portLayout) {
        auto* wsLabel = qobject_cast<QLabel*>(portLayout->labelForField(m_wsPortSpin));
        if (wsLabel) wsLabel->setText(tr("WebSocket サーバーポート:"));
        auto* httpLabel = qobject_cast<QLabel*>(portLayout->labelForField(m_httpPortSpin));
        if (httpLabel) httpLabel->setText(tr("HTTP アセット配信ポート:"));
    }
    m_savePortsBtn->setText(tr("ポート設定を保存"));

    m_twitchGroup->setTitle(tr("Twitch 連携認証設定"));
    m_authBtn->setText(tr("Twitch アカウント連携（OAuth）を開始する"));
    m_useCustomCredentialsCb->setText(tr("カスタムの認可情報を使用する（開発者・パワーユーザー向け）"));
    m_customCredentialsGroup->setTitle(tr("詳細開発者設定"));
    
    auto* customLayout = qobject_cast<QFormLayout*>(m_customCredentialsGroup->layout());
    if (customLayout) {
        auto* clientLabel = qobject_cast<QLabel*>(customLayout->labelForField(m_clientIdEdit));
        if (clientLabel) clientLabel->setText("Client ID:");
        auto* secretLabel = qobject_cast<QLabel*>(customLayout->labelForField(m_clientSecretEdit));
        if (secretLabel) secretLabel->setText("Client Secret:");
        auto* broadcasterLabel = qobject_cast<QLabel*>(customLayout->labelForField(m_broadcasterIdEdit));
        if (broadcasterLabel) broadcasterLabel->setText("Broadcaster ID:");
    }
    m_clientIdEdit->setPlaceholderText(tr("Twitch Developer Consoleの Client ID を入力"));
    m_clientSecretEdit->setPlaceholderText(tr("Twitch Developer Console of Client Secret を入力"));
    m_broadcasterIdEdit->setPlaceholderText(tr("あなたの Twitch Broadcaster User ID を入力（通常は自動取得されます）"));
}

void SettingsWidget::onSaveLanguageClicked()
{
    QString langCode = m_languageCombo->currentData().toString();
    m_app->config()->set("ui_language", langCode);
    m_app->config()->save();

    // 動的に言語を切り替える (再起動不要！)
    m_app->loadLanguage(langCode);

    QMessageBox::information(this, tr("成功"), tr("言語設定を保存し、表示言語を切り替えました。"));
}

void SettingsWidget::onSavePortsClicked()
{
    if (!m_app->database()) return;

    QString wsPort = QString::number(m_wsPortSpin->value());
    QString httpPort = QString::number(m_httpPortSpin->value());

    m_app->database()->saveSetting("websocket_port", wsPort);
    m_app->database()->saveSetting("asset_server_port", httpPort);

    QMessageBox::information(this, tr("成功"), tr("ポート設定を保存しました。設定の反映にはアプリの再起動が必要です。"));
}

void SettingsWidget::onAuthClicked()
{
    QString secretKey = "twitch_overlay_secret_key_2026";

    if (m_useCustomCredentialsCb->isChecked()) {
        QString clientId = m_clientIdEdit->text().trimmed();
        QString clientSecret = m_clientSecretEdit->text().trimmed();
        QString broadcasterId = m_broadcasterIdEdit->text().trimmed();

        if (clientId.isEmpty() || clientSecret.isEmpty()) {
            QMessageBox::warning(this, tr("入力エラー"), tr("カスタム設定を使用する場合、Client ID と Client Secret は必須です。"));
            return;
        }

        m_app->config()->set("twitch_client_id", clientId);
        m_app->config()->saveSecureString("twitch_client_secret", clientSecret, secretKey);
        if (!broadcasterId.isEmpty()) {
            m_app->config()->set("twitch_broadcaster_id", broadcasterId);
        } else {
            m_app->config()->remove("twitch_broadcaster_id");
        }
        m_app->config()->save();

        m_app->twitchAuth()->setCredentials(clientId, clientSecret);
    } else {
        m_app->config()->remove("twitch_client_id");
        m_app->config()->remove("twitch_client_secret");
        m_app->config()->save();

        m_app->twitchAuth()->setCredentials(TWITCH_GLOBAL_CLIENT_ID, TWITCH_GLOBAL_CLIENT_SECRET);
    }

    QMessageBox::information(this, tr("認証開始"), tr("これより外部ブラウザを開いて Twitch 連携認可を開始します。"));
    m_app->twitchAuth()->startAuthFlow();
}
