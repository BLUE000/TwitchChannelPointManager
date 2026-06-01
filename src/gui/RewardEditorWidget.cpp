#include "RewardEditorWidget.hpp"
#include "../core/Application.hpp"
#include "../core/Config.hpp"
#include "../database/Database.hpp"
#include "../twitch/TwitchAuth.hpp"
#include "../reward/RewardManager.hpp"
#include "../reward/QueueManager.hpp"
#include "../overlay/OverlayServer.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QDir>
#include <QLabel>
#include <QEvent>
#include <QCoreApplication>

RewardEditorWidget::RewardEditorWidget(Application* app, QWidget* parent)
    : QWidget(parent)
    , m_app(app)
{
    setupUi();
    reloadRewardsList();

    if (m_app->twitchAuth()) {
        connect(m_app->twitchAuth(), &TwitchAuth::customRewardsFetched, this, &RewardEditorWidget::onCustomRewardsFetched);
        connect(m_app->twitchAuth(), &TwitchAuth::customRewardsFetchFailed, this, &RewardEditorWidget::onCustomRewardsFetchFailed);
    }
}

void RewardEditorWidget::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);

    // 1. 左側: 報酬一覧リスト
    auto* leftLayout = new QVBoxLayout();
    m_listLabel = new QLabel(tr("登録済みの報酬一覧:"), this);
    leftLayout->addWidget(m_listLabel);
    
    m_rewardsList = new QListWidget(this);
    m_rewardsList->setMinimumWidth(220);
    connect(m_rewardsList, &QListWidget::itemClicked, this, &RewardEditorWidget::onRewardSelected);
    leftLayout->addWidget(m_rewardsList);

    m_newButton = new QPushButton(tr("新規演出を登録"), this);
    m_newButton->setStyleSheet("background-color: #2E7D32; color: white; font-weight: bold; padding: 6px;");
    connect(m_newButton, &QPushButton::clicked, this, &RewardEditorWidget::onNewClicked);

    m_syncButton = new QPushButton(tr("🔄 Twitch同期"), this);
    m_syncButton->setStyleSheet("background-color: #6441A5; color: white; font-weight: bold; padding: 6px;");
    connect(m_syncButton, &QPushButton::clicked, this, &RewardEditorWidget::onSyncClicked);

    auto* listBtnLayout = new QHBoxLayout();
    listBtnLayout->addWidget(m_newButton);
    listBtnLayout->addWidget(m_syncButton);
    leftLayout->addLayout(listBtnLayout);

    mainLayout->addLayout(leftLayout);

    // 2. 右側: 詳細編集フォーム
    auto* rightLayout = new QVBoxLayout();
    m_detailGroup = new QGroupBox(tr("報酬情報の設定"), this);
    auto* formLayout = new QFormLayout(m_detailGroup);

    m_idEdit = new QLineEdit(this);
    m_idEdit->setPlaceholderText(tr("左のリストから選択するか、Twitchカスタム報酬IDを入力"));
    m_idEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    formLayout->addRow(tr("報酬 ID (Twitch):"), m_idEdit);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("例: たぬき投げ"));
    m_nameEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    formLayout->addRow(tr("報酬名 (表示用):"), m_nameEdit);

    m_costSpin = new QSpinBox(this);
    m_costSpin->setRange(0, 1000000);
    m_costSpin->setValue(500);
    m_costSpin->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    formLayout->addRow(tr("消費ポイント数:"), m_costSpin);

    m_cooldownSpin = new QSpinBox(this);
    m_cooldownSpin->setRange(0, 86400);
    m_cooldownSpin->setSuffix(tr(" 秒"));
    m_cooldownSpin->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    formLayout->addRow(tr("クールタイム:"), m_cooldownSpin);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("全ての演出を順番に再生 (sequential)"), "sequential");
    m_modeCombo->addItem(tr("演出リストからランダムで1つ再生 (random)"), "random");
    m_modeCombo->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    formLayout->addRow(tr("演出再生モード:"), m_modeCombo);

    m_enabledCheck = new QCheckBox(tr("報酬演出の有効化"), this);
    m_enabledCheck->setChecked(true);
    formLayout->addRow(tr("ステータス:"), m_enabledCheck);

    rightLayout->addWidget(m_detailGroup);

    // 演出効果の設定グループ
    m_effectGroup = new QGroupBox(tr("演出効果（エフェクト）設定"), this);
    auto* effectLayout = new QFormLayout(m_effectGroup);

    // 複数演出の切り替え・追加・削除コントロール
    auto* selectorLayout = new QHBoxLayout();
    m_effectSelectorCombo = new QComboBox(this);
    m_effectSelectorCombo->setMinimumWidth(180);
    m_effectSelectorCombo->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    connect(m_effectSelectorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RewardEditorWidget::onEffectSelectorChanged);
    
    m_addEffectBtn = new QPushButton(tr("＋ 演出を追加"), this);
    m_addEffectBtn->setStyleSheet("background-color: #2E7D32; color: white; font-weight: bold; padding: 4px 8px;");
    connect(m_addEffectBtn, &QPushButton::clicked, this, &RewardEditorWidget::onAddEffectClicked);
    
    m_deleteEffectBtn = new QPushButton(tr("❌ 削除"), this);
    m_deleteEffectBtn->setStyleSheet("background-color: #C62828; color: white; padding: 4px 8px;");
    connect(m_deleteEffectBtn, &QPushButton::clicked, this, &RewardEditorWidget::onDeleteEffectClicked);
    
    selectorLayout->addWidget(m_effectSelectorCombo, 1);
    selectorLayout->addWidget(m_addEffectBtn);
    selectorLayout->addWidget(m_deleteEffectBtn);
    m_editTargetLabel = new QLabel(tr("編集対象の演出:"), this);
    effectLayout->addRow(m_editTargetLabel, selectorLayout);

    m_isCustomHtmlOnlyCb = new QCheckBox(tr("カスタムHTML演出として実行"), this);
    m_isCustomHtmlOnlyCb->setStyleSheet("font-weight: bold; margin-bottom: 5px;");
    effectLayout->addRow(m_isCustomHtmlOnlyCb);
    connect(m_isCustomHtmlOnlyCb, &QCheckBox::toggled, this, &RewardEditorWidget::onCustomHtmlOnlyToggled);

    // --- 通常演出設定コンテナ ---
    m_normalEffectConfigWidget = new QWidget(this);
    auto* normalLayout = new QFormLayout(m_normalEffectConfigWidget);
    normalLayout->setContentsMargins(0, 0, 0, 0);

    m_effectTypeCombo = new QComboBox(this);
    m_effectTypeCombo->addItem(tr("画像のみ (image)"), "image");
    m_effectTypeCombo->addItem(tr("動画（透過WebMなど） (video)"), "video");
    m_effectTypeCombo->addItem(tr("音響効果のみ (sound)"), "sound");
    m_effectTypeCombo->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    normalLayout->addRow(tr("演出の種類:"), m_effectTypeCombo);
    connect(m_effectTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RewardEditorWidget::onEffectTypeChanged);

    // アセットファイル選択
    auto* imgLayout = new QHBoxLayout();
    m_imagePathEdit = new QLineEdit(this);
    m_imagePathEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    m_imageSelectBtn = new QPushButton(tr("参照..."), this);
    imgLayout->addWidget(m_imagePathEdit);
    imgLayout->addWidget(m_imageSelectBtn);
    m_imagePathLabel = new QLabel(tr("画像/動画ファイル:"), this);
    normalLayout->addRow(m_imagePathLabel, imgLayout);
    connect(m_imageSelectBtn, &QPushButton::clicked, this, &RewardEditorWidget::selectImagePath);

    auto* audLayout = new QHBoxLayout();
    m_audioPathEdit = new QLineEdit(this);
    m_audioPathEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    auto* audSelectBtn = new QPushButton(tr("参照..."), this);
    audLayout->addWidget(m_audioPathEdit);
    audLayout->addWidget(audSelectBtn);
    normalLayout->addRow(tr("効果音ファイル:"), audLayout);
    connect(audSelectBtn, &QPushButton::clicked, this, &RewardEditorWidget::selectAudioPath);

    m_durationSpin = new QSpinBox(this);
    m_durationSpin->setRange(1, 300);
    m_durationSpin->setValue(5);
    m_durationSpin->setSuffix(tr(" 秒"));
    m_durationSpin->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    normalLayout->addRow(tr("表示・演出時間:"), m_durationSpin);

    m_scaleSpin = new QSpinBox(this);
    m_scaleSpin->setRange(1, 100);
    m_scaleSpin->setValue(100);
    m_scaleSpin->setSuffix(" %");
    m_scaleSpin->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    normalLayout->addRow(tr("表示サイズ (1-100%):"), m_scaleSpin);

    m_textEdit = new QLineEdit(this);
    m_textEdit->setPlaceholderText(tr("例: {user}がたぬきを投げた！"));
    m_textEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    normalLayout->addRow(tr("吹き出し表示文字列:"), m_textEdit);

    // --- 表示位置設定 ---
    m_positionPresetCombo = new QComboBox(this);
    m_positionPresetCombo->addItem(tr("中央 (center)"),       "center");
    m_positionPresetCombo->addItem(tr("左上 (top_left)"),     "top_left");
    m_positionPresetCombo->addItem(tr("右上 (top_right)"),    "top_right");
    m_positionPresetCombo->addItem(tr("左下 (bottom_left)"),  "bottom_left");
    m_positionPresetCombo->addItem(tr("右下 (bottom_right)"), "bottom_right");
    m_positionPresetCombo->addItem(tr("カスタム"),             "custom");
    m_positionPresetCombo->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    normalLayout->addRow(tr("表示位置:"), m_positionPresetCombo);

    // 座標微調整
    auto* coordWidget = new QWidget(this);
    auto* coordLayout = new QHBoxLayout(coordWidget);
    coordLayout->setContentsMargins(0, 0, 0, 0);
    coordLayout->addWidget(new QLabel("X:", coordWidget));
    m_positionXSpin = new QSpinBox(coordWidget);
    m_positionXSpin->setRange(0, 3840);
    m_positionXSpin->setValue(960);
    m_positionXSpin->setSuffix(" px");
    m_positionXSpin->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    coordLayout->addWidget(m_positionXSpin);
    coordLayout->addSpacing(12);
    coordLayout->addWidget(new QLabel("Y:", coordWidget));
    m_positionYSpin = new QSpinBox(coordWidget);
    m_positionYSpin->setRange(0, 2160);
    m_positionYSpin->setValue(540);
    m_positionYSpin->setSuffix(" px");
    m_positionYSpin->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    coordLayout->addWidget(m_positionYSpin);
    coordLayout->addStretch();
    m_coordLabel = new QLabel(tr("  ↳ 中心座標 (px):"), this);
    normalLayout->addRow(m_coordLabel, coordWidget);

    effectLayout->addRow(m_normalEffectConfigWidget);

    // --- カスタムHTML専用設定コンテナ ---
    m_customHtmlConfigWidget = new QWidget(this);
    auto* extLayout = new QFormLayout(m_customHtmlConfigWidget);
    extLayout->setContentsMargins(0, 0, 0, 0);

    // HTMLファイルパス
    auto* htmlLayout = new QHBoxLayout();
    m_htmlPathEdit = new QLineEdit(this);
    m_htmlPathEdit->setReadOnly(true);
    m_htmlPathEdit->setPlaceholderText(tr("ドキュメントルート(custom_html/)配下のHTMLファイルを選択してください"));
    m_htmlPathEdit->setStyleSheet("background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 6px;");
    m_htmlSelectBtn = new QPushButton(tr("参照..."), this);
    auto* htmlClearBtn = new QPushButton(tr("クリア"), this);
    htmlClearBtn->setStyleSheet("padding: 4px;");
    htmlLayout->addWidget(m_htmlPathEdit, 1);
    htmlLayout->addWidget(m_htmlSelectBtn);
    htmlLayout->addWidget(htmlClearBtn);
    m_htmlLabel = new QLabel(tr("HTML 演出ファイル (OBS用):"), this);
    extLayout->addRow(m_htmlLabel, htmlLayout);
    connect(m_htmlSelectBtn, &QPushButton::clicked, this, &RewardEditorWidget::selectHtmlPath);
    connect(htmlClearBtn, &QPushButton::clicked, [this]() { m_htmlPathEdit->clear(); });

    effectLayout->addRow(m_customHtmlConfigWidget);
    m_customHtmlConfigWidget->setVisible(false);

    // プリセット選択時に中心座標を自動更新（1920x1080 基準）
    connect(m_positionPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
        const QString p = m_positionPresetCombo->currentData().toString();
        if      (p == "center")       { m_positionXSpin->setValue(960);  m_positionYSpin->setValue(540); }
        else if (p == "top_left")     { m_positionXSpin->setValue(200);  m_positionYSpin->setValue(150); }
        else if (p == "top_right")    { m_positionXSpin->setValue(1720); m_positionYSpin->setValue(150); }
        else if (p == "bottom_left")  { m_positionXSpin->setValue(200);  m_positionYSpin->setValue(930); }
        else if (p == "bottom_right") { m_positionXSpin->setValue(1720); m_positionYSpin->setValue(930); }
    });

    rightLayout->addWidget(m_effectGroup);

    // 保存＆削除アクション
    m_testButton = new QPushButton(tr("▶️ 演出をテスト再生 (OBS)"), this);
    m_testButton->setStyleSheet("background-color: #9C27B0; color: white; font-weight: bold; padding: 8px; font-size: 13px; border-radius: 4px; margin-bottom: 5px;");
    m_testButton->setEnabled(false);
    connect(m_testButton, &QPushButton::clicked, this, &RewardEditorWidget::onTestClicked);
    rightLayout->addWidget(m_testButton);

    auto* actLayout = new QHBoxLayout();
    m_saveButton = new QPushButton(tr("設定を保存"), this);
    m_saveButton->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 6px;");
    connect(m_saveButton, &QPushButton::clicked, this, &RewardEditorWidget::onSaveClicked);

    m_deleteButton = new QPushButton(tr("この報酬を削除"), this);
    m_deleteButton->setStyleSheet("background-color: #E53935; color: white; padding: 6px;");
    connect(m_deleteButton, &QPushButton::clicked, this, &RewardEditorWidget::onDeleteClicked);

    actLayout->addWidget(m_saveButton);
    actLayout->addWidget(m_deleteButton);
    rightLayout->addLayout(actLayout);

    mainLayout->addLayout(rightLayout);
    m_isCustomHtmlOnlyCb->setVisible(true);
}

