#include "DbViewerWindow.hpp"
#include "../../database/Database.hpp"
#include "../../core/Logger.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QUuid>
#include <QScrollArea>
#include <QFormLayout>

DbViewerWindow::DbViewerWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_db(new Database(this))
    , m_selectedRewardId("")
    , m_selectedEffectIndex(-1)
{
    setWindowTitle("Twitch Overlay - stand-alone Database Viewer & Editor");
    resize(1200, 750);
    
    setupUi();
    clearForm();
}

DbViewerWindow::~DbViewerWindow()
{
    m_db->close();
}

bool DbViewerWindow::initializeDb(const QString& dbPath)
{
    Logger::instance()->initialize("db_viewer.log");
    
    if (!m_db->open(dbPath)) {
        QMessageBox::critical(this, "接続エラー", 
            QString("データベースファイルのオープンに失敗しました。\nパス: %1").arg(dbPath));
        return false;
    }
    
    refreshData();
    return true;
}

void DbViewerWindow::setupUi()
{
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    // ==========================================
    // タブ1: 🎁 報酬と演出の編集
    // ==========================================
    auto* editorTab = new QWidget(m_tabWidget);
    auto* editorLayout = new QHBoxLayout(editorTab);
    editorLayout->setContentsMargins(10, 10, 10, 10);

    // スプリッターで左右に分割
    auto* splitter = new QSplitter(Qt::Horizontal, editorTab);
    editorLayout->addWidget(splitter);

    // ==========================================
    // 左ペイン: 一覧 ＆ 検索
    // ==========================================
    auto* leftContainer = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // 検索部
    auto* searchLayout = new QHBoxLayout();
    auto* searchLabel = new QLabel("🔍 検索:", leftContainer);
    searchLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #FFFFFF;");
    searchLayout->addWidget(searchLabel);

    m_searchEdit = new QLineEdit(leftContainer);
    m_searchEdit->setPlaceholderText("報酬名、ファイルパス、テキスト等で絞り込み...");
    m_searchEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DbViewerWindow::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit);

    m_refreshButton = new QPushButton("🔄 更新", leftContainer);
    m_refreshButton->setStyleSheet("background-color: #29292E; color: #FFFFFF; font-weight: bold; border: 1px solid #35353B; border-radius: 4px; padding: 6px 12px;");
    connect(m_refreshButton, &QPushButton::clicked, this, &DbViewerWindow::refreshData);
    searchLayout->addWidget(m_refreshButton);

    leftLayout->addLayout(searchLayout);

    // テーブル
    m_tableWidget = new QTableWidget(leftContainer);
    m_tableWidget->setColumnCount(8);
    QStringList headers;
    headers << "🎁 報酬名 (ポイント数)" << "🔢 演出" << "🎨 種類" << "🖼️ 画像/動画アセット" << "🔊 効果音" << "⏱️ 秒" << "🔉 音量" << "💬 テキスト";
    m_tableWidget->setHorizontalHeaderLabels(headers);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->verticalHeader()->setVisible(false);
    
    m_tableWidget->setStyleSheet(R"(
        QTableWidget {
            background-color: #121214;
            color: #E1E1E6;
            gridline-color: #29292E;
            border: 1px solid #29292E;
            border-radius: 6px;
            alternate-background-color: #1D1D22;
        }
        QTableWidget::item { padding: 8px; }
        QTableWidget::item:selected { background-color: #29292E; color: #FFFFFF; }
        QHeaderView::section {
            background-color: #1D1D22;
            color: #FFFFFF;
            font-weight: bold;
            border: 1px solid #29292E;
            padding: 6px;
        }
    )");

    m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    connect(m_tableWidget, &QTableWidget::itemSelectionChanged, this, &DbViewerWindow::onTableSelectionChanged);
    leftLayout->addWidget(m_tableWidget);

    // 左下ボタン群
    auto* leftBottomLayout = new QHBoxLayout();
    m_newRewardButton = new QPushButton("➕ 新規報酬を追加", leftContainer);
    m_newRewardButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; border: none; border-radius: 4px; padding: 8px 15px;");
    connect(m_newRewardButton, &QPushButton::clicked, this, &DbViewerWindow::onNewRewardClicked);
    leftBottomLayout->addWidget(m_newRewardButton);
    leftBottomLayout->addStretch();
    leftLayout->addLayout(leftBottomLayout);

    // ==========================================
    // 右ペイン: 編集フォーム
    // ==========================================
    auto* rightContainer = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_formGroup = new QGroupBox("🛠️ 演出と報酬の編集", rightContainer);
    m_formGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #FFFFFF; }");
    auto* groupLayout = new QVBoxLayout(m_formGroup);

    // スクロールエリア（フォーム縦長化対策）
    auto* scrollArea = new QScrollArea(m_formGroup);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background-color: transparent;");
    
    auto* scrollContent = new QWidget(scrollArea);
    auto* formLayout = new QFormLayout(scrollContent);
    formLayout->setContentsMargins(10, 10, 10, 10);
    formLayout->setSpacing(12);

    // 1. 報酬の基本情報
    auto* section1 = new QLabel("📋 【1】 報酬基本設定", scrollContent);
    section1->setStyleSheet("font-weight: bold; color: #2196F3; font-size: 13px; margin-top: 5px;");
    formLayout->addRow(section1);

    m_rewardIdEdit = new QLineEdit(scrollContent);
    m_rewardIdEdit->setReadOnly(true);
    m_rewardIdEdit->setStyleSheet("background-color: #1D1D22; color: #888888; border: 1px solid #29292E;");
    formLayout->addRow("報酬 ID (読取専用):", m_rewardIdEdit);

    m_rewardNameEdit = new QLineEdit(scrollContent);
    formLayout->addRow("🎁 報酬名:", m_rewardNameEdit);

    m_rewardCostSpin = new QSpinBox(scrollContent);
    m_rewardCostSpin->setRange(0, 1000000);
    m_rewardCostSpin->setSingleStep(100);
    formLayout->addRow("ポイント数 (Cost):", m_rewardCostSpin);

    m_rewardCooldownSpin = new QSpinBox(scrollContent);
    m_rewardCooldownSpin->setRange(0, 86400);
    formLayout->addRow("クールダウン秒数:", m_rewardCooldownSpin);

    m_rewardModeCombo = new QComboBox(scrollContent);
    m_rewardModeCombo->addItem("全て再生 (sequential)", "sequential");
    m_rewardModeCombo->addItem("ランダムで1つ再生 (random)", "random");
    formLayout->addRow("演出再生モード:", m_rewardModeCombo);

    m_rewardEnabledCheck = new QCheckBox("この報酬を有効にする", scrollContent);
    formLayout->addRow("ステータス:", m_rewardEnabledCheck);

    // 分割線
    auto* line = new QFrame(scrollContent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("background-color: #29292E;");
    formLayout->addRow(line);

    // 2. 演出の詳細情報
    auto* section2 = new QLabel("🎨 【2】 演出詳細設定", scrollContent);
    section2->setStyleSheet("font-weight: bold; color: #4CAF50; font-size: 13px; margin-top: 5px;");
    formLayout->addRow(section2);

    m_effectTypeCombo = new QComboBox(scrollContent);
    m_effectTypeCombo->addItem("🖼️ 画像", "image");
    m_effectTypeCombo->addItem("🎥 動画", "video");
    m_effectTypeCombo->addItem("🔊 音声のみ", "sound");
    m_effectTypeCombo->addItem("💬 テキストのみ", "text");
    connect(m_effectTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DbViewerWindow::onEffectTypeChanged);
    formLayout->addRow("演出の種類:", m_effectTypeCombo);

    // 画像/動画パス
    auto* pathLayout = new QHBoxLayout();
    m_filePathEdit = new QLineEdit(scrollContent);
    pathLayout->addWidget(m_filePathEdit);
    m_filePathSelectBtn = new QPushButton("📁 参照", scrollContent);
    m_filePathSelectBtn->setStyleSheet("padding: 4px 10px; background-color: #29292E;");
    connect(m_filePathSelectBtn, &QPushButton::clicked, this, &DbViewerWindow::selectFilePath);
    pathLayout->addWidget(m_filePathSelectBtn);
    formLayout->addRow("画像・動画ファイルパス:", pathLayout);

    // 音声パス
    auto* audioLayout = new QHBoxLayout();
    m_audioPathEdit = new QLineEdit(scrollContent);
    audioLayout->addWidget(m_audioPathEdit);
    m_audioPathSelectBtn = new QPushButton("📁 参照", scrollContent);
    m_audioPathSelectBtn->setStyleSheet("padding: 4px 10px; background-color: #29292E;");
    connect(m_audioPathSelectBtn, &QPushButton::clicked, this, &DbViewerWindow::selectAudioPath);
    audioLayout->addWidget(m_audioPathSelectBtn);
    formLayout->addRow("効果音ファイルパス:", audioLayout);

    m_durationSpin = new QSpinBox(scrollContent);
    m_durationSpin->setRange(1, 300);
    formLayout->addRow("演出表示時間 (秒):", m_durationSpin);

    m_volumeSpin = new QSpinBox(scrollContent);
    m_volumeSpin->setRange(0, 100);
    formLayout->addRow("音量 (%):", m_volumeSpin);

    m_scaleSpin = new QSpinBox(scrollContent);
    m_scaleSpin->setRange(1, 100);
    formLayout->addRow("画像・動画縮尺 (%):", m_scaleSpin);

    m_textEdit = new QLineEdit(scrollContent);
    formLayout->addRow("表示テキスト:", m_textEdit);

    m_positionPresetCombo = new QComboBox(scrollContent);
    m_positionPresetCombo->addItem("中央 (center)", "center");
    m_positionPresetCombo->addItem("左上 (top_left)", "top_left");
    m_positionPresetCombo->addItem("右上 (top_right)", "top_right");
    m_positionPresetCombo->addItem("左下 (bottom_left)", "bottom_left");
    m_positionPresetCombo->addItem("右下 (bottom_right)", "bottom_right");
    m_positionPresetCombo->addItem("カスタム座標", "custom");
    formLayout->addRow("表示位置プリセット:", m_positionPresetCombo);

    auto* coordLayout = new QHBoxLayout();
    coordLayout->addWidget(new QLabel("Xオフセット:"));
    m_positionXSpin = new QSpinBox(scrollContent);
    m_positionXSpin->setRange(-2000, 2000);
    coordLayout->addWidget(m_positionXSpin);
    coordLayout->addWidget(new QLabel("Yオフセット:"));
    m_positionYSpin = new QSpinBox(scrollContent);
    m_positionYSpin->setRange(-2000, 2000);
    coordLayout->addWidget(m_positionYSpin);
    formLayout->addRow("カスタム位置座標:", coordLayout);

    m_animationCombo = new QComboBox(scrollContent);
    m_animationCombo->addItem("フェードイン・アウト (fade)", "fade");
    m_animationCombo->addItem("スライドイン (slide)", "slide");
    m_animationCombo->addItem("バウンス (bounce)", "bounce");
    m_animationCombo->addItem("アニメーションなし (none)", "none");
    formLayout->addRow("演出効果アニメーション:", m_animationCombo);

    scrollContent->setLayout(formLayout);
    scrollArea->setWidget(scrollContent);
    groupLayout->addWidget(scrollArea);

    // アクションボタン
    auto* btnLayout = new QHBoxLayout();
    
    m_saveButton = new QPushButton("💾 変更をデータベースに保存", m_formGroup);
    m_saveButton->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 10px; border: none; border-radius: 4px;");
    connect(m_saveButton, &QPushButton::clicked, this, &DbViewerWindow::onSaveClicked);
    btnLayout->addWidget(m_saveButton);

    groupLayout->addLayout(btnLayout);

    auto* subBtnLayout = new QHBoxLayout();
    
    m_addEffectButton = new QPushButton("➕ 演出追加", m_formGroup);
    m_addEffectButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 6px; border: none; border-radius: 4px;");
    connect(m_addEffectButton, &QPushButton::clicked, this, &DbViewerWindow::onAddEffectClicked);
    subBtnLayout->addWidget(m_addEffectButton);

    m_deleteEffectButton = new QPushButton("❌ 選択演出を削除", m_formGroup);
    m_deleteEffectButton->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; padding: 6px; border: none; border-radius: 4px;");
    connect(m_deleteEffectButton, &QPushButton::clicked, this, &DbViewerWindow::onDeleteEffectClicked);
    subBtnLayout->addWidget(m_deleteEffectButton);

    m_deleteRewardButton = new QPushButton("🗑️ 報酬ごと削除", m_formGroup);
    m_deleteRewardButton->setStyleSheet("background-color: #F44336; color: white; font-weight: bold; padding: 6px; border: none; border-radius: 4px;");
    connect(m_deleteRewardButton, &QPushButton::clicked, this, &DbViewerWindow::onDeleteRewardClicked);
    subBtnLayout->addWidget(m_deleteRewardButton);

    groupLayout->addLayout(subBtnLayout);
    rightLayout->addWidget(m_formGroup);

    splitter->addWidget(leftContainer);
    splitter->addWidget(rightContainer);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    m_tabWidget->addTab(editorTab, "🎁 報酬と演出の編集");

    // ==========================================
    // タブ2: 📊 統計ログ管理とDB軽量化
    // ==========================================
    auto* logsTab = new QWidget(m_tabWidget);
    auto* logsLayout = new QVBoxLayout(logsTab);
    logsLayout->setContentsMargins(15, 15, 15, 15);
    logsLayout->setSpacing(12);

    // 上部コントロールバー
    auto* topLogLayout = new QHBoxLayout();
    
    auto* logSearchLabel = new QLabel("🔍 ログ検索:", logsTab);
    logSearchLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #FFFFFF;");
    topLogLayout->addWidget(logSearchLabel);

    m_logSearchEdit = new QLineEdit(logsTab);
    m_logSearchEdit->setPlaceholderText("ユーザー名、報酬名等で絞り込み...");
    m_logSearchEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    connect(m_logSearchEdit, &QLineEdit::textChanged, this, &DbViewerWindow::onLogSearchTextChanged);
    topLogLayout->addWidget(m_logSearchEdit);

    auto* refreshLogBtn = new QPushButton("🔄 ログ更新", logsTab);
    refreshLogBtn->setStyleSheet("background-color: #29292E; color: #FFFFFF; font-weight: bold; border: 1px solid #35353B; border-radius: 4px; padding: 6px 12px;");
    connect(refreshLogBtn, &QPushButton::clicked, this, &DbViewerWindow::refreshLogData);
    topLogLayout->addWidget(refreshLogBtn);

    topLogLayout->addSpacing(20);

    // DBサイズ表示ラベル
    m_dbSizeLabel = new QLabel("DBファイルサイズ: -", logsTab);
    m_dbSizeLabel->setStyleSheet("font-weight: bold; color: #2196F3; font-size: 13px; background-color: #1D1D22; border: 1px solid #29292E; border-radius: 4px; padding: 6px 12px;");
    topLogLayout->addWidget(m_dbSizeLabel);

    logsLayout->addLayout(topLogLayout);

    // ログ表示テーブル
    m_logTableWidget = new QTableWidget(logsTab);
    m_logTableWidget->setColumnCount(4);
    QStringList logHeaders;
    logHeaders << "ID" << "🎁 報酬名" << "👤 ユーザー名" << "⏱️ 使用日時";
    m_logTableWidget->setHorizontalHeaderLabels(logHeaders);
    m_logTableWidget->setAlternatingRowColors(true);
    m_logTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logTableWidget->verticalHeader()->setVisible(false);
    m_logTableWidget->setStyleSheet(m_tableWidget->styleSheet());
    m_logTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_logTableWidget->horizontalHeader()->setStretchLastSection(true);
    logsLayout->addWidget(m_logTableWidget);

    // 下部クリーンアップツールバー
    auto* cleanupGroup = new QGroupBox("🗑️ 統計ログのクリーンアップ & データベース最適化", logsTab);
    cleanupGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #FFFFFF; padding: 15px; }");
    auto* cleanupLayout = new QHBoxLayout(cleanupGroup);
    cleanupLayout->setContentsMargins(15, 15, 15, 15);
    cleanupLayout->setSpacing(12);

    cleanupLayout->addWidget(new QLabel("クリーンアップ対象の期間:", cleanupGroup));

    m_cleanupPeriodCombo = new QComboBox(cleanupGroup);
    m_cleanupPeriodCombo->addItem("全てのログデータを削除", "all");
    m_cleanupPeriodCombo->addItem("1週間以上前のログデータを削除", "1week");
    m_cleanupPeriodCombo->addItem("1ヶ月以上前のログデータを削除", "1month");
    m_cleanupPeriodCombo->addItem("3ヶ月以上前のログデータを削除", "3months");
    m_cleanupPeriodCombo->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px; min-width: 250px;");
    cleanupLayout->addWidget(m_cleanupPeriodCombo);

    m_cleanupButton = new QPushButton("🚨 クリーンアップ & DB軽量化 (VACUUM) を実行", cleanupGroup);
    m_cleanupButton->setStyleSheet("background-color: #D32F2F; color: white; font-weight: bold; padding: 8px 16px; border: none; border-radius: 4px;");
    connect(m_cleanupButton, &QPushButton::clicked, this, &DbViewerWindow::onCleanupClicked);
    cleanupLayout->addWidget(m_cleanupButton);

    cleanupLayout->addStretch();
    logsLayout->addWidget(cleanupGroup);

    m_tabWidget->addTab(logsTab, "📊 統計ログ管理とDB軽量化");

    // 全体的なテーマスタイル
    setStyleSheet(R"(
        QMainWindow { background-color: #121214; }
        QLabel { color: #E1E1E6; font-size: 12px; }
        QLineEdit { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 5px; }
        QSpinBox { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 5px 26px 5px 6px; min-height: 26px; }
        QSpinBox::up-button { width: 20px; subcontrol-origin: border; subcontrol-position: top right; }
        QSpinBox::down-button { width: 20px; subcontrol-origin: border; subcontrol-position: bottom right; }
        QComboBox { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 5px; }
        QCheckBox { color: #E1E1E6; font-size: 13px; }
        QCheckBox::indicator { border: 1px solid #29292E; background-color: #121214; width: 14px; height: 14px; border-radius: 3px; }
        QCheckBox::indicator:checked { background-color: #2196F3; border-color: #2196F3; }
        QGroupBox { border: 1px solid #29292E; border-radius: 6px; margin-top: 10px; padding-top: 15px; }
        QTabWidget::pane { border: 1px solid #29292E; background-color: #1D1D22; top: -1px; }
        QTabBar::tab { background-color: #121214; color: #A9A9B2; border: 1px solid #29292E; padding: 10px 20px; border-top-left-radius: 4px; border-top-right-radius: 4px; }
        QTabBar::tab:selected { background-color: #1D1D22; color: #FFFFFF; border-bottom-color: #1D1D22; font-weight: bold; }
    )");
}

void DbViewerWindow::refreshData()
{
    m_tableWidget->setRowCount(0);
    m_rewards.clear();
    
    if (!m_db->loadRewards(m_rewards)) {
        LOG_ERROR("Failed to load rewards from DB.");
        return;
    }

    int rowCount = 0;
    for (const auto& r : m_rewards) {
        for (int i = 0; i < r.effects.size(); ++i) {
            const auto& eff = r.effects[i];
            m_tableWidget->insertRow(rowCount);

            // 0: 報酬名
            auto* nameItem = new QTableWidgetItem(QString("%1 (%2pt)").arg(r.name).arg(r.cost));
            nameItem->setData(Qt::UserRole, r.id);
            nameItem->setData(Qt::UserRole + 1, i);
            nameItem->setForeground(QBrush(QColor("#FFFFFF")));
            nameItem->setFont(QFont("Arial", 9, QFont::Bold));
            m_tableWidget->setItem(rowCount, 0, nameItem);

            // 1: 演出インデックス
            auto* indexItem = new QTableWidgetItem(QString("演出 [%1]").arg(i));
            indexItem->setTextAlignment(Qt::AlignCenter);
            indexItem->setForeground(QBrush(QColor("#A9A9B2")));
            m_tableWidget->setItem(rowCount, 1, indexItem);

            // 2: 演出種類
            QString typeStr = "不明";
            QString typeColor = "#FFFFFF";
            if (eff.type == "image")      { typeStr = "🖼️ 画像"; typeColor = "#4CAF50"; }
            else if (eff.type == "video") { typeStr = "🎥 動画"; typeColor = "#FF9800"; }
            else if (eff.type == "sound") { typeStr = "🔊 音声"; typeColor = "#2196F3"; }
            else if (eff.type == "text")  { typeStr = "💬 テキスト"; typeColor = "#9C27B0"; }

            auto* typeItem = new QTableWidgetItem(typeStr);
            typeItem->setTextAlignment(Qt::AlignCenter);
            typeItem->setForeground(QBrush(QColor(typeColor)));
            typeItem->setFont(QFont("Arial", 9, QFont::Bold));
            m_tableWidget->setItem(rowCount, 2, typeItem);

            // 3: 画像/動画パス
            auto* fileItem = new QTableWidgetItem(eff.filePath.isEmpty() ? "-" : eff.filePath);
            if (eff.filePath.isEmpty()) fileItem->setForeground(QBrush(QColor("#555555")));
            m_tableWidget->setItem(rowCount, 3, fileItem);

            // 4: 効果音パス
            auto* audioItem = new QTableWidgetItem(eff.audioPath.isEmpty() ? "-" : eff.audioPath);
            if (eff.audioPath.isEmpty()) audioItem->setForeground(QBrush(QColor("#555555")));
            m_tableWidget->setItem(rowCount, 4, audioItem);

            // 5: 秒数
            auto* durItem = new QTableWidgetItem(QString("%1秒").arg(eff.duration));
            durItem->setTextAlignment(Qt::AlignCenter);
            m_tableWidget->setItem(rowCount, 5, durItem);

            // 6: 音量
            auto* volItem = new QTableWidgetItem(QString("%1%").arg(eff.volume));
            volItem->setTextAlignment(Qt::AlignCenter);
            m_tableWidget->setItem(rowCount, 6, volItem);

            // 7: テキスト
            auto* textItem = new QTableWidgetItem(eff.text.isEmpty() ? "-" : eff.text);
            if (eff.text.isEmpty()) textItem->setForeground(QBrush(QColor("#555555")));
            m_tableWidget->setItem(rowCount, 7, textItem);

            rowCount++;
        }
    }

    m_tableWidget->resizeColumnToContents(0);
    m_tableWidget->resizeColumnToContents(1);
    m_tableWidget->resizeColumnToContents(2);
    m_tableWidget->setColumnWidth(3, 180);
    m_tableWidget->setColumnWidth(4, 180);
    m_tableWidget->resizeColumnToContents(5);
    m_tableWidget->resizeColumnToContents(6);
    
    // 検索フィルタの再適用
    onSearchTextChanged(m_searchEdit->text());

    // 以前選択していた項目があれば再選択
    if (!m_selectedRewardId.isEmpty() && m_selectedEffectIndex >= 0) {
        for (int r = 0; r < m_tableWidget->rowCount(); ++r) {
            auto* item = m_tableWidget->item(r, 0);
            if (item && item->data(Qt::UserRole).toString() == m_selectedRewardId &&
                item->data(Qt::UserRole + 1).toInt() == m_selectedEffectIndex) {
                m_tableWidget->selectRow(r);
                break;
            }
        }
    } else {
        clearForm();
    }

    refreshLogData();
    updateDbSizeDisplay();
}

void DbViewerWindow::onSearchTextChanged(const QString& text)
{
    for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
        bool showRow = false;
        if (text.isEmpty()) {
            showRow = true;
        } else {
            for (int j = 0; j < 8; ++j) {
                auto* item = m_tableWidget->item(i, j);
                if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                    showRow = true;
                    break;
                }
            }
        }
        m_tableWidget->setRowHidden(i, !showRow);
    }
}

void DbViewerWindow::onTableSelectionChanged()
{
    int currentRow = m_tableWidget->currentRow();
    if (currentRow < 0) {
        clearForm();
        return;
    }

    auto* nameItem = m_tableWidget->item(currentRow, 0);
    if (!nameItem) return;

    m_selectedRewardId = nameItem->data(Qt::UserRole).toString();
    m_selectedEffectIndex = nameItem->data(Qt::UserRole + 1).toInt();

    Reward* r = findRewardById(m_selectedRewardId);
    if (r && m_selectedEffectIndex >= 0 && m_selectedEffectIndex < r->effects.size()) {
        loadRewardToForm(*r, m_selectedEffectIndex);
    } else {
        clearForm();
    }
}

void DbViewerWindow::clearForm()
{
    m_rewardIdEdit->clear();
    m_rewardNameEdit->clear();
    m_rewardCostSpin->setValue(0);
    m_rewardCooldownSpin->setValue(0);
    m_rewardModeCombo->setCurrentIndex(0);
    m_rewardEnabledCheck->setChecked(true);

    m_effectTypeCombo->setCurrentIndex(0);
    m_filePathEdit->clear();
    m_audioPathEdit->clear();
    m_durationSpin->setValue(5);
    m_volumeSpin->setValue(80);
    m_scaleSpin->setValue(100);
    m_textEdit->clear();
    m_positionPresetCombo->setCurrentIndex(0);
    m_positionXSpin->setValue(0);
    m_positionYSpin->setValue(0);
    m_animationCombo->setCurrentIndex(0);

    m_formGroup->setEnabled(false);
}

void DbViewerWindow::loadRewardToForm(const Reward& r, int effectIndex)
{
    m_formGroup->setEnabled(true);

    m_rewardIdEdit->setText(r.id);
    m_rewardNameEdit->setText(r.name);
    m_rewardCostSpin->setValue(r.cost);
    m_rewardCooldownSpin->setValue(r.cooldown);
    
    int modeIdx = m_rewardModeCombo->findData(r.mode);
    m_rewardModeCombo->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);
    
    m_rewardEnabledCheck->setChecked(r.enabled);

    const auto& eff = r.effects[effectIndex];
    
    int typeIdx = m_effectTypeCombo->findData(eff.type);
    m_effectTypeCombo->setCurrentIndex(typeIdx >= 0 ? typeIdx : 0);

    m_filePathEdit->setText(eff.filePath);
    m_audioPathEdit->setText(eff.audioPath);
    m_durationSpin->setValue(eff.duration);
    m_volumeSpin->setValue(eff.volume);
    m_scaleSpin->setValue(eff.scale);
    m_textEdit->setText(eff.text);

    int presetIdx = m_positionPresetCombo->findData(eff.position.preset);
    m_positionPresetCombo->setCurrentIndex(presetIdx >= 0 ? presetIdx : 0);
    m_positionXSpin->setValue(eff.position.offsetX);
    m_positionYSpin->setValue(eff.position.offsetY);

    int animIdx = m_animationCombo->findData(eff.animation);
    m_animationCombo->setCurrentIndex(animIdx >= 0 ? animIdx : 0);

    // 演出種別に基づくフィールドのグレーアウト
    onEffectTypeChanged(m_effectTypeCombo->currentIndex());
}

void DbViewerWindow::onEffectTypeChanged(int index)
{
    QString type = m_effectTypeCombo->itemData(index).toString();

    bool hasImage = (type == "image" || type == "video");
    bool hasAudio = (type != "text"); // 音声のみ、または画像・動画と音

    m_filePathEdit->setEnabled(hasImage);
    m_filePathSelectBtn->setEnabled(hasImage);
    m_scaleSpin->setEnabled(hasImage);

    m_audioPathEdit->setEnabled(hasAudio);
    m_audioPathSelectBtn->setEnabled(hasAudio);
    m_volumeSpin->setEnabled(hasAudio);

    m_textEdit->setEnabled(type == "text");
    m_positionPresetCombo->setEnabled(type != "sound");
    m_positionXSpin->setEnabled(type != "sound");
    m_positionYSpin->setEnabled(type != "sound");
    m_animationCombo->setEnabled(type != "sound");

    // スタイルシートを更新して無効化時の視覚的変化を適用
    QString enabledStyle = "background-color: #121214; color: #E1E1E6; border: 1px solid #29292E;";
    QString disabledStyle = "background-color: #1D1D22; color: #555555; border: 1px solid #29292E;";

    m_filePathEdit->setStyleSheet(hasImage ? enabledStyle : disabledStyle);
    m_audioPathEdit->setStyleSheet(hasAudio ? enabledStyle : disabledStyle);
    m_textEdit->setStyleSheet((type == "text") ? enabledStyle : disabledStyle);
}

void DbViewerWindow::selectFilePath()
{
    QString type = m_effectTypeCombo->currentData().toString();
    QString filter = "アセットファイル (*.png *.jpg *.jpeg *.gif *.webp *.mp4 *.webm)";
    if (type == "video") {
        filter = "動画ファイル (*.mp4 *.webm)";
    } else if (type == "image") {
        filter = "画像ファイル (*.png *.jpg *.jpeg *.gif *.webp)";
    }

    QString path = QFileDialog::getOpenFileName(this, "アセットファイルを選択", "", filter);
    if (!path.isEmpty()) {
        m_filePathEdit->setText(path);
    }
}

void DbViewerWindow::selectAudioPath()
{
    QString path = QFileDialog::getOpenFileName(this, "効果音ファイルを選択", "", "オーディオファイル (*.mp3 *.wav *.ogg *.aac *.m4a)");
    if (!path.isEmpty()) {
        m_audioPathEdit->setText(path);
    }
}

void DbViewerWindow::onSaveClicked()
{
    if (m_selectedRewardId.isEmpty()) return;

    Reward* r = findRewardById(m_selectedRewardId);
    if (!r) return;

    // 基本設定の更新
    r->name = m_rewardNameEdit->text();
    r->cost = m_rewardCostSpin->value();
    r->cooldown = m_rewardCooldownSpin->value();
    r->mode = m_rewardModeCombo->currentData().toString();
    r->enabled = m_rewardEnabledCheck->isChecked();

    // 演出設定の更新
    if (m_selectedEffectIndex >= 0 && m_selectedEffectIndex < r->effects.size()) {
        auto& eff = r->effects[m_selectedEffectIndex];
        eff.type = m_effectTypeCombo->currentData().toString();
        
        // 音声のみの時はfilePathを空にする安全対策
        if (eff.type == "sound") {
            eff.filePath = "";
        } else {
            eff.filePath = m_filePathEdit->text();
        }

        // テキストのみの時はaudioPathを空にするなどの整備
        if (eff.type == "text") {
            eff.audioPath = "";
        } else {
            eff.audioPath = m_audioPathEdit->text();
        }

        eff.duration = m_durationSpin->value();
        eff.volume = m_volumeSpin->value();
        eff.scale = m_scaleSpin->value();
        eff.text = m_textEdit->text();
        eff.position.preset = m_positionPresetCombo->currentData().toString();
        eff.position.offsetX = m_positionXSpin->value();
        eff.position.offsetY = m_positionYSpin->value();
        eff.animation = m_animationCombo->currentData().toString();
    }

    // データベースへの保存実行
    if (m_db->saveReward(*r)) {
        QMessageBox::information(this, "保存成功", "変更がデータベースへ正常に保存されました。");
        refreshData();
    } else {
        QMessageBox::critical(this, "保存エラー", "データベースへの変更保存に失敗しました。");
    }
}

void DbViewerWindow::onNewRewardClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "新規報酬", "追加する Twitch チャンネルポイント報酬名を入力してください:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    int cost = QInputDialog::getInt(this, "新規報酬", "必要ポイント数（Cost）を入力してください:", 100, 0, 1000000, 50, &ok);
    if (!ok) return;

    Reward newReward;
    newReward.id = QString("custom_%1").arg(QUuid::createUuid().toString().replace("{", "").replace("}", "").left(12));
    newReward.name = name;
    newReward.cost = cost;
    newReward.cooldown = 0;
    newReward.enabled = true;
    newReward.mode = "sequential";
    newReward.allowedRoles << "everyone";

    // デフォルトの演出を1つ追加
    Effect defaultEffect;
    defaultEffect.type = "image";
    defaultEffect.duration = 5;
    defaultEffect.scale = 100;
    defaultEffect.volume = 80;
    defaultEffect.animation = "fade";
    defaultEffect.position.preset = "center";
    newReward.effects.append(defaultEffect);

    if (m_db->saveReward(newReward)) {
        m_selectedRewardId = newReward.id;
        m_selectedEffectIndex = 0;
        QMessageBox::information(this, "追加完了", "新規報酬が正常に作成されました。");
        refreshData();
    } else {
        QMessageBox::critical(this, "エラー", "新規報酬の保存に失敗しました。");
    }
}

void DbViewerWindow::onAddEffectClicked()
{
    if (m_selectedRewardId.isEmpty()) return;

    Reward* r = findRewardById(m_selectedRewardId);
    if (!r) return;

    Effect newEffect;
    newEffect.type = "sound"; // 追加時は扱いやすい効果音のみをデフォルトにする
    newEffect.duration = 3;
    newEffect.scale = 100;
    newEffect.volume = 80;
    newEffect.animation = "fade";
    newEffect.position.preset = "center";
    
    r->effects.append(newEffect);

    if (m_db->saveReward(*r)) {
        m_selectedEffectIndex = r->effects.size() - 1;
        QMessageBox::information(this, "追加完了", "報酬に新しい演出行を追加しました。");
        refreshData();
    } else {
        QMessageBox::critical(this, "エラー", "演出の追加に失敗しました。");
    }
}

void DbViewerWindow::onDeleteEffectClicked()
{
    if (m_selectedRewardId.isEmpty() || m_selectedEffectIndex < 0) return;

    Reward* r = findRewardById(m_selectedRewardId);
    if (!r) return;

    auto result = QMessageBox::question(this, "演出の削除確認", 
        QString("選択された「演出 [%1]」を削除しますか？\n(注意: これが報酬に唯一の演出だった場合、報酬そのものが削除されます。)")
        .arg(m_selectedEffectIndex), QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes) return;

    r->effects.removeAt(m_selectedEffectIndex);

    if (r->effects.isEmpty()) {
        // 演出が0個になったので報酬ごと削除
        if (m_db->deleteReward(r->id)) {
            m_selectedRewardId = "";
            m_selectedEffectIndex = -1;
            QMessageBox::information(this, "削除完了", "すべての演出が削除されたため、報酬情報を削除しました。");
            refreshData();
        }
    } else {
        if (m_db->saveReward(*r)) {
            m_selectedEffectIndex = qMax(0, m_selectedEffectIndex - 1);
            QMessageBox::information(this, "削除完了", "選択演出を正常に削除しました。");
            refreshData();
        }
    }
}

void DbViewerWindow::onDeleteRewardClicked()
{
    if (m_selectedRewardId.isEmpty()) return;

    Reward* r = findRewardById(m_selectedRewardId);
    if (!r) return;

    auto result = QMessageBox::question(this, "報酬の削除確認", 
        QString("報酬「%1」をデータベースから完全に削除しますか？\n(この操作は取り消せません。演出もすべて削除されます。)")
        .arg(r->name), QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes) return;

    if (m_db->deleteReward(r->id)) {
        m_selectedRewardId = "";
        m_selectedEffectIndex = -1;
        QMessageBox::information(this, "削除完了", "報酬データを完全に削除しました。");
        refreshData();
    } else {
        QMessageBox::critical(this, "エラー", "報酬データの削除に失敗しました。");
    }
}

Reward* DbViewerWindow::findRewardById(const QString& id)
{
    for (auto& r : m_rewards) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

void DbViewerWindow::refreshLogData()
{
    m_logTableWidget->setRowCount(0);
    QList<UsageLogEntry> logs = m_db->getUsageLogs();

    int rowCount = 0;
    for (const auto& log : logs) {
        m_logTableWidget->insertRow(rowCount);

        // Column 0: ID
        auto* idItem = new QTableWidgetItem(QString::number(log.id));
        idItem->setTextAlignment(Qt::AlignCenter);
        idItem->setForeground(QBrush(QColor("#A9A9B2")));
        m_logTableWidget->setItem(rowCount, 0, idItem);

        // Column 1: 報酬名
        auto* nameItem = new QTableWidgetItem(log.rewardName);
        nameItem->setForeground(QBrush(QColor("#FFFFFF")));
        nameItem->setFont(QFont("Arial", 9, QFont::Bold));
        m_logTableWidget->setItem(rowCount, 1, nameItem);

        // Column 2: ユーザー名
        auto* userItem = new QTableWidgetItem(log.username);
        userItem->setForeground(QBrush(QColor("#2196F3")));
        m_logTableWidget->setItem(rowCount, 2, userItem);

        // Column 3: 使用日時
        auto* timeItem = new QTableWidgetItem(log.timestamp);
        timeItem->setForeground(QBrush(QColor("#4CAF50")));
        m_logTableWidget->setItem(rowCount, 3, timeItem);

        rowCount++;
    }

    m_logTableWidget->resizeColumnToContents(0);
    m_logTableWidget->resizeColumnToContents(2);
    m_logTableWidget->resizeColumnToContents(3);
    m_logTableWidget->setColumnWidth(1, 250);

    // フィルタの適用
    onLogSearchTextChanged(m_logSearchEdit->text());
}

void DbViewerWindow::onLogSearchTextChanged(const QString& text)
{
    for (int i = 0; i < m_logTableWidget->rowCount(); ++i) {
        bool showRow = false;
        if (text.isEmpty()) {
            showRow = true;
        } else {
            auto* nameItem = m_logTableWidget->item(i, 1);
            auto* userItem = m_logTableWidget->item(i, 2);
            if ((nameItem && nameItem->text().contains(text, Qt::CaseInsensitive)) ||
                (userItem && userItem->text().contains(text, Qt::CaseInsensitive))) {
                showRow = true;
            }
        }
        m_logTableWidget->setRowHidden(i, !showRow);
    }
}

void DbViewerWindow::updateDbSizeDisplay()
{
    QString dbPath = m_db->getDatabasePath();
    if (dbPath.isEmpty()) {
        m_dbSizeLabel->setText("DBファイルサイズ: 不明");
        return;
    }
    
    QFileInfo fileInfo(dbPath);
    if (!fileInfo.exists()) {
        m_dbSizeLabel->setText("DBファイルサイズ: 存在しません");
        return;
    }
    
    double sizeInMb = (double)fileInfo.size() / (1024.0 * 1024.0);
    m_dbSizeLabel->setText(QString("DBファイルサイズ: %1 MB").arg(sizeInMb, 0, 'f', 2));
}

void DbViewerWindow::onCleanupClicked()
{
    QString period = m_cleanupPeriodCombo->currentData().toString();
    QString confirmMsg;
    QString targetDateStr;

    if (period == "all") {
        confirmMsg = "本当に『すべての統計ログデータ』を削除しますか？\n(注意: 報酬の設定データ自体は削除されません。)";
    } else if (period == "1week") {
        confirmMsg = "1週間以上前の統計ログデータをクリーンアップしますか？";
        targetDateStr = QDateTime::currentDateTime().addDays(-7).toString("yyyy-MM-dd");
    } else if (period == "1month") {
        confirmMsg = "1ヶ月以上前の統計ログデータをクリーンアップしますか？";
        targetDateStr = QDateTime::currentDateTime().addMonths(-1).toString("yyyy-MM-dd");
    } else if (period == "3months") {
        confirmMsg = "3ヶ月以上前の統計ログデータをクリーンアップしますか？";
        targetDateStr = QDateTime::currentDateTime().addMonths(-3).toString("yyyy-MM-dd");
    }

    auto result = QMessageBox::question(this, "クリーンアップの確認", confirmMsg, QMessageBox::Yes | QMessageBox::No);
    if (result != QMessageBox::Yes) return;

    QString dbPath = m_db->getDatabasePath();
    qint64 beforeSize = QFileInfo(dbPath).size();

    bool success = false;
    if (period == "all") {
        success = m_db->clearUsageLogs();
    } else {
        success = m_db->deleteUsageLogsBefore(targetDateStr);
    }

    if (!success) {
        QMessageBox::critical(this, "クリーンアップエラー", "ログデータの削除に失敗しました。");
        return;
    }

    // SQLiteの最適化・サイズ圧縮
    m_db->vacuum();

    qint64 afterSize = QFileInfo(dbPath).size();
    double savedKb = (double)(beforeSize - afterSize) / 1024.0;

    QString infoMsg = "統計ログのクリーンアップが正常に完了しました。\n";
    if (savedKb > 0) {
        infoMsg += QString("データベースファイルが %1 KB 軽量化されました！").arg(savedKb, 0, 'f', 1);
    } else {
        infoMsg += "データベースファイルは既に最適化されています。";
    }

    QMessageBox::information(this, "クリーンアップ完了", infoMsg);

    refreshLogData();
    updateDbSizeDisplay();
}
