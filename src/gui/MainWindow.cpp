#include "MainWindow.hpp"
#include "DashboardWidget.hpp"
#include "RewardEditorWidget.hpp"
#include "StatisticsWidget.hpp"
#include "SettingsWidget.hpp"
#include "../core/Application.hpp"
#include <QVBoxLayout>

MainWindow::MainWindow(Application* app, QWidget* parent)
    : QMainWindow(parent)
    , m_app(app)
{
    setWindowTitle("Twitch Channel Point Manager - Version 2.0.0");
    resize(850, 600);

    setupUi();
    setupConnections();
}

void MainWindow::setupUi()
{
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    m_dashboardWidget = new DashboardWidget(m_app, m_tabWidget);
    m_rewardEditorWidget = new RewardEditorWidget(m_app, m_tabWidget);
    m_statisticsWidget = new StatisticsWidget(m_app, m_tabWidget);
    m_settingsWidget = new SettingsWidget(m_app, m_tabWidget);

    m_tabWidget->addTab(m_dashboardWidget, "🏠 ダッシュボード");
    m_tabWidget->addTab(m_rewardEditorWidget, "🎁 報酬演出管理");
    m_tabWidget->addTab(m_statisticsWidget, "📊 統計ランキング");
    m_tabWidget->addTab(m_settingsWidget, "⚙️ システム設定");

    // 全体の基本スタイル
    setStyleSheet(R"(
        QMainWindow { background-color: #121214; }
        QTabWidget::pane { border: 1px solid #29292E; background-color: #1D1D22; top: -1px; }
        QTabBar::tab { background-color: #121214; color: #A9A9B2; border: 1px solid #29292E; padding: 10px 20px; border-top-left-radius: 4px; border-top-right-radius: 4px; }
        QTabBar::tab:selected { background-color: #1D1D22; color: #FFFFFF; border-bottom-color: #1D1D22; font-weight: bold; }
        QGroupBox { border: 1px solid #29292E; border-radius: 6px; margin-top: 12px; font-weight: bold; color: #FFFFFF; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }
        QLabel { color: #E1E1E6; }
        QLineEdit { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px; }
        QSpinBox { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px; }
        QComboBox { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px; }
        QPushButton { border: 1px solid #29292E; border-radius: 4px; padding: 5px; color: #FFFFFF; background-color: #29292E; }
        QPushButton:hover { background-color: #35353B; }
    )");
}

void MainWindow::setupConnections()
{
    // タブが切り替えられた際に自動的にデータを最新に更新する
    connect(m_tabWidget, &QTabWidget::currentChanged, [this](int index) {
        switch (index) {
            case 0: // ダッシュボード
                m_dashboardWidget->refreshStats();
                break;
            case 1: // 報酬管理
                m_rewardEditorWidget->reloadRewardsList();
                break;
            case 2: // 統計ランキング
                m_statisticsWidget->refreshRanking();
                break;
            case 3: // 設定
                m_settingsWidget->loadCurrentSettings();
                break;
        }
    });
}