void RewardEditorWidget::reloadRewardsList()
{
    m_rewardsList->clear();
    
    QList<Reward> localList = m_app->rewardManager()->getAllRewards();
    for (const auto& r : localList) {
        auto* item = new QListWidgetItem(QString(tr("🟡 [読込中] %1 (%2pt)")).arg(r.name).arg(r.cost), m_rewardsList);
        item->setData(Qt::UserRole, r.id);
        item->setData(Qt::UserRole + 1, true);
        item->setData(Qt::UserRole + 2, r.name);
        item->setData(Qt::UserRole + 3, r.cost);
        item->setForeground(QBrush(QColor("#A9A9B2")));
    }

    QString secretKey = "twitch_overlay_secret_key_2026";
    QString access = m_app->config()->loadSecureString("twitch_access_token", secretKey);
    QString broadcasterId = m_app->config()->get("twitch_broadcaster_id").toString();
    QString clientId = m_app->config()->get("twitch_client_id", TWITCH_GLOBAL_CLIENT_ID).toString();

    if (!access.isEmpty() && !broadcasterId.isEmpty()) {
        m_app->twitchAuth()->fetchCustomRewards(access, clientId, broadcasterId);
    } else {
        m_rewardsList->clear();
        for (const auto& r : localList) {
            auto* item = new QListWidgetItem(QString(tr("🟢 [設定済] %1 (%2pt)")).arg(r.name).arg(r.cost), m_rewardsList);
            item->setData(Qt::UserRole, r.id);
            item->setData(Qt::UserRole + 1, true);
            item->setData(Qt::UserRole + 2, r.name);
            item->setData(Qt::UserRole + 3, r.cost);
            item->setForeground(QBrush(QColor("#FFFFFF")));
        }
    }
}

