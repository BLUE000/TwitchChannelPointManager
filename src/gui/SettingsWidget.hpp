#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>

class Application;

class SettingsWidget : public QWidget {
    Q_OBJECT
private:
    Application* m_app;

    QSpinBox* m_wsPortSpin;
    QSpinBox* m_httpPortSpin;
    QPushButton* m_savePortsBtn;

    QLineEdit* m_clientIdEdit;
    QLineEdit* m_clientSecretEdit;
    QLineEdit* m_broadcasterIdEdit;
    QPushButton* m_authBtn;

public:
    explicit SettingsWidget(Application* app, QWidget* parent = nullptr);
    ~SettingsWidget() = default;

    void loadCurrentSettings();

private slots:
    void onSavePortsClicked();
    void onAuthClicked();

private:
    void setupUi();
};
