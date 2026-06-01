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
#include <QEvent>

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
    m_titleLabel = new QLabel(tr("📊 統計情報"), this);
    headerLayout->addWidget(m_titleLabel);
    
    m_periodCombo = new QComboBox(this);
    // 初期値設定
    m_periodCombo->addItem(tr("今日"));
    m_periodCombo->addItem(tr("今週"));
    m_periodCombo->addItem(tr("今月"));
    m_periodCombo->addItem(tr("全期間"));
    m_periodCombo->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px;");
    connect(m_periodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StatisticsWidget::refreshRanking);
    
    m_periodLabel = new QLabel(tr(" 表示期間:"), this);
    headerLayout->addWidget(m_periodLabel);
    headerLayout->addWidget(m_periodCombo);
    
    headerLayout->addStretch();

    m_exportCsvButton = new QPushButton(tr("CSV出力"), this);
    connect(m_exportCsvButton, &QPushButton::clicked, this, &StatisticsWidget::onExportCsvClicked);
    headerLayout->addWidget(m_exportCsvButton);

    m_resetButton = new QPushButton(tr("リセット"), this);
    m_resetButton->setStyleSheet("color: #ff5555;");
    connect(m_resetButton, &QPushButton::clicked, this, &StatisticsWidget::onResetClicked);
    headerLayout->addWidget(m_resetButton);

    m_refreshButton = new QPushButton(tr("更新"), this);
    m_refreshButton->setFixedWidth(80);
    connect(m_refreshButton, &QPushButton::clicked, this, &StatisticsWidget::refreshRanking);
    headerLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(headerLayout);

    m_tabWidget = new QTabWidget(this);
    
    // --- タブ1: ランキング ---
    QWidget* tab1 = new QWidget(this);
    auto* tab1Layout = new QVBoxLayout(tab1);
    
    auto* tab1Controls = new QHBoxLayout();
    m_rankingBgBtn = new QPushButton(tr("背景画像設定..."), this);
    m_rankingColorBtn = new QPushButton(tr("文字色設定..."), this);
    connect(m_rankingBgBtn, &QPushButton::clicked, this, &StatisticsWidget::onChangeRankingBg);
    connect(m_rankingColorBtn, &QPushButton::clicked, this, &StatisticsWidget::onChangeRankingColor);
    tab1Controls->addWidget(m_rankingBgBtn);
    tab1Controls->addWidget(m_rankingColorBtn);
    tab1Controls->addStretch();
    tab1Layout->addLayout(tab1Controls);

    m_rankingTable = new QTableWidget(this);
    m_rankingTable->setColumnCount(3);
    m_rankingTable->setHorizontalHeaderLabels({tr("順位"), tr("報酬名"), tr("再生回数")});
    m_rankingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_rankingTable->horizontalHeader()->setStretchLastSection(true);
    m_rankingTable->verticalHeader()->setVisible(false);
    m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tab1Layout->addWidget(m_rankingTable);
    m_tabWidget->addTab(tab1, tr("ランキング"));

    // --- タブ2: ユーザ別利用統計 ---
    QWidget* tab2 = new QWidget(this);
    auto* tab2Layout = new QVBoxLayout(tab2);

    auto* tab2Controls = new QHBoxLayout();
    m_userStatsBgBtn = new QPushButton(tr("背景画像設定..."), this);
    m_userStatsColorBtn = new QPushButton(tr("文字色設定..."), this);
    connect(m_userStatsBgBtn, &QPushButton::clicked, this, &StatisticsWidget::onChangeUserStatsBg);
    connect(m_userStatsColorBtn, &QPushButton::clicked, this, &StatisticsWidget::onChangeUserStatsColor);
    tab2Controls->addWidget(m_userStatsBgBtn);
    tab2Controls->addWidget(m_userStatsColorBtn);
    tab2Controls->addStretch();
    tab2Layout->addLayout(tab2Controls);

    m_userStatsTable = new QTableWidget(this);
    m_userStatsTable->setColumnCount(3);
    m_userStatsTable->setHorizontalHeaderLabels({tr("ユーザ名"), tr("報酬名"), tr("利用回数")});
    m_userStatsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_userStatsTable->horizontalHeader()->setStretchLastSection(true);
    m_userStatsTable->verticalHeader()->setVisible(false);
    m_userStatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tab2Layout->addWidget(m_userStatsTable);
    m_tabWidget->addTab(tab2, tr("ユーザ別利用統計"));

    mainLayout->addWidget(m_tabWidget);
}

void StatisticsWidget::refreshRanking()
{
    m_rankingTable->setRowCount(0);
    m_userStatsTable->setRowCount(0);

    if (!m_app->database()) return;

    int periodIndex = m_periodCombo->currentIndex();
    if (periodIndex < 0) periodIndex = 0;

    // --- ランキング ---
    QList<QPair<QString, int>> ranking = m_app->database()->getRanking(periodIndex);
    int row = 0;
    for (const auto& pair : ranking) {
        m_rankingTable->insertRow(row);
        
        auto* rankItem = new QTableWidgetItem(QString(tr("%1 位")).arg(row + 1));
        rankItem->setTextAlignment(Qt::AlignCenter);
        auto* nameItem = new QTableWidgetItem(pair.first);
        auto* countItem = new QTableWidgetItem(QString(tr("%1 回")).arg(pair.second));
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
        auto* countItem = new QTableWidgetItem(QString(tr("%1 回")).arg(stat.count));
        countItem->setTextAlignment(Qt::AlignCenter);

        m_userStatsTable->setItem(row, 0, userItem);
        m_userStatsTable->setItem(row, 1, nameItem);
        m_userStatsTable->setItem(row, 2, countItem);
        row++;
    }

    // 同一ユーザのセルを縦方向に結合
    int i = 0;
    while (i < userStats.size()) {
        int j = i + 1;
        while (j < userStats.size() && userStats[j].username == userStats[i].username) {
            j++;
        }
        int span = j - i;
        if (span > 1) {
            m_userStatsTable->setSpan(i, 0, span, 1);
        }
        i = j;
    }
}