void RewardEditorWidget::onRewardSelected(QListWidgetItem* item)
{
    if (!item) return;

    QString id = item->data(Qt::UserRole).toString();
    bool hasLocal = item->data(Qt::UserRole + 1).toBool();
    QString name = item->data(Qt::UserRole + 2).toString();
    int cost = item->data(Qt::UserRole + 3).toInt();

    m_idEdit->setText(id);
    m_nameEdit->setText(name);
    m_costSpin->setValue(cost);

    if (hasLocal) {
        Reward r;
        if (m_app->rewardManager()->getReward(id, r)) {
            m_cooldownSpin->setValue(r.cooldown);
            m_enabledCheck->setChecked(r.enabled);
            
            int modeIdx = m_modeCombo->findData(r.mode);
            if (modeIdx != -1) m_modeCombo->setCurrentIndex(modeIdx);

            m_editingEffects = r.effects;
        } else {
            m_cooldownSpin->setValue(0);
            m_enabledCheck->setChecked(true);
            m_editingEffects.clear();
        }
    } else {
        m_cooldownSpin->setValue(0);
        m_enabledCheck->setChecked(true);
        m_editingEffects.clear();
    }

    if (m_editingEffects.isEmpty()) {
        Effect def;
        def.type = "image";
        def.duration = 5;
        def.scale = 100;
        def.volume = 100;
        def.position.preset = "center";
        def.position.offsetX = 960;
        def.position.offsetY = 540;
        m_editingEffects.append(def);
    }

    updateEffectSelectorCombo();
    m_testButton->setEnabled(hasLocal);
}

