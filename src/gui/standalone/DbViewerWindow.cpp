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
#include <QEvent>
#include <QFileInfo>
#include <QDateTime>

DbViewerWindow::DbViewerWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_db(new Database(this))
    , m_selectedRewardId("")
    , m_selectedEffectIndex(-1)
{
    setWindowTitle(tr("Twitch Overlay - stand-alone Database Viewer & Editor"));
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
        QMessageBox::critical(this, tr("接続エラー"), 
            tr("データベースファイルのオープンに失敗しました。\nパス: %1").arg(dbPath));
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

    auto* splitter = new QSplitter(Qt::Horizontal, editorTab);
    editorLayout->addWidget(splitter);

    // 左ペイン: 一覧 ＆ 検索
    auto* leftContainer = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto* searchLayout = new QHBoxLayout();
    m_searchLabel = new QLabel(tr("🔍 検索:"), leftContainer);
    m_searchLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #FFFFFF;");
    searchLayout->addWidget(m_searchLabel);

    m_searchEdit = new QLineEdit(leftContainer);
    m_searchEdit->setPlaceholderText(tr("報酬名、ファイルパス、テキスト等で絞り込み..."));
    m_searchEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DbViewerWindow::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit);

    m_refreshButton = new QPushButton(tr("🔄 更新"), leftContainer);
    m_refreshButton->setStyleSheet("background-color: #29292E; color: #FFFFFF; font-weight: bold; border: 1px solid #35353B; border-radius: 4px; padding: 6px 12px;");
    connect(m_refreshButton, &QPushButton::clicked, this, &DbViewerWindow::refreshData);
    searchLayout->addWidget(m_refreshButton);

    leftLayout->addLayout(searchLayout);

    m_tableWidget = new QTableWidget(leftContainer);
    m_tableWidget->setColumnCount(8);
    QStringList headers;
    headers << tr("🎁 報酬名 (ポイント数)") << tr("🔢 演出") << tr("🎨 種類") << tr("🖼️ 画像/動画アセット") << tr("🔊 効果音") << tr("⏱️ 秒") << tr("🔉 音量") << tr("💬 テキスト");
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

    auto* leftBottomLayout = new QHBoxLayout();
    m_newRewardButton = new QPushButton(tr("➕ 新規報酬を追加"), leftContainer);
    m_newRewardButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; border: none; border-radius: 4px; padding: 8px 15px;");
    connect(m_newRewardButton, &QPushButton::clicked, this, &DbViewerWindow::onNewRewardClicked);
    leftBottomLayout->addWidget(m_newRewardButton);
    leftBottomLayout->addStretch();
    leftLayout->addLayout(leftBottomLayout);

    // 右ペイン: 編集フォーム
    auto* rightContainer = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_formGroup = new QGroupBox(tr("🛠️ 演出と報酬の編集"), rightContainer);
    m_formGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #FFFFFF; }");
    auto* groupLayout = new QVBoxLayout(m_formGroup);

    auto* scrollArea = new QScrollArea(m_formGroup);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background-color: transparent;");
    
    auto* scrollContent = new QWidget(scrollArea);
    auto* formLayout = new QFormLayout(scrollContent);
    formLayout->setContentsMargins(10, 10, 10, 10);
    formLayout->setSpacing(12);

    auto* section1 = new QLabel(tr("📋 【1】 報酬基本設定"), scrollContent);
    section1->setObjectName("section1");
    section1->setStyleSheet("font-weight: bold; color: #2196F3; font-size: 13px; margin-top: 5px;");
    formLayout->addRow(section1);

    m_rewardIdEdit = new QLineEdit(scrollContent);
    m_rewardIdEdit->setReadOnly(true);
    m_rewardIdEdit->setStyleSheet("background-color: #1D1D22; color: #888888; border: 1px solid #29292E;");
    formLayout->addRow(tr("報酬 ID (読取専用):"), m_rewardIdEdit);

    m_rewardNameEdit = new QLineEdit(scrollContent);
    formLayout->addRow(tr("🎁 報酬名:"), m_rewardNameEdit);

    m_rewardCostSpin = new QSpinBox(scrollContent);
    m_rewardCostSpin->setRange(0, 1000000);
    m_rewardCostSpin->setSingleStep(100);
    formLayout->addRow(tr("ポイント数 (Cost):"), m_rewardCostSpin);

    m_rewardCooldownSpin = new QSpinBox(scrollContent);
    m_rewardCooldownSpin->setRange(0, 86400);
    formLayout->addRow(tr("クールダウン秒数:"), m_rewardCooldownSpin);

    m_rewardModeCombo = new QComboBox(scrollContent);
    m_rewardModeCombo->addItem(tr("全て再生 (sequential)"), "sequential");
    m_rewardModeCombo->addItem(tr("ランダムで1つ再生 (random)"), "random");
    formLayout->addRow(tr("演出再生モード:"), m_rewardModeCombo);

    m_rewardEnabledCheck = new QCheckBox(tr("この報酬を有効にする"), scrollContent);
    formLayout->addRow(tr("ステータス:"), m_rewardEnabledCheck);

    auto* line = new QFrame(scrollContent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("background-color: #29292E;");
    formLayout->addRow(line);

    auto* section2 = new QLabel(tr("🎨 【2】 演出詳細設定"), scrollContent);
    section2->setObjectName("section2");
    section2->setStyleSheet("font-weight: bold; color: #4CAF50; font-size: 13px; margin-top: 5px;");
    formLayout->addRow(section2);

    m_effectTypeCombo = new QComboBox(scrollContent);
    m_effectTypeCombo->addItem(tr("🖼️ 画像"), "image");
    m_effectTypeCombo->addItem(tr("🎥 動画"), "video");
    m_effectTypeCombo->addItem(tr("🔊 音声のみ"), "sound");
    m_effectTypeCombo->addItem(tr("💬 テキストのみ"), "text");
    connect(m_effectTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DbViewerWindow::onEffectTypeChanged);
    formLayout->addRow(tr("演出の種類:"), m_effectTypeCombo);

    auto* pathLayout = new QHBoxLayout();
    m_filePathEdit = new QLineEdit(scrollContent);
    pathLayout->addWidget(m_filePathEdit);
    m_filePathSelectBtn = new QPushButton(tr("📁 参照"), scrollContent);
    m_filePathSelectBtn->setStyleSheet("padding: 4px 10px; background-color: #29292E;");
    connect(m_filePathSelectBtn, &QPushButton::clicked, this, &DbViewerWindow::selectFilePath);
    pathLayout->addWidget(m_filePathSelectBtn);
    formLayout->addRow(tr("画像・動画ファイルパス:"), pathLayout);

    auto* audioLayout = new QHBoxLayout();
    m_audioPathEdit = new QLineEdit(scrollContent);
    audioLayout->addWidget(m_audioPathEdit);
    m_audioPathSelectBtn = new QPushButton(tr("📁 参照"), scrollContent);
    m_audioPathSelectBtn->setStyleSheet("padding: 4px 10px; background-color: #29292E;");
    connect(m_audioPathSelectBtn, &QPushButton::clicked, this, &DbViewerWindow::selectAudioPath);
    pathLayout->addWidget(m_audioPathSelectBtn);
    formLayout->addRow(tr("効果音ファイルパス:"), audioLayout);

    m_durationSpin = new QSpinBox(scrollContent);
    m_durationSpin->setRange(1, 300);
    formLayout->addRow(tr("演出表示時間 (秒):"), m_durationSpin);

    m_volumeSpin = new QSpinBox(scrollContent);
    m_volumeSpin->setRange(0, 100);
    formLayout->addRow(tr("音量 (%):"), m_volumeSpin);

    m_scaleSpin = new QSpinBox(scrollContent);
    m_scaleSpin->setRange(1, 100);
    formLayout->addRow(tr("画像・動画縮尺 (%):"), m_scaleSpin);

    m_textEdit = new QLineEdit(scrollContent);
    formLayout->addRow(tr("表示テキスト:"), m_textEdit);

    m_positionPresetCombo = new QComboBox(scrollContent);
    m_positionPresetCombo->addItem(tr("中央 (center)"), "center");
    m_positionPresetCombo->addItem(tr("左上 (top_left)"), "top_left");
    m_positionPresetCombo->addItem(tr("右上 (top_right)"), "top_right");
    m_positionPresetCombo->addItem(tr("左下 (bottom_left)"), "bottom_left");
    m_positionPresetCombo->addItem(tr("右下 (bottom_right)"), "bottom_right");
    m_positionPresetCombo->addItem(tr("カスタム座標"), "custom");
    formLayout->addRow(tr("表示位置プリセット:"), m_positionPresetCombo);

    auto* coordLayout = new QHBoxLayout();
    m_centerCoordLabel = new QLabel(tr("Xオフセット:"));
    coordLayout->addWidget(m_centerCoordLabel);
    m_positionXSpin = new QSpinBox(scrollContent);
    m_positionXSpin->setRange(-2000, 2000);
    coordLayout->addWidget(m_positionXSpin);
    
    auto* coordLabelY = new QLabel(tr("Yオフセット:"));
    coordLabelY->setObjectName("coordLabelY");
    coordLayout->addWidget(coordLabelY);
    m_positionYSpin = new QSpinBox(scrollContent);
    m_positionYSpin->setRange(-2000, 2000);
    coordLayout->addWidget(m_positionYSpin);
    formLayout->addRow(tr("カスタム位置座標:"), coordLayout);

    m_animationCombo = new QComboBox(scrollContent);
    m_animationCombo->addItem(tr("フェードイン・アウト (fade)"), "fade");
    m_animationCombo->addItem(tr("スライドイン (slide)"), "slide");
    m_animationCombo->addItem(tr("バウンス (bounce)"), "bounce");
    m_animationCombo->addItem(tr("アニメーションなし (none)"), "none");
    formLayout->addRow(tr("演出効果アニメーション:"), m_animationCombo);

    scrollContent->setLayout(formLayout);
    scrollArea->setWidget(scrollContent);
    groupLayout->addWidget(scrollArea);

    auto* btnLayout = new QHBoxLayout();
    m_saveButton = new QPushButton(tr("💾 変更をデータベースに保存"), m_formGroup);
    m_saveButton->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 10px; border: none; border-radius: 4px;");
    connect(m_saveButton, &QPushButton::clicked, this, &DbViewerWindow::onSaveClicked);
    btnLayout->addWidget(m_saveButton);
    groupLayout->addLayout(btnLayout);

    auto* subBtnLayout = new QHBoxLayout();
    m_addEffectButton = new QPushButton(tr("➕ 演出追加"), m_formGroup);
    m_addEffectButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 6px; border: none; border-radius: 4px;");
    connect(m_addEffectButton, &QPushButton::clicked, this, &DbViewerWindow::onAddEffectClicked);
    subBtnLayout->addWidget(m_addEffectButton);

    m_deleteEffectButton = new QPushButton(tr("❌ 選択演出を削除"), m_formGroup);
    m_deleteEffectButton->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; padding: 6px; border: none; border-radius: 4px;");
    connect(m_deleteEffectButton, &QPushButton::clicked, this, &DbViewerWindow::onDeleteEffectClicked);
    subBtnLayout->addWidget(m_deleteEffectButton);

    m_deleteRewardButton = new QPushButton(tr("🗑️ 報酬ごと削除"), m_formGroup);
    m_deleteRewardButton->setStyleSheet("background-color: #F44336; color: white; font-weight: bold; padding: 6px; border: none; border-radius: 4px;");
    connect(m_deleteRewardButton, &QPushButton::clicked, this, &DbViewerWindow::onDeleteRewardClicked);
    subBtnLayout->addWidget(m_deleteRewardButton);

    groupLayout->addLayout(subBtnLayout);
    rightLayout->addWidget(m_formGroup);

    splitter->addWidget(leftContainer);
    splitter->addWidget(rightContainer);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    m_tabWidget->addTab(editorTab, tr("🎁 報酬と演出の編集"));

    // ==========================================
    // タブ2: 📊 統計ログ管理とDB軽量化
    // ==========================================
    auto* logsTab = new QWidget(m_tabWidget);
    auto* logsLayout = new QVBoxLayout(logsTab);
    logsLayout->setContentsMargins(15, 15, 15, 15);
    logsLayout->setSpacing(12);

    auto* topLogLayout = new QHBoxLayout();
    m_logSearchLabel = new QLabel(tr("🔍 ログ検索:"), logsTab);
    m_logSearchLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #FFFFFF;");
    topLogLayout->addWidget(m_logSearchLabel);

    m_logSearchEdit = new QLineEdit(logsTab);
    m_logSearchEdit->setPlaceholderText(tr("ユーザー名、報酬名等で絞り込み..."));
    m_logSearchEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    connect(m_logSearchEdit, &QLineEdit::textChanged, this, &DbViewerWindow::onLogSearchTextChanged);
    topLogLayout->addWidget(m_logSearchEdit);

    auto* refreshLogBtn = new QPushButton(tr("🔄 ログ更新"), logsTab);
    refreshLogBtn->setStyleSheet("background-color: #29292E; color: #FFFFFF; font-weight: bold; border: 1px solid #35353B; border-radius: 4px; padding: 6px 12px;");
    connect(refreshLogBtn, &QPushButton::clicked, this, &DbViewerWindow::refreshLogData);
    topLogLayout->addWidget(refreshLogBtn);

    topLogLayout->addSpacing(20);

    m_dbSizeLabel = new QLabel(tr("DBファイルサイズ: -"), logsTab);
    m_dbSizeLabel->setStyleSheet("font-weight: bold; color: #2196F3; font-size: 13px; background-color: #1D1D22; border: 1px solid #29292E; border-radius: 4px; padding: 6px 12px;");
    topLogLayout->addWidget(m_dbSizeLabel);

    logsLayout->addLayout(topLogLayout);

    m_logTableWidget = new QTableWidget(logsTab);
    m_logTableWidget->setColumnCount(4);
    QStringList logHeaders;
    logHeaders << tr("ID") << tr("🎁 報酬名") << tr("👤 ユーザー名") << tr("⏱️ 使用日時");
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

    m_cleanupGroup = new QGroupBox(tr("🗑️ 統計ログのクリーンアップ & データベース最適化"), logsTab);
    m_cleanupGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #FFFFFF; padding: 15px; }");
    auto* cleanupLayout = new QHBoxLayout(m_cleanupGroup);
    cleanupLayout->setContentsMargins(15, 15, 15, 15);
    cleanupLayout->setSpacing(12);

    m_cleanupPeriodLabel = new QLabel(tr("クリーンアップ対象の期間:"), m_cleanupGroup);
    cleanupLayout->addWidget(m_cleanupPeriodLabel);

    m_cleanupPeriodCombo = new QComboBox(m_cleanupGroup);
    m_cleanupPeriodCombo->addItem(tr("全てのログデータを削除"), "all");
    m_cleanupPeriodCombo->addItem(tr("1週間以上前のログデータを削除"), "1week");
    m_cleanupPeriodCombo->addItem(tr("1ヶ月以上前のログデータを削除"), "1month");
    m_cleanupPeriodCombo->addItem(tr("3ヶ月以上前のログデータを削除"), "3months");
    m_cleanupPeriodCombo->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px; min-width: 250px;");
    cleanupLayout->addWidget(m_cleanupPeriodCombo);

    m_cleanupButton = new QPushButton(tr("🚨 クリーンアップ & DB軽量化 (VACUUM) を実行"), m_cleanupGroup);
    m_cleanupButton->setStyleSheet("background-color: #D32F2F; color: white; font-weight: bold; padding: 8px 16px; border: none; border-radius: 4px;");
    connect(m_cleanupButton, &QPushButton::clicked, this, &DbViewerWindow::onCleanupClicked);
    cleanupLayout->addWidget(m_cleanupButton);

    cleanupLayout->addStretch();
    logsLayout->addWidget(m_cleanupGroup);

    m_tabWidget->addTab(logsTab, tr("📊 統計ログ管理とDB軽量化"));

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
    )");
}

