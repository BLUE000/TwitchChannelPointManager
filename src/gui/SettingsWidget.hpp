#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>

class Application;
class QCheckBox;

class SettingsWidget : public QWidget {
    Q_OBJECT
private:
    Application* m_app;

    // 表示言語設定
    QGroupBox* m_langGroup;
    QComboBox* m_languageCombo;
    QPushButton* m_saveLangBtn;

    // ポート設定
    QGroupBox* m_portGroup;
    QSpinBox* m_wsPortSpin;
    QSpinBox* m_httpPortSpin;
    QPushButton* m_savePortsBtn;

    // Twitch設定
    QGroupBox* m_twitchGroup;
    QCheckBox* m_useCustomCredentialsCb;
    QGroupBox* m_customCredentialsGroup;
    QLineEdit* m_clientIdEdit;
    QLineEdit* m_clientSecretEdit;
    QLineEdit* m_broadcasterIdEdit;
    QPushButton* m_authBtn;

public:
    explicit SettingsWidget(Application* app, QWidget* parent = nullptr);
    ~SettingsWidget() = default;

    void loadCurrentSettings();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onSavePortsClicked();
    void onAuthClicked();
    void onSaveLanguageClicked();

private:
    void setupUi();
    void retranslateUi();
};