void RewardEditorWidget::updateEffectSelectorCombo()
{
    m_effectSelectorCombo->blockSignals(true);
    m_effectSelectorCombo->clear();
    for (int i = 0; i < m_editingEffects.size(); ++i) {
        m_effectSelectorCombo->addItem(QString(tr("演出 %1")).arg(i + 1), i);
    }
    m_effectSelectorCombo->blockSignals(false);

    if (m_editingEffects.size() > 0) {
        m_effectSelectorCombo->setCurrentIndex(0);
        loadEffectFromBuffer(0);
    } else {
        m_currentEffectIndex = -1;
    }
}

void RewardEditorWidget::onEffectSelectorChanged(int index)
{
    if (index < 0 || index >= m_editingEffects.size()) return;
    saveCurrentEffectToBuffer();
    loadEffectFromBuffer(index);
}

void RewardEditorWidget::saveCurrentEffectToBuffer()
{
    if (m_currentEffectIndex < 0 || m_currentEffectIndex >= m_editingEffects.size()) return;

    Effect& eff = m_editingEffects[m_currentEffectIndex];
    eff.isCustomHtmlOnly = m_isCustomHtmlOnlyCb->isChecked();

    if (eff.isCustomHtmlOnly) {
        eff.htmlPath = m_htmlPathEdit->text().trimmed();
    } else {
        eff.type = m_effectTypeCombo->currentData().toString();
        eff.filePath = m_imagePathEdit->text().trimmed();
        eff.audioPath = m_audioPathEdit->text().trimmed();
        eff.duration = m_durationSpin->value();
        eff.scale = m_scaleSpin->value();
        eff.text = m_textEdit->text().trimmed();
        eff.position.preset = m_positionPresetCombo->currentData().toString();
        eff.position.offsetX = m_positionXSpin->value();
        eff.position.offsetY = m_positionYSpin->value();
    }
}

