#include "StatisticsWidget.hpp"
#include "../core/Application.hpp"
#include "../database/Database.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QFile>
#include <QTextStream>

StatisticsWidget::StatisticsWidget(Application* app, QWidget* parent)
    : QWidget(parent)
    , m_app(app)
{
    setupUi();
    loadStyles();
    refreshRanking();
}

void StatisticsWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->addWidget(new QLabel("📊 統計情報", this));
    
    m_periodCombo = new QComboBox(this);
    m_periodCombo->addItem("今日");
    m_periodCombo->addItem("今週");
    m_periodCombo->addItem("今月");
    m_periodCombo->addItem("全期間");
    connect(m_periodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StatisticsWidget::refreshRanking);
    headerLayout->addWidget(new QLabel(" 表示期間:", this));
    headerLayout->addWidget(m_periodCombo);
    
    headerLayout->addStretch();

    m_exportCsvButton = new QPushButton("CSV出力", this);
    connect(m_exportCsvButton, &QPushButton::clicked, this, &StatisticsWidget::onExportCsvClicked);
    headerLayout->addWidget(m_exportCsvButton);

    m_resetButton = new QPushButton("リセット", this);
    m_resetButton->setStyleSheet("color: #ff5555;");
    connect(m_resetButton, &QPushButton::clicked, this, &StatisticsWidget::onResetClicked);
    headerLayout->addWidget(m_resetButton);

    m_refreshButton = new QPushButton("更新", this);
    m_refreshButton->setFixedWidth(80);
    connect(m_refreshButton, &QPushButton::clicked, this, &StatisticsWidget::refreshRanking);
    headerLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(headerLayout);

    m_tabWidget = new QTabWidget(this);
    
    // --- タブ1: ランキング ---
    QWidget* tab1 = new QWidget(this);
    auto* tab1Layout = new QVBoxLayout(tab1);
    
    auto* tab1Controls = new QHBoxLayout();
    m_rankingBgBtn = new QPushButton("背景画像設定...", this);
    m_rankingColorBtn = new QPushButton("文字色設定...", this);
    connect(m_rankingBgBtn, &QPushButton::clicked, this, &StatisticsWidget::onChangeRankingBg);
    connect(m_rankingColorBtn, &QPushButton::clicked, this, &StatisticsWidget::onChangeRankingColor);
    tab1Controls->addWidget(m_rankingBgBtn);
    tab1Controls->addWidget(m_rankingColorBtn);
    tab1Controls->addStretch();
    tab1Layout->addLayout(tab1Controls);

    m_rankingTable = new QTableWidget(this);
    m_rankingTable->setColumnCount(3);
    m_rankingTable->setHorizontalHeaderLabels({"順位", "報酬名", "再生回数"});
    m_rankingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_rankingTable->horizontalHeader()->setStretchLastSection(true);
    m_rankingTable->verticalHeader()->setVisible(false);
    m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tab1Layout->addWidget(m_rankingTable);
    m_tabWidget->addTab(tab1, "ランキング");

    // --- タブ2: ユーザ別利用統計 ---
    QWidget* tab2 = new QWidget(this);
    auto* tab2Layout = new QVBoxLayout(tab2);

    auto* tab2Controls = new QHBoxLayout();
    m_userStatsBgBtn = new QPushButton("背景画像設定...", this);
    m_userStatsColorBtn = new QPushButton("文字色設定...", this);
    connect(m_userStatsBgBtn, &QPushButton::clicked, this, &StatisticsWidget::onChangeUserStatsBg);
    connect(m_userStatsColorBtn, &QPushButton::clicked, this, &StatisticsWidget::onChangeUserStatsColor);
    tab2Controls->addWidget(m_userStatsBgBtn);
    tab2Controls->addWidget(m_userStatsColorBtn);
    tab2Controls->addStretch();
    tab2Layout->addLayout(tab2Controls);

    m_userStatsTable = new QTableWidget(this);
    m_userStatsTable->setColumnCount(3);
    m_userStatsTable->setHorizontalHeaderLabels({"ユーザ名", "報酬名", "利用回数"});
    m_userStatsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_userStatsTable->horizontalHeader()->setStretchLastSection(true);
    m_userStatsTable->verticalHeader()->setVisible(false);
    m_userStatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tab2Layout->addWidget(m_userStatsTable);
    m_tabWidget->addTab(tab2, "ユーザ別利用統計");

    mainLayout->addWidget(m_tabWidget);
}

