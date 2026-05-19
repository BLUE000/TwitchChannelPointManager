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
    headerLayout->addWidget(new QLabel("📊 今日の演出再生ランキング:", this));

    m_refreshButton = new QPushButton("更新", this);
    m_refreshButton->setFixedWidth(80);
    connect(m_refreshButton, &QPushButton::clicked, this, &StatisticsWidget::refreshRanking);
    headerLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(headerLayout);

    m_rankingTable = new QTableWidget(this);
    m_rankingTable->setColumnCount(3);
    m_rankingTable->setHorizontalHeaderLabels({"順位", "報酬名", "再生回数"});
    m_rankingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_rankingTable->verticalHeader()->setVisible(false);
    m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rankingTable->setStyleSheet("QTableWidget { gridline-color: #333333; }");
    
    mainLayout->addWidget(m_rankingTable);
}

void StatisticsWidget::refreshRanking()
{
    m_rankingTable->setRowCount(0);

    if (!m_app->database()) return;

    QList<QPair<QString, int>> ranking = m_app->database()->getTodayRanking();

    int row = 0;
    for (const auto& pair : ranking) {
        m_rankingTable->insertRow(row);
        
        // 順位
        auto* rankItem = new QTableWidgetItem(QString("%1 位").arg(row + 1));
        rankItem->setTextAlignment(Qt::AlignCenter);
        
        // 報酬名
        auto* nameItem = new QTableWidgetItem(pair.first);
        
        // 再生数
        auto* countItem = new QTableWidgetItem(QString("%1 回").arg(pair.second));
        countItem->setTextAlignment(Qt::AlignCenter);

        m_rankingTable->setItem(row, 0, rankItem);
        m_rankingTable->setItem(row, 1, nameItem);
        m_rankingTable->setItem(row, 2, countItem);
        row++;
    }
}