void DbViewerWindow::refreshData()
{
    m_rewards.clear();
    m_tableWidget->setRowCount(0);
    clearForm();

    if (!m_db->loadRewards(m_rewards)) {
        QMessageBox::critical(this, tr("ロードエラー"), tr("報酬データの読み込みに失敗しました。"));
        return;
    }

    onSearchTextChanged(m_searchEdit->text());
}

void DbViewerWindow::onSearchTextChanged(const QString& text)
{
    m_tableWidget->setRowCount(0);
    m_selectedRewardId = "";
    m_selectedEffectIndex = -1;
    clearForm();

    QString query = text.trimmed();

    int row = 0;
    for (const auto& r : m_rewards) {
        if (!query.isEmpty() && !r.name.contains(query, Qt::CaseInsensitive)) {
            bool matchedEffect = false;
            for (const auto& eff : r.effects) {
                if (eff.filePath.contains(query, Qt::CaseInsensitive) || 
                    eff.audioPath.contains(query, Qt::CaseInsensitive) || 
                    eff.text.contains(query, Qt::CaseInsensitive)) {
                    matchedEffect = true;
                    break;
                }
            }
            if (!matchedEffect) continue;
        }

        for (int i = 0; i < r.effects.size(); ++i) {
            const auto& eff = r.effects[i];
            m_tableWidget->insertRow(row);

            auto* nameItem = new QTableWidgetItem(QString("%1 (%2pt)").arg(r.name).arg(r.cost));
            nameItem->setData(Qt::UserRole, r.id);
            nameItem->setData(Qt::UserRole + 1, i); // 演出のインデックス番号

            auto* effIdxItem = new QTableWidgetItem(QString(tr("演出 %1")).arg(i + 1));
            effIdxItem->setTextAlignment(Qt::AlignCenter);

            auto* typeItem = new QTableWidgetItem(eff.type);
            typeItem->setTextAlignment(Qt::AlignCenter);

            auto* fileItem = new QTableWidgetItem(QFileInfo(eff.filePath).fileName());
            auto* audioItem = new QTableWidgetItem(QFileInfo(eff.audioPath).fileName());
            
            auto* durItem = new QTableWidgetItem(QString(tr("%1 秒")).arg(eff.duration));
            durItem->setTextAlignment(Qt::AlignCenter);
            
            auto* volItem = new QTableWidgetItem(QString("%1%").arg(eff.volume));
            volItem->setTextAlignment(Qt::AlignCenter);

            auto* textItem = new QTableWidgetItem(eff.text);

            m_tableWidget->setItem(row, 0, nameItem);
            m_tableWidget->setItem(row, 1, effIdxItem);
            m_tableWidget->setItem(row, 2, typeItem);
            m_tableWidget->setItem(row, 3, fileItem);
            m_tableWidget->setItem(row, 4, audioItem);
            m_tableWidget->setItem(row, 5, durItem);
            m_tableWidget->setItem(row, 6, volItem);
            m_tableWidget->setItem(row, 7, textItem);
            row++;
        }
    }
}

