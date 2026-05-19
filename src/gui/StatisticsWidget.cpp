#include "StatisticsWidget.hpp"
#include "../core/Application.hpp"
#include "../database/Database.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

StatisticsWidget::StatisticsWidget(Application* app, QWidget* parent)
    : QWidget(parent)
    , m_app(app)
{
    setupUi();
    refreshRanking();
}

void StatisticsWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->addWidget(new QLabel("📊 統計情報", this));
    headerLayout->addStretch();

    m_refreshButton = new QPushButton("更新", this);
    m_refreshButton->setFixedWidth(80);
    connect(m_refreshButton, &QPushButton::clicked, this, &StatisticsWidget::refreshRanking);
    headerLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(headerLayout);

    m_tabWidget = new QTabWidget(this);
    
    // --- タブ1: 今日の全体ランキング ---
    m_rankingTable = new QTableWidget(this);
    m_rankingTable->setColumnCount(3);
    m_rankingTable->setHorizontalHeaderLabels({"順位", "報酬名", "再生回数"});
    m_rankingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_rankingTable->verticalHeader()->setVisible(false);
    m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rankingTable->setStyleSheet("QTableWidget { gridline-color: #333333; }");
    m_tabWidget->addTab(m_rankingTable, "今日のランキング");

    // --- タブ2: ユーザ別利用統計 ---
    m_userStatsTable = new QTableWidget(this);
    m_userStatsTable->setColumnCount(3);
    m_userStatsTable->setHorizontalHeaderLabels({"ユーザ名", "報酬名", "利用回数"});
    m_userStatsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_userStatsTable->verticalHeader()->setVisible(false);
    m_userStatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userStatsTable->setStyleSheet("QTableWidget { gridline-color: #333333; }");
    m_tabWidget->addTab(m_userStatsTable, "ユーザ別利用統計");

    mainLayout->addWidget(m_tabWidget);
}

void StatisticsWidget::refreshRanking()
{
    m_rankingTable->setRowCount(0);
    m_userStatsTable->setRowCount(0);

    if (!m_app->database()) return;

    // --- 今日のランキング ---
    QList<QPair<QString, int>> ranking = m_app->database()->getTodayRanking();
    int row = 0;
    for (const auto& pair : ranking) {
        m_rankingTable->insertRow(row);
        
        auto* rankItem = new QTableWidgetItem(QString("%1 位").arg(row + 1));
        rankItem->setTextAlignment(Qt::AlignCenter);
        auto* nameItem = new QTableWidgetItem(pair.first);
        auto* countItem = new QTableWidgetItem(QString("%1 回").arg(pair.second));
        countItem->setTextAlignment(Qt::AlignCenter);

        m_rankingTable->setItem(row, 0, rankItem);
        m_rankingTable->setItem(row, 1, nameItem);
        m_rankingTable->setItem(row, 2, countItem);
        row++;
    }

    // --- ユーザ別利用統計 ---
    QList<UserUsageStat> userStats = m_app->database()->getUserUsageStatistics();
    row = 0;
    for (const auto& stat : userStats) {
        m_userStatsTable->insertRow(row);
        
        auto* userItem = new QTableWidgetItem(stat.username);
        userItem->setTextAlignment(Qt::AlignCenter);
        auto* nameItem = new QTableWidgetItem(stat.rewardName);
        auto* countItem = new QTableWidgetItem(QString("%1 回").arg(stat.count));
        countItem->setTextAlignment(Qt::AlignCenter);

        m_userStatsTable->setItem(row, 0, userItem);
        m_userStatsTable->setItem(row, 1, nameItem);
        m_userStatsTable->setItem(row, 2, countItem);
        row++;
    }
}