void RewardEditorWidget::loadEffectFromBuffer(int index)
{
    if (index < 0 || index >= m_editingEffects.size()) return;
    m_currentEffectIndex = index;

    const Effect& eff = m_editingEffects[index];
    
    m_isCustomHtmlOnlyCb->blockSignals(true);
    m_isCustomHtmlOnlyCb->setChecked(eff.isCustomHtmlOnly);
    m_isCustomHtmlOnlyCb->blockSignals(false);

    onCustomHtmlOnlyToggled(eff.isCustomHtmlOnly);

    if (eff.isCustomHtmlOnly) {
        m_htmlPathEdit->setText(eff.htmlPath);
    } else {
        int typeIdx = m_effectTypeCombo->findData(eff.type);
        if (typeIdx != -1) m_effectTypeCombo->setCurrentIndex(typeIdx);

        m_imagePathEdit->setText(eff.filePath);
        m_audioPathEdit->setText(eff.audioPath);
        m_durationSpin->setValue(eff.duration);
        m_scaleSpin->setValue(eff.scale);
        m_textEdit->setText(eff.text);

        int posIdx = m_positionPresetCombo->findData(eff.position.preset);
        if (posIdx != -1) m_positionPresetCombo->setCurrentIndex(posIdx);

        m_positionXSpin->setValue(eff.position.offsetX);
        m_positionYSpin->setValue(eff.position.offsetY);

        onEffectTypeChanged(m_effectTypeCombo->currentIndex());
    }
}

void RewardEditorWidget::onCustomHtmlOnlyToggled(bool checked)
{
    m_normalEffectConfigWidget->setVisible(!checked);
    m_customHtmlConfigWidget->setVisible(checked);
}

void RewardEditorWidget::onEffectTypeChanged(int index)
{
    QString type = m_effectTypeCombo->itemData(index).toString();
    if (type == "sound") {
        m_imagePathLabel->setVisible(false);
        m_imagePathEdit->setVisible(false);
        m_imageSelectBtn->setVisible(false);
    } else if (type == "video") {
        m_imagePathLabel->setText(tr("動画ファイル:"));
        m_imagePathLabel->setVisible(true);
        m_imagePathEdit->setVisible(true);
        m_imageSelectBtn->setVisible(true);
    } else {
        m_imagePathLabel->setText(tr("画像ファイル:"));
        m_imagePathLabel->setVisible(true);
        m_imagePathEdit->setVisible(true);
        m_imageSelectBtn->setVisible(true);
    }
}

void RewardEditorWidget::onAddEffectClicked()
{
    saveCurrentEffectToBuffer();

    Effect def;
    def.type = "image";
    def.duration = 5;
    def.scale = 100;
    def.volume = 100;
    def.position.preset = "center";
    def.position.offsetX = 960;
    def.position.offsetY = 540;

    m_editingEffects.append(def);
    updateEffectSelectorCombo();
    m_effectSelectorCombo->setCurrentIndex(m_editingEffects.size() - 1);
}

void RewardEditorWidget::onDeleteEffectClicked()
{
    if (m_editingEffects.size() <= 1) {
        QMessageBox::warning(this, tr("削除不可"), tr("少なくとも1つの演出設定が必要です。"));
        return;
    }

    int idx = m_effectSelectorCombo->currentIndex();
    if (idx >= 0 && idx < m_editingEffects.size()) {
        m_editingEffects.removeAt(idx);
        m_currentEffectIndex = -1;
        updateEffectSelectorCombo();
    }
}

void RewardEditorWidget::onSaveClicked()
{
    saveCurrentEffectToBuffer();

    QString id = m_idEdit->text().trimmed();
    QString name = m_nameEdit->text().trimmed();

    if (id.isEmpty() || name.isEmpty()) {
        QMessageBox::warning(this, tr("保存エラー"), tr("ID と 報酬名は必須です。"));
        return;
    }

    Reward r;
    r.id = id;
    r.name = name;
    r.cost = m_costSpin->value();
    r.cooldown = m_cooldownSpin->value();
    r.mode = m_modeCombo->currentData().toString();
    r.enabled = m_enabledCheck->isChecked();
    r.effects = m_editingEffects;
    r.allowedRoles = {"everyone"};

    if (m_app->rewardManager()->saveReward(r)) {
        QMessageBox::information(this, tr("成功"), tr("報酬演出設定を保存しました。"));
        reloadRewardsList();
        
        for (int i = 0; i < m_rewardsList->count(); ++i) {
            if (m_rewardsList->item(i)->data(Qt::UserRole).toString() == id) {
                m_rewardsList->setCurrentRow(i);
                onRewardSelected(m_rewardsList->item(i));
                break;
            }
        }
    } else {
        QMessageBox::critical(this, tr("エラー"), tr("データベースへの保存に失敗しました。"));
    }
}

