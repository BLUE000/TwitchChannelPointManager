#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>

class Application;

class QCheckBox;
class QGroupBox;

class SettingsWidget : public QWidget {
    Q_OBJECT
private:
    Application* m_app;

    QSpinBox* m_wsPortSpin;
    QSpinBox* m_httpPortSpin;
    QPushButton* m_savePortsBtn;

    QCheckBox* m_useCustomCredentialsCb;
    QGroupBox* m_customCredentialsGroup;
    QLineEdit* m_clientIdEdit;
    QLineEdit* m_clientSecretEdit;
    QLineEdit* m_broadcasterIdEdit;
    QPushButton* m_authBtn;

    // 外部スクリプト設定用
    QCheckBox* m_enableScriptIntegrationCb;
    QGroupBox* m_scriptGroup;
    QLineEdit* m_phpPathEdit;
    QLineEdit* m_perlPathEdit;
    QPushButton* m_saveScriptBtn;

public:
    explicit SettingsWidget(Application* app, QWidget* parent = nullptr);
    ~SettingsWidget() = default;

    void loadCurrentSettings();

private slots:
    void onSavePortsClicked();
    void onAuthClicked();
    void onBrowsePhpPath();
    void onBrowsePerlPath();
    void onSaveScriptClicked();

private:
    void setupUi();
};