void DbViewerWindow::onTableSelectionChanged()
{
    QList<QTableWidgetItem*> selected = m_tableWidget->selectedItems();
    if (selected.isEmpty()) return;

    int curRow = selected.first()->row();
    QTableWidgetItem* nameItem = m_tableWidget->item(curRow, 0);

    if (!nameItem) return;

    m_selectedRewardId = nameItem->data(Qt::UserRole).toString();
    m_selectedEffectIndex = nameItem->data(Qt::UserRole + 1).toInt();

    Reward* r = findRewardById(m_selectedRewardId);
    if (r) {
        loadRewardToForm(*r, m_selectedEffectIndex);
    }
}

void DbViewerWindow::clearForm()
{
    m_rewardIdEdit->clear();
    m_rewardNameEdit->clear();
    m_rewardCostSpin->setValue(500);
    m_rewardCooldownSpin->setValue(0);
    m_rewardModeCombo->setCurrentIndex(0);
    m_rewardEnabledCheck->setChecked(true);

    m_effectTypeCombo->setCurrentIndex(0);
    m_filePathEdit->clear();
    m_audioPathEdit->clear();
    m_durationSpin->setValue(5);
    m_volumeSpin->setValue(100);
    m_scaleSpin->setValue(100);
    m_textEdit->clear();
    m_positionPresetCombo->setCurrentIndex(0);
    m_positionXSpin->setValue(0);
    m_positionYSpin->setValue(0);
    m_animationCombo->setCurrentIndex(0);

    m_saveButton->setEnabled(false);
    m_addEffectButton->setEnabled(false);
    m_deleteEffectButton->setEnabled(false);
    m_deleteRewardButton->setEnabled(false);
}