void RewardEditorWidget::onDeleteClicked()
{
    QString id = m_idEdit->text().trimmed();
    if (id.isEmpty()) return;

    auto btn = QMessageBox::question(this, tr("削除の確認"),
                                     tr("この報酬の演出設定をデータベースから削除します。\nよろしいですか？"),
                                     QMessageBox::Yes | QMessageBox::No);

    if (btn == QMessageBox::Yes) {
        if (m_app->rewardManager()->deleteReward(id)) {
            QMessageBox::information(this, tr("完了"), tr("報酬演出設定を削除しました。"));
            
            m_idEdit->clear();
            m_nameEdit->clear();
            m_costSpin->setValue(500);
            m_cooldownSpin->setValue(0);
            m_editingEffects.clear();
            
            reloadRewardsList();
            updateEffectSelectorCombo();
        } else {
            QMessageBox::critical(this, tr("エラー"), tr("削除に失敗しました。"));
        }
    }
}

void RewardEditorWidget::onNewClicked()
{
    m_idEdit->clear();
    m_nameEdit->clear();
    m_costSpin->setValue(500);
    m_cooldownSpin->setValue(0);
    m_enabledCheck->setChecked(true);
    
    m_editingEffects.clear();
    Effect def;
    def.type = "image";
    def.duration = 5;
    def.scale = 100;
    def.volume = 100;
    def.position.preset = "center";
    def.position.offsetX = 960;
    def.position.offsetY = 540;
    m_editingEffects.append(def);

    updateEffectSelectorCombo();
    m_rewardsList->clearSelection();
    m_testButton->setEnabled(false);
}

void RewardEditorWidget::onSyncClicked()
{
    QString secretKey = "twitch_overlay_secret_key_2026";
    QString access = m_app->config()->loadSecureString("twitch_access_token", secretKey);
    QString broadcasterId = m_app->config()->get("twitch_broadcaster_id").toString();
    QString clientId = m_app->config()->get("twitch_client_id", TWITCH_GLOBAL_CLIENT_ID).toString();

    if (access.isEmpty() || broadcasterId.isEmpty()) {
        QMessageBox::warning(this, tr("同期エラー"), tr("Twitchアカウントの認証連携が完了していません。設定タブから連携を完了させてください。"));
        return;
    }

    QMessageBox::information(this, tr("同期開始"), tr("Twitchからカスタム報酬リストを同期取得します..."));
    m_app->twitchAuth()->fetchCustomRewards(access, clientId, broadcasterId);
}

void RewardEditorWidget::onTestClicked()
{
    saveCurrentEffectToBuffer();

    QString id = m_idEdit->text().trimmed();
    if (id.isEmpty()) return;

    Reward r;
    if (m_app->rewardManager()->getReward(id, r)) {
        QueueItem item;
        item.queueId = "TEST_PLAY_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        item.rewardId = r.id;
        item.username = tr("テスト配信者");
        item.timestamp = QDateTime::currentDateTime();
        
        for (const auto& eff : m_editingEffects) {
            item.effects.enqueue(eff);
        }

        m_app->overlayServer()->sendEffect(item, m_editingEffects[0]);
    }
}

void RewardEditorWidget::onCustomRewardsFetched(const QJsonArray& rewards)
{
    QList<Reward> localList = m_app->rewardManager()->getAllRewards();
    m_rewardsList->clear();

    QSet<QString> localIds;
    for (const auto& r : localList) {
        localIds.insert(r.id);
    }

    for (int i = 0; i < rewards.size(); ++i) {
        QJsonObject obj = rewards[i].toObject();
        QString id = obj["id"].toString();
        QString name = obj["title"].toString();
        int cost = obj["cost"].toInt();
        bool isEnabled = obj["is_enabled"].toBool();

        if (!isEnabled) continue;

        bool hasLocal = localIds.contains(id);
        QString prefix = hasLocal ? tr("🟢 [設定済] ") : tr("⚪ [未設定] ");
        
        auto* item = new QListWidgetItem(prefix + name + QString(" (%1pt)").arg(cost), m_rewardsList);
        item->setData(Qt::UserRole, id);
        item->setData(Qt::UserRole + 1, hasLocal);
        item->setData(Qt::UserRole + 2, name);
        item->setData(Qt::UserRole + 3, cost);

        if (hasLocal) {
            item->setForeground(QBrush(QColor("#FFFFFF")));
        } else {
            item->setForeground(QBrush(QColor("#FFB74D")));
        }
    }

    QSet<QString> remoteIds;
    for (int i = 0; i < rewards.size(); ++i) {
        remoteIds.insert(rewards[i].toObject()["id"].toString());
    }

    for (const auto& r : localList) {
        if (!remoteIds.contains(r.id)) {
            auto* item = new QListWidgetItem(QString(tr("🔴 [オフライン] %1 (%2pt)")).arg(r.name).arg(r.cost), m_rewardsList);
            item->setData(Qt::UserRole, r.id);
            item->setData(Qt::UserRole + 1, true);
            item->setData(Qt::UserRole + 2, r.name);
            item->setData(Qt::UserRole + 3, r.cost);
            item->setForeground(QBrush(QColor("#EF5350")));
        }
    }
}