void StatisticsWidget::onExportCsvClicked()
{
    QTableWidget* currentTable = m_tabWidget->currentIndex() == 0 ? m_rankingTable : m_userStatsTable;
    if (currentTable->rowCount() == 0) {
        QMessageBox::information(this, tr("CSV出力"), tr("出力するデータがありません。"));
        return;
    }

    QString defaultName = m_tabWidget->currentIndex() == 0 ? "ranking_stats.csv" : "user_stats.csv";
    QString fileName = QFileDialog::getSaveFileName(this, tr("CSVを保存"), defaultName, tr("CSVファイル (*.csv)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("エラー"), tr("ファイルの作成に失敗しました。"));
        return;
    }

    QTextStream out(&file);
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
            text.replace("\"", "\"\"");
            rowData << "\"" + text + "\"";
        }
        out << rowData.join(",") << "\n";
    }

    file.close();
    QMessageBox::information(this, tr("完了"), tr("CSVを出力しました。"));
}

void StatisticsWidget::onResetClicked()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("統計リセットの確認"),
                                  tr("すべての統計履歴（ランキングおよびユーザ別統計）を削除します。\nよろしいですか？"),
                                  QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (m_app->database() && m_app->database()->clearUsageLogs()) {
            QMessageBox::information(this, tr("完了"), tr("統計をリセットしました。"));
            refreshRanking();
        } else {
            QMessageBox::critical(this, tr("エラー"), tr("統計のリセットに失敗しました。"));
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
        style += QString("border-image: url('%1') 0 0 0 0 stretch stretch; ").arg(bgImage);
    }
    
    if (!textColor.isEmpty()) {
        style += QString("color: %1; ").arg(textColor);
    }
    
    style += "} ";
    style += "QTableWidget::item { background-color: transparent; } ";
    
    table->setStyleSheet(style);
}

void StatisticsWidget::onChangeRankingBg()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("背景画像を選択（キャンセルでクリア）"), "", "Images (*.png *.jpg *.jpeg *.bmp)");
    if (m_app->database()) {
        m_app->database()->saveSetting("stats_ranking_bg", fileName);
        applyTableStyle(m_rankingTable, "stats_ranking");
    }
}

void StatisticsWidget::onChangeRankingColor()
{
    QColor color = QColorDialog::getColor(Qt::white, this, tr("文字色を選択"));
    if (color.isValid() && m_app->database()) {
        m_app->database()->saveSetting("stats_ranking_color", color.name());
        applyTableStyle(m_rankingTable, "stats_ranking");
    }
}

void StatisticsWidget::onChangeUserStatsBg()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("背景画像を選択（キャンセルでクリア）"), "", "Images (*.png *.jpg *.jpeg *.bmp)");
    if (m_app->database()) {
        m_app->database()->saveSetting("stats_user_bg", fileName);
        applyTableStyle(m_userStatsTable, "stats_user");
    }
}

void StatisticsWidget::onChangeUserStatsColor()
{
    QColor color = QColorDialog::getColor(Qt::white, this, tr("文字色を選択"));
    if (color.isValid() && m_app->database()) {
        m_app->database()->saveSetting("stats_user_color", color.name());
        applyTableStyle(m_userStatsTable, "stats_user");
    }
}

void StatisticsWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void StatisticsWidget::retranslateUi()
{
    m_titleLabel->setText(tr("📊 統計情報"));
    m_periodLabel->setText(tr(" 表示期間:"));

    // コンボボックスのアイテムを再翻訳
    m_periodCombo->blockSignals(true);
    int currentIdx = m_periodCombo->currentIndex();
    m_periodCombo->clear();
    m_periodCombo->addItem(tr("今日"));
    m_periodCombo->addItem(tr("今週"));
    m_periodCombo->addItem(tr("今月"));
    m_periodCombo->addItem(tr("全期間"));
    m_periodCombo->setCurrentIndex(currentIdx >= 0 ? currentIdx : 0);
    m_periodCombo->blockSignals(false);

    m_exportCsvButton->setText(tr("CSV出力"));
    m_resetButton->setText(tr("リセット"));
    m_refreshButton->setText(tr("更新"));

    m_rankingBgBtn->setText(tr("背景画像設定..."));
    m_rankingColorBtn->setText(tr("文字色設定..."));
    m_rankingTable->setHorizontalHeaderLabels({tr("順位"), tr("報酬名"), tr("再生回数")});
    m_tabWidget->setTabText(0, tr("ランキング"));

    m_userStatsBgBtn->setText(tr("背景画像設定..."));
    m_userStatsColorBtn->setText(tr("文字色設定..."));
    m_userStatsTable->setHorizontalHeaderLabels({tr("ユーザ名"), tr("報酬名"), tr("利用回数")});
    m_tabWidget->setTabText(1, tr("ユーザ別利用統計"));

    refreshRanking();
}