void DbViewerWindow::loadRewardToForm(const Reward& r, int effectIndex)
{
    m_rewardIdEdit->setText(r.id);
    m_rewardNameEdit->setText(r.name);
    m_rewardCostSpin->setValue(r.cost);
    m_rewardCooldownSpin->setValue(r.cooldown);
    
    int modeIdx = m_rewardModeCombo->findData(r.mode);
    if (modeIdx != -1) m_rewardModeCombo->setCurrentIndex(modeIdx);
    
    m_rewardEnabledCheck->setChecked(r.enabled);

    if (effectIndex >= 0 && effectIndex < r.effects.size()) {
        const auto& eff = r.effects[effectIndex];
        
        int typeIdx = m_effectTypeCombo->findData(eff.type);
        if (typeIdx != -1) m_effectTypeCombo->setCurrentIndex(typeIdx);

        m_filePathEdit->setText(eff.filePath);
        m_audioPathEdit->setText(eff.audioPath);
        m_durationSpin->setValue(eff.duration);
        m_volumeSpin->setValue(eff.volume);
        m_scaleSpin->setValue(eff.scale);
        m_textEdit->setText(eff.text);

        int posIdx = m_positionPresetCombo->findData(eff.position.preset);
        if (posIdx != -1) m_positionPresetCombo->setCurrentIndex(posIdx);

        m_positionXSpin->setValue(eff.position.offsetX);
        m_positionYSpin->setValue(eff.position.offsetY);

        int animIdx = m_animationCombo->findData(eff.animation);
        if (animIdx != -1) m_animationCombo->setCurrentIndex(animIdx);

        onEffectTypeChanged(m_effectTypeCombo->currentIndex());
    }

    m_saveButton->setEnabled(true);
    m_addEffectButton->setEnabled(true);
    m_deleteEffectButton->setEnabled(r.effects.size() > 1);
    m_deleteRewardButton->setEnabled(true);
}