void StatisticsWidget::refreshRanking()
{
    m_rankingTable->setRowCount(0);
    m_userStatsTable->setRowCount(0);

    if (!m_app->database()) return;

    int periodIndex = m_periodCombo->currentIndex();

    // --- ランキング ---
    QList<QPair<QString, int>> ranking = m_app->database()->getRanking(periodIndex);
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
    QList<UserUsageStat> userStats = m_app->database()->getUserUsageStatistics(periodIndex);
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

void StatisticsWidget::onExportCsvClicked()
{
    QTableWidget* currentTable = m_tabWidget->currentIndex() == 0 ? m_rankingTable : m_userStatsTable;
    if (currentTable->rowCount() == 0) {
        QMessageBox::information(this, "CSV出力", "出力するデータがありません。");
        return;
    }

    QString defaultName = m_tabWidget->currentIndex() == 0 ? "ranking_stats.csv" : "user_stats.csv";
    QString fileName = QFileDialog::getSaveFileName(this, "CSVを保存", defaultName, "CSVファイル (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "エラー", "ファイルの作成に失敗しました。");
        return;
    }

    QTextStream out(&file);
    // BOM for UTF-8 Excel compatibility
    out << QString("\xEF\xBB\xBF");

    // Header
    QStringList headers;
    for (int col = 0; col < currentTable->columnCount(); ++col) {
        headers << "\"" + currentTable->horizontalHeaderItem(col)->text() + "\"";
    }
    out << headers.join(",") << "\n";

    // Data
    for (int row = 0; row < currentTable->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < currentTable->columnCount(); ++col) {
            QTableWidgetItem* item = currentTable->item(row, col);
            QString text = item ? item->text() : "";
            // Escape quotes
            text.replace("\"", "\"\"");
            rowData << "\"" + text + "\"";
        }
        out << rowData.join(",") << "\n";
    }

    file.close();
    QMessageBox::information(this, "完了", "CSVを出力しました。");
}

void StatisticsWidget::onResetClicked()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "統計リセットの確認",
                                  "すべての統計履歴（ランキングおよびユーザ別統計）を削除します。\nよろしいですか？",
                                  QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (m_app->database() && m_app->database()->clearUsageLogs()) {
            QMessageBox::information(this, "完了", "統計をリセットしました。");
            refreshRanking();
        } else {
            QMessageBox::critical(this, "エラー", "統計のリセットに失敗しました。");
        }
    }
}

void StatisticsWidget::loadStyles()
{
    applyTableStyle(m_rankingTable, "stats_ranking");
    applyTableStyle(m_userStatsTable, "stats_user");
}

void StatisticsWidget::applyTableStyle(QTableWidget* table, const QString& prefix)
{
    if (!m_app->database()) return;
    
    QString bgImage = m_app->database()->getSetting(prefix + "_bg", "");
    QString textColor = m_app->database()->getSetting(prefix + "_color", "");

    QString style = "QTableWidget { gridline-color: #333333; ";
    
    if (!bgImage.isEmpty()) {
        style += QString("background-image: url('%1'); ").arg(bgImage);
        style += "background-position: center; ";
        style += "background-attachment: fixed; "; // Qt TableWidgetで背景を固定
    }
    
    if (!textColor.isEmpty()) {
        style += QString("color: %1; ").arg(textColor);
    }
    
    style += "} ";
    
    // セル背景を透明にして画像を見せる
    style += "QTableWidget::item { background-color: transparent; } ";
    
    table->setStyleSheet(style);
}

void StatisticsWidget::onChangeRankingBg()
{
    QString fileName = QFileDialog::getOpenFileName(this, "背景画像を選択（キャンセルでクリア）", "", "Images (*.png *.jpg *.jpeg *.bmp)");
    if (m_app->database()) {
        m_app->database()->saveSetting("stats_ranking_bg", fileName);
        applyTableStyle(m_rankingTable, "stats_ranking");
    }
}

void StatisticsWidget::onChangeRankingColor()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "文字色を選択");
    if (color.isValid() && m_app->database()) {
        m_app->database()->saveSetting("stats_ranking_color", color.name());
        applyTableStyle(m_rankingTable, "stats_ranking");
    }
}

void StatisticsWidget::onChangeUserStatsBg()
{
    QString fileName = QFileDialog::getOpenFileName(this, "背景画像を選択（キャンセルでクリア）", "", "Images (*.png *.jpg *.jpeg *.bmp)");
    if (m_app->database()) {
        m_app->database()->saveSetting("stats_user_bg", fileName);
        applyTableStyle(m_userStatsTable, "stats_user");
    }
}

void StatisticsWidget::onChangeUserStatsColor()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "文字色を選択");
    if (color.isValid() && m_app->database()) {
        m_app->database()->saveSetting("stats_user_color", color.name());
        applyTableStyle(m_userStatsTable, "stats_user");
    }
}
