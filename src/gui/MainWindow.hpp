#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QTimer>
#include <QEvent>
#include <QPushButton>
#include <memory>

class Application;
class DashboardWidget;
class RewardEditorWidget;
class StatisticsWidget;
class SettingsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
private:
    Application* m_app;
    QTabWidget* m_tabWidget;

    DashboardWidget* m_dashboardWidget;
    RewardEditorWidget* m_rewardEditorWidget;
    StatisticsWidget* m_statisticsWidget;
    SettingsWidget* m_settingsWidget;

    QPushButton* m_aboutButton;
    QPushButton* m_helpButton;

    // イースターエッグ（おこじょ）用メンバ
    QTimer* m_idleTimer = nullptr;
    QLabel* m_okojoLabel = nullptr;
    void resetIdleTimer();
    void onIdleTimeout();

public:
    explicit MainWindow(Application* app, QWidget* parent = nullptr);
    ~MainWindow() = default;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupConnections();
    void retranslateUi();
};