Reward* DbViewerWindow::findRewardById(const QString& id)
{
    for (auto& r : m_rewards) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

void DbViewerWindow::onEffectTypeChanged(int index)
{
    QString type = m_effectTypeCombo->itemData(index).toString();
    if (type == "sound") {
        m_filePathEdit->setEnabled(false);
        m_filePathSelectBtn->setEnabled(false);
        m_scaleSpin->setEnabled(false);
        m_positionPresetCombo->setEnabled(false);
        m_positionXSpin->setEnabled(false);
        m_positionYSpin->setEnabled(false);
    } else if (type == "text") {
        m_filePathEdit->setEnabled(false);
        m_filePathSelectBtn->setEnabled(false);
        m_scaleSpin->setEnabled(false);
        m_positionPresetCombo->setEnabled(true);
        m_positionXSpin->setEnabled(m_positionPresetCombo->currentData().toString() == "custom");
        m_positionYSpin->setEnabled(m_positionPresetCombo->currentData().toString() == "custom");
    } else {
        m_filePathEdit->setEnabled(true);
        m_filePathSelectBtn->setEnabled(true);
        m_scaleSpin->setEnabled(true);
        m_positionPresetCombo->setEnabled(true);
        m_positionXSpin->setEnabled(m_positionPresetCombo->currentData().toString() == "custom");
        m_positionYSpin->setEnabled(m_positionPresetCombo->currentData().toString() == "custom");
    }
}

void DbViewerWindow::selectFilePath()
{
    QString filter = tr("Images/Videos (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.webm *.mp4);;All Files (*)");
    QString path = QFileDialog::getOpenFileName(this, tr("アセットファイルを選択"), m_filePathEdit->text(), filter);
    if (!path.isEmpty()) {
        m_filePathEdit->setText(QDir::toNativeSeparators(path));
    }
}

void DbViewerWindow::selectAudioPath()
{
    QString filter = tr("Audio Files (*.mp3 *.wav *.ogg *.aac *.m4a);;All Files (*)");
    QString path = QFileDialog::getOpenFileName(this, tr("効果音ファイルを選択"), m_audioPathEdit->text(), filter);
    if (!path.isEmpty()) {
        m_audioPathEdit->setText(QDir::toNativeSeparators(path));
    }
}

void DbViewerWindow::onNewRewardClicked()
{
    QString name = QInputDialog::getText(this, tr("新規報酬の登録"), tr("追加する報酬の名前を入力してください:"));
    if (name.trimmed().isEmpty()) return;

    Reward r;
    r.id = QUuid::createUuid().toString().replace("{", "").replace("}", "");
    r.name = name.trimmed();
    r.cost = 500;
    r.cooldown = 0;
    r.enabled = true;
    r.mode = "sequential";
    r.allowedRoles = {"everyone"};

    Effect def;
    def.type = "image";
    def.duration = 5;
    def.scale = 100;
    def.volume = 100;
    def.position.preset = "center";
    def.position.offsetX = 0;
    def.position.offsetY = 0;
    def.animation = "fade";
    r.effects.append(def);

    if (m_db->saveReward(r)) {
        QMessageBox::information(this, tr("成功"), tr("新規報酬を登録しました。"));
        refreshData();

        for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
            if (m_tableWidget->item(i, 0)->data(Qt::UserRole).toString() == r.id) {
                m_tableWidget->setCurrentCell(i, 0);
                break;
            }
        }
    } else {
        QMessageBox::critical(this, tr("エラー"), tr("報酬の追加に失敗しました。"));
    }
}

void DbViewerWindow::onAddEffectClicked()
{
    if (m_selectedRewardId.isEmpty()) return;

    Reward* r = findRewardById(m_selectedRewardId);
    if (!r) return;

    Effect def;
    def.type = "image";
    def.duration = 5;
    def.scale = 100;
    def.volume = 100;
    def.position.preset = "center";
    def.position.offsetX = 0;
    def.position.offsetY = 0;
    def.animation = "fade";
    r->effects.append(def);

    if (m_db->saveReward(*r)) {
        QMessageBox::information(this, tr("成功"), tr("新しい演出効果を追加しました。"));
        int lastIdx = r->effects.size() - 1;
        refreshData();

        for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
            if (m_tableWidget->item(i, 0)->data(Qt::UserRole).toString() == r->id &&
                m_tableWidget->item(i, 0)->data(Qt::UserRole + 1).toInt() == lastIdx) {
                m_tableWidget->setCurrentCell(i, 0);
                break;
            }
        }
    } else {
        QMessageBox::critical(this, tr("エラー"), tr("演出の追加に失敗しました。"));
    }
}