void RewardEditorWidget::onCustomRewardsFetchFailed(const QString& errorMessage)
{
    QMessageBox::warning(this, tr("同期失敗"), tr("Twitchからのカスタム報酬の取得に失敗しました。\nエラー: ") + errorMessage);
    
    m_rewardsList->clear();
    QList<Reward> localList = m_app->rewardManager()->getAllRewards();
    for (const auto& r : localList) {
        auto* item = new QListWidgetItem(QString(tr("🟢 [設定済] %1 (%2pt)")).arg(r.name).arg(r.cost), m_rewardsList);
        item->setData(Qt::UserRole, r.id);
        item->setData(Qt::UserRole + 1, true);
        item->setData(Qt::UserRole + 2, r.name);
        item->setData(Qt::UserRole + 3, r.cost);
        item->setForeground(QBrush(QColor("#FFFFFF")));
    }
}

void RewardEditorWidget::selectImagePath()
{
    QString filter = tr("Images/Videos (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.webm *.mp4);;All Files (*)");
    QString path = QFileDialog::getOpenFileName(this, tr("ファイルを選択"), "", filter);
    if (path.isEmpty()) return;

    path = QDir::toNativeSeparators(path);
    m_imagePathEdit->setText(path);
}

void RewardEditorWidget::selectAudioPath()
{
    QString filter = tr("Audio Files (*.mp3 *.wav *.ogg *.aac *.m4a);;All Files (*)");
    QString path = QFileDialog::getOpenFileName(this, tr("効果音ファイルを選択"), "", filter);
    if (path.isEmpty()) return;

    path = QDir::toNativeSeparators(path);
    m_audioPathEdit->setText(path);
}

void RewardEditorWidget::selectHtmlPath()
{
    QString customHtmlDir = QDir(QCoreApplication::applicationDirPath()).filePath("custom_html");
    
    QString path = QFileDialog::getOpenFileName(
        this, 
        tr("HTML演出ファイルを選択 (custom_html フォルダ配下)"), 
        customHtmlDir, 
        "HTML Files (*.html);;All Files (*)"
    );
    
    if (path.isEmpty()) return;
    
    path = QDir::cleanPath(path);
    QString cleanHtmlDir = QDir::cleanPath(customHtmlDir);
    
    if (!path.startsWith(cleanHtmlDir, Qt::CaseInsensitive)) {
        QMessageBox::warning(
            this, 
            tr("配置場所エラー"), 
            tr("アセットファイルは、アプリケーション実行フォルダ内の「custom_html」フォルダ配下に配置する必要があります。\n"
               "手動でファイルを「custom_html」フォルダに配置してから、再度選択してください。")
        );
        return;
    }
    
    QDir dir(cleanHtmlDir);
    QString relativePath = "custom_html/" + dir.relativeFilePath(path);
    m_htmlPathEdit->setText(QDir::toNativeSeparators(relativePath));
}

void RewardEditorWidget::selectRewardAndEffect(const QString& rewardId, int effectIndex)
{
    for (int i = 0; i < m_rewardsList->count(); ++i) {
        if (m_rewardsList->item(i)->data(Qt::UserRole).toString() == rewardId) {
            m_rewardsList->setCurrentRow(i);
            onRewardSelected(m_rewardsList->item(i));
            
            if (effectIndex >= 0 && effectIndex < m_editingEffects.size()) {
                m_effectSelectorCombo->setCurrentIndex(effectIndex);
            }
            break;
        }
    }
}

void RewardEditorWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void RewardEditorWidget::retranslateUi()
{
    m_listLabel->setText(tr("登録済みの報酬一覧:"));
    m_newButton->setText(tr("新規演出を登録"));
    m_syncButton->setText(tr("🔄 Twitch同期"));

    m_detailGroup->setTitle(tr("報酬情報の設定"));
    auto* detailLayout = qobject_cast<QFormLayout*>(m_detailGroup->layout());
    if (detailLayout) {
        auto* label = qobject_cast<QLabel*>(detailLayout->labelForField(m_idEdit));
        if (label) label->setText(tr("報酬 ID (Twitch):"));
        auto* label2 = qobject_cast<QLabel*>(detailLayout->labelForField(m_nameEdit));
        if (label2) label2->setText(tr("報酬名 (表示用):"));
        auto* label3 = qobject_cast<QLabel*>(detailLayout->labelForField(m_costSpin));
        if (label3) label3->setText(tr("消費ポイント数:"));
        auto* label4 = qobject_cast<QLabel*>(detailLayout->labelForField(m_cooldownSpin));
        if (label4) label4->setText(tr("クールタイム:"));
        auto* label5 = qobject_cast<QLabel*>(detailLayout->labelForField(m_modeCombo));
        if (label5) label5->setText(tr("演出再生モード:"));
        auto* label6 = qobject_cast<QLabel*>(detailLayout->labelForField(m_enabledCheck));
        if (label6) label6->setText(tr("ステータス:"));
    }
    m_idEdit->setPlaceholderText(tr("左のリストから選択するか、Twitchカスタム報酬IDを入力"));
    m_nameEdit->setPlaceholderText(tr("例: たぬき投げ"));
    m_cooldownSpin->setSuffix(tr(" 秒"));

    m_modeCombo->blockSignals(true);
    int currentModeIdx = m_modeCombo->currentIndex();
    m_modeCombo->clear();
    m_modeCombo->addItem(tr("全ての演出を順番に再生 (sequential)"), "sequential");
    m_modeCombo->addItem(tr("演出リストからランダムで1つ再生 (random)"), "random");
    m_modeCombo->setCurrentIndex(currentModeIdx >= 0 ? currentModeIdx : 0);
    m_modeCombo->blockSignals(false);

    m_enabledCheck->setText(tr("報酬演出の有効化"));

    m_effectGroup->setTitle(tr("演出効果（エフェクト）設定"));
    m_editTargetLabel->setText(tr("編集対象の演出:"));
    m_addEffectBtn->setText(tr("＋ 演出を追加"));
    m_deleteEffectBtn->setText(tr("❌ 削除"));
    m_isCustomHtmlOnlyCb->setText(tr("カスタムHTML演出として実行"));

    // 通常演出
    auto* normalLayout = qobject_cast<QFormLayout*>(m_normalEffectConfigWidget->layout());
    if (normalLayout) {
        auto* label = qobject_cast<QLabel*>(normalLayout->labelForField(m_effectTypeCombo));
        if (label) label->setText(tr("演出の種類:"));
        auto* label2 = qobject_cast<QLabel*>(normalLayout->labelForField(m_durationSpin));
        if (label2) label2->setText(tr("表示・演出時間:"));
        auto* label3 = qobject_cast<QLabel*>(normalLayout->labelForField(m_scaleSpin));
        if (label3) label3->setText(tr("表示サイズ (1-100%):"));
        auto* label4 = qobject_cast<QLabel*>(normalLayout->labelForField(m_textEdit));
        if (label4) label4->setText(tr("吹き出し表示文字列:"));
        auto* label5 = qobject_cast<QLabel*>(normalLayout->labelForField(m_positionPresetCombo));
        if (label5) label5->setText(tr("表示位置:"));
    }
    
    m_effectTypeCombo->blockSignals(true);
    int currentTypeIdx = m_effectTypeCombo->currentIndex();
    m_effectTypeCombo->clear();
    m_effectTypeCombo->addItem(tr("画像のみ (image)"), "image");
    m_effectTypeCombo->addItem(tr("動画（透過WebMなど） (video)"), "video");
    m_effectTypeCombo->addItem(tr("音響効果のみ (sound)"), "sound");
    m_effectTypeCombo->setCurrentIndex(currentTypeIdx >= 0 ? currentTypeIdx : 0);
    m_effectTypeCombo->blockSignals(false);

    m_imageSelectBtn->setText(tr("参照..."));
    m_durationSpin->setSuffix(tr(" 秒"));
    m_scaleSpin->setSuffix(" %");
    m_textEdit->setPlaceholderText(tr("例: {user}がたぬきを投げた！"));

    m_positionPresetCombo->blockSignals(true);
    int currentPresetIdx = m_positionPresetCombo->currentIndex();
    m_positionPresetCombo->clear();
    m_positionPresetCombo->addItem(tr("中央 (center)"),       "center");
    m_positionPresetCombo->addItem(tr("左上 (top_left)"),     "top_left");
    m_positionPresetCombo->addItem(tr("右上 (top_right)"),    "top_right");
    m_positionPresetCombo->addItem(tr("左下 (bottom_left)"),  "bottom_left");
    m_positionPresetCombo->addItem(tr("右下 (bottom_right)"), "bottom_right");
    m_positionPresetCombo->addItem(tr("カスタム"),             "custom");
    m_positionPresetCombo->setCurrentIndex(currentPresetIdx >= 0 ? currentPresetIdx : 0);
    m_positionPresetCombo->blockSignals(false);

    m_positionXSpin->setSuffix(" px");
    m_positionYSpin->setSuffix(" px");
    m_coordLabel->setText(tr("  ↳ 中心座標 (px):"));

    // html側
    m_htmlLabel->setText(tr("HTML 演出ファイル (OBS用):"));
    m_htmlPathEdit->setPlaceholderText(tr("ドキュメントルート(custom_html/)配下のHTMLファイルを選択してください"));
    m_htmlSelectBtn->setText(tr("参照..."));

    m_testButton->setText(tr("▶️ 演出をテスト再生 (OBS)"));
    m_saveButton->setText(tr("設定を保存"));
    m_deleteButton->setText(tr("この報酬を削除"));

    reloadRewardsList();
    updateEffectSelectorCombo();
}
