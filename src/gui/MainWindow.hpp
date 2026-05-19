#pragma once

#include <QMainWindow>
#include <QTabWidget>
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

public:
    explicit MainWindow(Application* app, QWidget* parent = nullptr);
    ~MainWindow() = default;

private:
    void setupUi();
    void setupConnections();
};