void DbViewerWindow::onSaveClicked()
{
    if (m_selectedRewardId.isEmpty() || m_selectedEffectIndex < 0) return;

    Reward* r = findRewardById(m_selectedRewardId);
    if (!r || m_selectedEffectIndex >= r->effects.size()) return;

    r->name = m_rewardNameEdit->text().trimmed();
    r->cost = m_rewardCostSpin->value();
    r->cooldown = m_rewardCooldownSpin->value();
    r->mode = m_rewardModeCombo->currentData().toString();
    r->enabled = m_rewardEnabledCheck->isChecked();

    auto& eff = r->effects[m_selectedEffectIndex];
    eff.type = m_effectTypeCombo->currentData().toString();
    eff.filePath = m_filePathEdit->text().trimmed();
    eff.audioPath = m_audioPathEdit->text().trimmed();
    eff.duration = m_durationSpin->value();
    eff.volume = m_volumeSpin->value();
    eff.scale = m_scaleSpin->value();
    eff.text = m_textEdit->text().trimmed();
    eff.position.preset = m_positionPresetCombo->currentData().toString();
    eff.position.offsetX = m_positionXSpin->value();
    eff.position.offsetY = m_positionYSpin->value();
    eff.animation = m_animationCombo->currentData().toString();

    if (r->name.isEmpty()) {
        QMessageBox::warning(this, tr("保存エラー"), tr("報酬名は空にできません。"));
        return;
    }

    if (m_db->saveReward(*r)) {
        QMessageBox::information(this, tr("成功"), tr("データベースの変更を保存しました。"));
        QString lastId = m_selectedRewardId;
        int lastIdx = m_selectedEffectIndex;
        refreshData();

        for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
            if (m_tableWidget->item(i, 0)->data(Qt::UserRole).toString() == lastId &&
                m_tableWidget->item(i, 0)->data(Qt::UserRole + 1).toInt() == lastIdx) {
                m_tableWidget->setCurrentCell(i, 0);
                break;
            }
        }
    } else {
        QMessageBox::critical(this, tr("エラー"), tr("データの保存に失敗しました。"));
    }
}

void DbViewerWindow::onDeleteEffectClicked()
{
    if (m_selectedRewardId.isEmpty() || m_selectedEffectIndex < 0) return;

    Reward* r = findRewardById(m_selectedRewardId);
    if (!r || r->effects.size() <= 1) return;

    auto result = QMessageBox::question(this, tr("演出削除の確認"),
        tr("選択されている演出を削除しますか？"), QMessageBox::Yes | QMessageBox::No);
        
    if (result != QMessageBox::Yes) return;

    r->effects.removeAt(m_selectedEffectIndex);

    if (m_db->saveReward(*r)) {
        QMessageBox::information(this, tr("成功"), tr("演出効果を削除しました。"));
        refreshData();
    } else {
        QMessageBox::critical(this, tr("エラー"), tr("演出の削除に失敗しました。"));
    }
}

void DbViewerWindow::onDeleteRewardClicked()
{
    if (m_selectedRewardId.isEmpty()) return;

    auto result = QMessageBox::question(this, tr("報酬削除の確認"),
        tr("この報酬および付随するすべての演出設定を削除しますか？\nこの操作は取り消せません。"),
        QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes) return;

    if (m_db->deleteReward(m_selectedRewardId)) {
        QMessageBox::information(this, tr("成功"), tr("報酬データを削除しました。"));
        refreshData();
    } else {
        QMessageBox::critical(this, tr("エラー"), tr("報酬データの削除に失敗しました。"));
    }
}

void DbViewerWindow::refreshLogData()
{
    m_logTableWidget->setRowCount(0);
    if (!m_db) return;

    QList<UsageLogEntry> logs = m_db->getUsageLogs();
    QString query = m_logSearchEdit->text().trimmed();

    int row = 0;
    for (const auto& log : logs) {
        int logId = log.id;
        QString rewardName = log.rewardName;
        QString username = log.username;
        QString timestamp = log.timestamp;

        if (!query.isEmpty() && 
            !rewardName.contains(query, Qt::CaseInsensitive) && 
            !username.contains(query, Qt::CaseInsensitive)) {
            continue;
        }

        m_logTableWidget->insertRow(row);
        
        auto* idItem = new QTableWidgetItem(QString::number(logId));
        idItem->setTextAlignment(Qt::AlignCenter);
        
        auto* rewardItem = new QTableWidgetItem(rewardName);
        auto* userItem = new QTableWidgetItem(username);
        
        auto* timeItem = new QTableWidgetItem(timestamp);
        timeItem->setTextAlignment(Qt::AlignCenter);

        m_logTableWidget->setItem(row, 0, idItem);
        m_logTableWidget->setItem(row, 1, rewardItem);
        m_logTableWidget->setItem(row, 2, userItem);
        m_logTableWidget->setItem(row, 3, timeItem);
        row++;
    }
}

void DbViewerWindow::onLogSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
    refreshLogData();
}

void DbViewerWindow::updateDbSizeDisplay()
{
    QString dbPath = m_db->getDatabasePath();
    if (dbPath.isEmpty() || !QFile::exists(dbPath)) {
        m_dbSizeLabel->setText(tr("DBファイルサイズ: -"));
        return;
    }

    qint64 bytes = QFileInfo(dbPath).size();
    double sizeInMb = (double)bytes / (1024.0 * 1024.0);
    m_dbSizeLabel->setText(QString(tr("DBファイルサイズ: %1 MB")).arg(sizeInMb, 0, 'f', 2));
}

void DbViewerWindow::onCleanupClicked()
{
    QString period = m_cleanupPeriodCombo->currentData().toString();
    QString confirmMsg;
    QString targetDateStr;

    if (period == "all") {
        confirmMsg = tr("本当に『すべての統計ログデータ』を削除しますか？\n(注意: 報酬の設定データ自体は削除されません。)");
    } else if (period == "1week") {
        confirmMsg = tr("1週間以上前の統計ログデータをクリーンアップしますか？");
        targetDateStr = QDateTime::currentDateTime().addDays(-7).toString("yyyy-MM-dd");
    } else if (period == "1month") {
        confirmMsg = tr("1ヶ月以上前の統計ログデータをクリーンアップしますか？");
        targetDateStr = QDateTime::currentDateTime().addMonths(-1).toString("yyyy-MM-dd");
    } else if (period == "3months") {
        confirmMsg = tr("3ヶ月以上前の統計ログデータをクリーンアップしますか？");
        targetDateStr = QDateTime::currentDateTime().addMonths(-3).toString("yyyy-MM-dd");
    }

    auto result = QMessageBox::question(this, tr("クリーンアップの確認"), confirmMsg, QMessageBox::Yes | QMessageBox::No);
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
        QMessageBox::critical(this, tr("クリーンアップエラー"), tr("ログデータの削除に失敗しました。"));
        return;
    }

    m_db->vacuum();

    qint64 afterSize = QFileInfo(dbPath).size();
    double savedKb = (double)(beforeSize - afterSize) / 1024.0;

    QString infoMsg = tr("統計ログのクリーンアップが正常に完了しました。\n");
    if (savedKb > 0) {
        infoMsg += QString(tr("データベースファイルが %1 KB 軽量化されました！")).arg(savedKb, 0, 'f', 1);
    } else {
        infoMsg += tr("データベースファイルは既に最適化されています。");
    }

    QMessageBox::information(this, tr("クリーンアップ完了"), infoMsg);

    refreshLogData();
    updateDbSizeDisplay();
}

void DbViewerWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void DbViewerWindow::retranslateUi()
{
    setWindowTitle(tr("Twitch Overlay - stand-alone Database Viewer & Editor"));

    m_searchLabel->setText(tr("🔍 検索:"));
    m_searchEdit->setPlaceholderText(tr("報酬名、ファイルパス、テキスト等で絞り込み..."));
    m_refreshButton->setText(tr("🔄 更新"));

    m_tableWidget->setHorizontalHeaderLabels({
        tr("🎁 報酬名 (ポイント数)"), 
        tr("🔢 演出"), 
        tr("🎨 種類"), 
        tr("🖼️ 画像/動画アセット"), 
        tr("🔊 効果音"), 
        tr("⏱️ 秒"), 
        tr("🔉 音量"), 
        tr("💬 テキスト")
    });

    m_newRewardButton->setText(tr("➕ 新規報酬を追加"));

    m_formGroup->setTitle(tr("🛠️ 演出と報酬の編集"));

    // セクションタイトルラベル
    auto* section1 = m_formGroup->findChild<QLabel*>("section1");
    if (section1) section1->setText(tr("📋 【1】 報酬基本設定"));
    auto* section2 = m_formGroup->findChild<QLabel*>("section2");
    if (section2) section2->setText(tr("🎨 【2】 演出詳細設定"));

    auto* scrollArea = m_formGroup->findChild<QScrollArea*>();
    if (scrollArea && scrollArea->widget()) {
        auto* formLayout = qobject_cast<QFormLayout*>(scrollArea->widget()->layout());
        if (formLayout) {
            auto* l1 = qobject_cast<QLabel*>(formLayout->labelForField(m_rewardIdEdit));
            if (l1) l1->setText(tr("報酬 ID (読取専用):"));
            auto* l2 = qobject_cast<QLabel*>(formLayout->labelForField(m_rewardNameEdit));
            if (l2) l2->setText(tr("🎁 報酬名:"));
            auto* l3 = qobject_cast<QLabel*>(formLayout->labelForField(m_rewardCostSpin));
            if (l3) l3->setText(tr("ポイント数 (Cost):"));
            auto* l4 = qobject_cast<QLabel*>(formLayout->labelForField(m_rewardCooldownSpin));
            if (l4) l4->setText(tr("クールダウン秒数:"));
            auto* l5 = qobject_cast<QLabel*>(formLayout->labelForField(m_rewardModeCombo));
            if (l5) l5->setText(tr("演出再生モード:"));
            auto* l6 = qobject_cast<QLabel*>(formLayout->labelForField(m_rewardEnabledCheck));
            if (l6) l6->setText(tr("ステータス:"));

            auto* l7 = qobject_cast<QLabel*>(formLayout->labelForField(m_effectTypeCombo));
            if (l7) l7->setText(tr("演出の種類:"));
            auto* l8 = qobject_cast<QLabel*>(formLayout->labelForField(m_filePathEdit->parentWidget()));
            if (l8) l8->setText(tr("画像・動画ファイルパス:"));
            auto* l9 = qobject_cast<QLabel*>(formLayout->labelForField(m_audioPathEdit->parentWidget()));
            if (l9) l9->setText(tr("効果音ファイルパス:"));
            auto* l10 = qobject_cast<QLabel*>(formLayout->labelForField(m_durationSpin));
            if (l10) l10->setText(tr("演出表示時間 (秒):"));
            auto* l11 = qobject_cast<QLabel*>(formLayout->labelForField(m_volumeSpin));
            if (l11) l11->setText(tr("音量 (%):"));
            auto* l12 = qobject_cast<QLabel*>(formLayout->labelForField(m_scaleSpin));
            if (l12) l12->setText(tr("画像・動画縮尺 (%):"));
            auto* l13 = qobject_cast<QLabel*>(formLayout->labelForField(m_textEdit));
            if (l13) l13->setText(tr("表示テキスト:"));
            auto* l14 = qobject_cast<QLabel*>(formLayout->labelForField(m_positionPresetCombo));
            if (l14) l14->setText(tr("表示位置プリセット:"));
            auto* l15 = qobject_cast<QLabel*>(formLayout->labelForField(m_positionXSpin->parentWidget()));
            if (l15) l15->setText(tr("カスタム位置座標:"));
            auto* l16 = qobject_cast<QLabel*>(formLayout->labelForField(m_animationCombo));
            if (l16) l16->setText(tr("演出効果アニメーション:"));
        }
    }

    m_rewardModeCombo->blockSignals(true);
    int curMode = m_rewardModeCombo->currentIndex();
    m_rewardModeCombo->clear();
    m_rewardModeCombo->addItem(tr("全て再生 (sequential)"), "sequential");
    m_rewardModeCombo->addItem(tr("ランダムで1つ再生 (random)"), "random");
    m_rewardModeCombo->setCurrentIndex(curMode >= 0 ? curMode : 0);
    m_rewardModeCombo->blockSignals(false);

    m_rewardEnabledCheck->setText(tr("この報酬を有効にする"));

    m_effectTypeCombo->blockSignals(true);
    int curEffType = m_effectTypeCombo->currentIndex();
    m_effectTypeCombo->clear();
    m_effectTypeCombo->addItem(tr("🖼️ 画像"), "image");
    m_effectTypeCombo->addItem(tr("🎥 動画"), "video");
    m_effectTypeCombo->addItem(tr("🔊 音声のみ"), "sound");
    m_effectTypeCombo->addItem(tr("💬 テキストのみ"), "text");
    m_effectTypeCombo->setCurrentIndex(curEffType >= 0 ? curEffType : 0);
    m_effectTypeCombo->blockSignals(false);

    m_filePathSelectBtn->setText(tr("📁 参照"));
    m_audioPathSelectBtn->setText(tr("📁 参照"));

    m_positionPresetCombo->blockSignals(true);
    int curPos = m_positionPresetCombo->currentIndex();
    m_positionPresetCombo->clear();
    m_positionPresetCombo->addItem(tr("中央 (center)"), "center");
    m_positionPresetCombo->addItem(tr("左上 (top_left)"), "top_left");
    m_positionPresetCombo->addItem(tr("右上 (top_right)"), "top_right");
    m_positionPresetCombo->addItem(tr("左下 (bottom_left)"), "bottom_left");
    m_positionPresetCombo->addItem(tr("右下 (bottom_right)"), "bottom_right");
    m_positionPresetCombo->addItem(tr("カスタム座標"), "custom");
    m_positionPresetCombo->setCurrentIndex(curPos >= 0 ? curPos : 0);
    m_positionPresetCombo->blockSignals(false);

    m_centerCoordLabel->setText(tr("Xオフセット:"));
    auto* coordLabelY = m_formGroup->findChild<QLabel*>("coordLabelY");
    if (coordLabelY) coordLabelY->setText(tr("Yオフセット:"));

    m_animationCombo->blockSignals(true);
    int curAnim = m_animationCombo->currentIndex();
    m_animationCombo->clear();
    m_animationCombo->addItem(tr("フェードイン・アウト (fade)"), "fade");
    m_animationCombo->addItem(tr("スライドイン (slide)"), "slide");
    m_animationCombo->addItem(tr("バウンス (bounce)"), "bounce");
    m_animationCombo->addItem(tr("アニメーションなし (none)"), "none");
    m_animationCombo->setCurrentIndex(curAnim >= 0 ? curAnim : 0);
    m_animationCombo->blockSignals(false);

    m_saveButton->setText(tr("💾 変更をデータベースに保存"));
    m_addEffectButton->setText(tr("➕ 演出追加"));
    m_deleteEffectButton->setText(tr("❌ 選択演出を削除"));
    m_deleteRewardButton->setText(tr("🗑️ 報酬ごと削除"));

    // タブ1の名前
    m_tabWidget->setTabText(0, tr("🎁 報酬と演出の編集"));

    // タブ2: ログ管理
    m_logSearchLabel->setText(tr("🔍 ログ検索:"));
    m_logSearchEdit->setPlaceholderText(tr("ユーザー名、報酬名等で絞り込み..."));
    m_logTableWidget->setHorizontalHeaderLabels({
        tr("ID"), 
        tr("🎁 報酬名"), 
        tr("👤 ユーザー名"), 
        tr("⏱️ 使用日時")
    });

    m_cleanupGroup->setTitle(tr("🗑️ 統計ログのクリーンアップ & データベース最適化"));
    m_cleanupPeriodLabel->setText(tr("クリーンアップ対象の期間:"));

    m_cleanupPeriodCombo->blockSignals(true);
    int curCleanIdx = m_cleanupPeriodCombo->currentIndex();
    m_cleanupPeriodCombo->clear();
    m_cleanupPeriodCombo->addItem(tr("全てのログデータを削除"), "all");
    m_cleanupPeriodCombo->addItem(tr("1週間以上前のログデータを削除"), "1week");
    m_cleanupPeriodCombo->addItem(tr("1ヶ月以上前のログデータを削除"), "1month");
    m_cleanupPeriodCombo->addItem(tr("3ヶ月以上前のログデータを削除"), "3months");
    m_cleanupPeriodCombo->setCurrentIndex(curCleanIdx >= 0 ? curCleanIdx : 0);
    m_cleanupPeriodCombo->blockSignals(false);

    m_cleanupButton->setText(tr("🚨 クリーンアップ & DB軽量化 (VACUUM) を実行"));
    m_tabWidget->setTabText(1, tr("📊 統計ログ管理とDB軽量化"));

    refreshData();
    refreshLogData();
    updateDbSizeDisplay();
}
