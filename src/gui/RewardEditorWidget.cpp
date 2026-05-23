#include "RewardEditorWidget.hpp"
#include "../core/Application.hpp"
#include "../core/Config.hpp"
#include "../twitch/TwitchAuth.hpp"
#include "../reward/RewardManager.hpp"
#include "../reward/QueueManager.hpp"
#include <QLabel>
#include <QDialog>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

RewardEditorWidget::RewardEditorWidget(Application* app, QWidget* parent)
    : QWidget(parent)
    , m_app(app)
{
    setupUi();
    reloadRewardsList();

    // Twitchの報酬取得用シグナル接続
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
    leftLayout->addWidget(new QLabel("登録済みの報酬一覧:", this));
    
    m_rewardsList = new QListWidget(this);
    m_rewardsList->setMinimumWidth(220);
    connect(m_rewardsList, &QListWidget::itemClicked, this, &RewardEditorWidget::onRewardSelected);
    leftLayout->addWidget(m_rewardsList);

    m_newButton = new QPushButton("新規演出を登録", this);
    m_newButton->setStyleSheet("background-color: #2E7D32; color: white; font-weight: bold; padding: 6px;");
    connect(m_newButton, &QPushButton::clicked, this, &RewardEditorWidget::onNewClicked);

    m_syncButton = new QPushButton("🔄 Twitch同期", this);
    m_syncButton->setStyleSheet("background-color: #6441A5; color: white; font-weight: bold; padding: 6px;");
    connect(m_syncButton, &QPushButton::clicked, this, &RewardEditorWidget::onSyncClicked);

    auto* listBtnLayout = new QHBoxLayout();
    listBtnLayout->addWidget(m_newButton);
    listBtnLayout->addWidget(m_syncButton);
    leftLayout->addLayout(listBtnLayout);

    mainLayout->addLayout(leftLayout);

    // 2. 右側: 詳細編集フォーム
    auto* rightLayout = new QVBoxLayout();
    auto* formGroup = new QGroupBox("報酬情報の設定", this);
    auto* formLayout = new QFormLayout(formGroup);

    m_idEdit = new QLineEdit(this);
    m_idEdit->setPlaceholderText("左のリストから選択するか、Twitchカスタム報酬IDを入力");
    formLayout->addRow("報酬 ID (Twitch):", m_idEdit);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("例: たぬき投げ");
    formLayout->addRow("報酬名 (表示用):", m_nameEdit);

    m_costSpin = new QSpinBox(this);
    m_costSpin->setRange(0, 1000000);
    m_costSpin->setValue(500);
    formLayout->addRow("消費ポイント数:", m_costSpin);

    m_cooldownSpin = new QSpinBox(this);
    m_cooldownSpin->setRange(0, 86400);
    m_cooldownSpin->setSuffix(" 秒");
    formLayout->addRow("クールタイム:", m_cooldownSpin);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("全ての演出を順番に再生 (sequential)", "sequential");
    m_modeCombo->addItem("演出リストからランダムで1つ再生 (random)", "random");
    formLayout->addRow("演出再生モード:", m_modeCombo);

    m_enabledCheck = new QCheckBox("報酬演出の有効化", this);
    m_enabledCheck->setChecked(true);
    formLayout->addRow("ステータス:", m_enabledCheck);

    rightLayout->addWidget(formGroup);

    // 演出効果の設定グループ
    auto* effectGroup = new QGroupBox("演出効果（エフェクト）設定", this);
    auto* effectLayout = new QFormLayout(effectGroup);

    // 複数演出の切り替え・追加・削除コントロール
    auto* selectorLayout = new QHBoxLayout();
    m_effectSelectorCombo = new QComboBox(this);
    m_effectSelectorCombo->setMinimumWidth(180);
    connect(m_effectSelectorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RewardEditorWidget::onEffectSelectorChanged);
    
    m_addEffectBtn = new QPushButton("＋ 演出を追加", this);
    m_addEffectBtn->setStyleSheet("background-color: #2E7D32; color: white; font-weight: bold; padding: 4px 8px;");
    connect(m_addEffectBtn, &QPushButton::clicked, this, &RewardEditorWidget::onAddEffectClicked);
    
    m_deleteEffectBtn = new QPushButton("❌ 削除", this);
    m_deleteEffectBtn->setStyleSheet("background-color: #C62828; color: white; padding: 4px 8px;");
    connect(m_deleteEffectBtn, &QPushButton::clicked, this, &RewardEditorWidget::onDeleteEffectClicked);
    
    selectorLayout->addWidget(m_effectSelectorCombo, 1);
    selectorLayout->addWidget(m_addEffectBtn);
    selectorLayout->addWidget(m_deleteEffectBtn);
    effectLayout->addRow("編集対象の演出:", selectorLayout);

    m_effectTypeCombo = new QComboBox(this);
    m_effectTypeCombo->addItem("画像のみ (image)", "image");
    m_effectTypeCombo->addItem("動画（透過WebMなど） (video)", "video");
    m_effectTypeCombo->addItem("音響効果のみ (sound)", "sound");
    m_effectTypeCombo->addItem("外部スクリプト実行 (script)", "script");
    effectLayout->addRow("演出の種類:", m_effectTypeCombo);
    connect(m_effectTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RewardEditorWidget::onEffectTypeChanged);

    // アセットファイル選択
    auto* imgLayout = new QHBoxLayout();
    m_imagePathEdit = new QLineEdit(this);
    m_imageSelectBtn = new QPushButton("参照...", this);
    imgLayout->addWidget(m_imagePathEdit);
    imgLayout->addWidget(m_imageSelectBtn);
    m_imagePathLabel = new QLabel("画像/動画ファイル:", this);
    effectLayout->addRow(m_imagePathLabel, imgLayout);
    connect(m_imageSelectBtn, &QPushButton::clicked, this, &RewardEditorWidget::selectImagePath);

    auto* audLayout = new QHBoxLayout();
    m_audioPathEdit = new QLineEdit(this);
    auto* audSelectBtn = new QPushButton("参照...", this);
    audLayout->addWidget(m_audioPathEdit);
    audLayout->addWidget(audSelectBtn);
    effectLayout->addRow("効果音ファイル:", audLayout);
    connect(audSelectBtn, &QPushButton::clicked, this, &RewardEditorWidget::selectAudioPath);

    m_durationSpin = new QSpinBox(this);
    m_durationSpin->setRange(1, 300);
    m_durationSpin->setValue(5);
    m_durationSpin->setSuffix(" 秒");
    effectLayout->addRow("表示・演出時間:", m_durationSpin);

    m_scaleSpin = new QSpinBox(this);
    m_scaleSpin->setRange(1, 100);
    m_scaleSpin->setValue(100);
    m_scaleSpin->setSuffix(" %");
    effectLayout->addRow("表示サイズ (1-100%):", m_scaleSpin);

    m_textEdit = new QLineEdit(this);
    m_textEdit->setPlaceholderText("例: {user}がたぬきを投げた！");
    effectLayout->addRow("吹き出し表示文字列:", m_textEdit);

    // --- 表示位置設定 ---
    m_positionPresetCombo = new QComboBox(this);
    m_positionPresetCombo->addItem("中央 (center)",       "center");
    m_positionPresetCombo->addItem("左上 (top_left)",     "top_left");
    m_positionPresetCombo->addItem("右上 (top_right)",    "top_right");
    m_positionPresetCombo->addItem("左下 (bottom_left)",  "bottom_left");
    m_positionPresetCombo->addItem("右下 (bottom_right)", "bottom_right");
    m_positionPresetCombo->addItem("カスタム",             "custom");
    effectLayout->addRow("表示位置:", m_positionPresetCombo);

    // 座標微調整（常に表示 - プリセット選択で自動更新、手動変更も可能）
    auto* coordWidget = new QWidget(this);
    auto* coordLayout = new QHBoxLayout(coordWidget);
    coordLayout->setContentsMargins(0, 0, 0, 0);
    coordLayout->addWidget(new QLabel("X:", coordWidget));
    m_positionXSpin = new QSpinBox(coordWidget);
    m_positionXSpin->setRange(0, 3840);
    m_positionXSpin->setValue(960);
    m_positionXSpin->setSuffix(" px");
    coordLayout->addWidget(m_positionXSpin);
    coordLayout->addSpacing(12);
    coordLayout->addWidget(new QLabel("Y:", coordWidget));
    m_positionYSpin = new QSpinBox(coordWidget);
    m_positionYSpin->setRange(0, 2160);
    m_positionYSpin->setValue(540);
    m_positionYSpin->setSuffix(" px");
    coordLayout->addWidget(m_positionYSpin);
    coordLayout->addStretch();
    effectLayout->addRow("  ↳ 中心座標 (px):", coordWidget);

    // プリセット選択時に中心座標を自動更新（1920x1080 基準）
    connect(m_positionPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
        const QString p = m_positionPresetCombo->currentData().toString();
        if      (p == "center")       { m_positionXSpin->setValue(960);  m_positionYSpin->setValue(540); }
        else if (p == "top_left")     { m_positionXSpin->setValue(200);  m_positionYSpin->setValue(150); }
        else if (p == "top_right")    { m_positionXSpin->setValue(1720); m_positionYSpin->setValue(150); }
        else if (p == "bottom_left")  { m_positionXSpin->setValue(200);  m_positionYSpin->setValue(930); }
        else if (p == "bottom_right") { m_positionXSpin->setValue(1720); m_positionYSpin->setValue(930); }
        // "custom" の場合は X/Y を変えない（ユーザが自由に入力）
    });

    rightLayout->addWidget(effectGroup);

    // 保存＆削除アクション
    m_testButton = new QPushButton("▶️ 演出をテスト再生 (OBS)", this);
    m_testButton->setStyleSheet("background-color: #9C27B0; color: white; font-weight: bold; padding: 8px; font-size: 13px; border-radius: 4px; margin-bottom: 5px;");
    m_testButton->setEnabled(false); // 報酬が選択されるまで無効
    connect(m_testButton, &QPushButton::clicked, this, &RewardEditorWidget::onTestClicked);
    rightLayout->addWidget(m_testButton);

    auto* actLayout = new QHBoxLayout();
    m_saveButton = new QPushButton("設定を保存", this);
    m_saveButton->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 6px;");
    connect(m_saveButton, &QPushButton::clicked, this, &RewardEditorWidget::onSaveClicked);

    m_deleteButton = new QPushButton("この報酬を削除", this);
    m_deleteButton->setStyleSheet("background-color: #E53935; color: white; padding: 6px;");
    connect(m_deleteButton, &QPushButton::clicked, this, &RewardEditorWidget::onDeleteClicked);

    actLayout->addWidget(m_saveButton);
    actLayout->addWidget(m_deleteButton);
    rightLayout->addLayout(actLayout);

    mainLayout->addLayout(rightLayout);
}

void RewardEditorWidget::reloadRewardsList()
{
    m_rewardsList->clear();
    
    // 1. まずローカルのSQLiteデータベースから取得して、初期表示する（即時表示で体感を良くする）
    QList<Reward> localList = m_app->rewardManager()->getAllRewards();
    for (const auto& r : localList) {
        auto* item = new QListWidgetItem(QString("🟡 [読込中] %1 (%2pt)").arg(r.name).arg(r.cost), m_rewardsList);
        item->setData(Qt::UserRole, r.id);
        item->setData(Qt::UserRole + 1, true); // ローカル設定あり
        item->setData(Qt::UserRole + 2, r.name);
        item->setData(Qt::UserRole + 3, r.cost);
        item->setForeground(QBrush(QColor("#A9A9B2"))); // 読み込み中は少し薄い色
    }

    // 2. Twitch連携情報がある場合、バックグラウンドで最新データを取得しマージする
    QString secretKey = "twitch_overlay_secret_key_2026";
    QString access = m_app->config()->loadSecureString("twitch_access_token", secretKey);
    QString broadcasterId = m_app->config()->get("twitch_broadcaster_id").toString();
    QString clientId = m_app->config()->get("twitch_client_id", TWITCH_GLOBAL_CLIENT_ID).toString();

    if (!access.isEmpty() && !broadcasterId.isEmpty()) {
        m_app->twitchAuth()->fetchCustomRewards(access, clientId, broadcasterId);
    } else {
        // 未連携の場合は、ローカルデータのみで「オフライン」として確定表示する
        m_rewardsList->clear();
        for (const auto& r : localList) {
            auto* item = new QListWidgetItem(QString("🟢 [設定済] %1 (%2pt)").arg(r.name).arg(r.cost), m_rewardsList);
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

    QString rewardId = item->data(Qt::UserRole).toString();
    bool isConfigured = item->data(Qt::UserRole + 1).toBool();

    m_editingEffects.clear();
    m_currentEffectIndex = -1;

    if (isConfigured) {
        // すでにローカルDBにある場合：DBから読み込む
        Reward r;
        if (m_app->rewardManager()->getReward(rewardId, r)) {
            m_idEdit->setText(r.id);
            m_nameEdit->setText(r.name);
            m_costSpin->setValue(r.cost);
            m_cooldownSpin->setValue(r.cooldown);
            m_enabledCheck->setChecked(r.enabled);

            int modeIndex = m_modeCombo->findData(r.mode);
            if (modeIndex >= 0) m_modeCombo->setCurrentIndex(modeIndex);

            m_editingEffects = r.effects;
        }
    } else {
        // Twitchにはあるが、ローカル未設定の場合：Twitchのデータでフォームを初期化
        QString name = item->data(Qt::UserRole + 2).toString();
        int cost = item->data(Qt::UserRole + 3).toInt();

        m_idEdit->setText(rewardId);
        m_nameEdit->setText(name);
        m_costSpin->setValue(cost);
        m_cooldownSpin->setValue(0);
        m_enabledCheck->setChecked(true);
        m_modeCombo->setCurrentIndex(0);
    }

    // 演出リストが空の場合は必ず1つデフォルトの演出を追加（最後の1つを削除させない設計に準拠）
    if (m_editingEffects.isEmpty()) {
        Effect eff;
        eff.type = "image";
        eff.duration = 5;
        m_editingEffects.append(eff);
    }

    // コンボボックスを更新して最初のエフェクトを表示
    m_currentEffectIndex = 0;
    updateEffectSelectorCombo();
    loadEffectFromBuffer(0);

    m_testButton->setEnabled(true);
}

void RewardEditorWidget::onSaveClicked()
{
    QString rewardId = m_idEdit->text().trimmed();
    QString name = m_nameEdit->text().trimmed();

    saveCurrentEffectToBuffer();

    // 全てのエフェクトに何らかの演出が設定されているか検証
    for (int i = 0; i < m_editingEffects.size(); ++i) {
        const auto& eff = m_editingEffects[i];
        if (eff.filePath.isEmpty() && eff.audioPath.isEmpty() && eff.text.isEmpty()) {
            QString msg = (eff.type == "script") 
                ? QString("演出 %1 のスクリプトファイルを選択してください。").arg(i + 1)
                : QString("演出 %1 の画像/動画、効果音、または表示文字列のいずれかを入力してください。").arg(i + 1);
            QMessageBox::warning(this, "演出効果未設定", msg);
            return;
        }
    }

    Reward r;
    r.id = rewardId;
    r.name = name;
    r.cost = m_costSpin->value();
    r.cooldown = m_cooldownSpin->value();
    r.mode = m_modeCombo->currentData().toString();
    r.enabled = m_enabledCheck->isChecked();
    r.allowedRoles.append("everyone"); // デフォルト値

    r.effects = m_editingEffects;

    if (m_app->rewardManager()->saveReward(r)) {
        QMessageBox::information(this, "成功", "報酬設定をデータベースに保存しました。");
        m_testButton->setEnabled(true);
        reloadRewardsList();
    } else {
        QMessageBox::critical(this, "失敗", "設定の保存に失敗しました。");
    }
}

void RewardEditorWidget::onDeleteClicked()
{
    QString rewardId = m_idEdit->text().trimmed();
    if (rewardId.isEmpty()) return;

    auto result = QMessageBox::question(this, "確認", "この報酬演出を完全に削除しますか？", QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes) {
        if (m_app->rewardManager()->deleteReward(rewardId)) {
            QMessageBox::information(this, "成功", "報酬設定を削除しました。");
            onNewClicked();
            reloadRewardsList();
        } else {
            QMessageBox::critical(this, "失敗", "報酬の削除に失敗しました。");
        }
    }
}

void RewardEditorWidget::onNewClicked()
{
    m_idEdit->clear();
    m_nameEdit->clear();
    m_costSpin->setValue(500);
    m_cooldownSpin->setValue(0);
    m_modeCombo->setCurrentIndex(0);
    m_enabledCheck->setChecked(true);

    m_imagePathEdit->clear();
    m_audioPathEdit->clear();
    m_durationSpin->setValue(5);
    m_textEdit->clear();

    m_testButton->setEnabled(false);
}

void RewardEditorWidget::selectImagePath()
{
    QString type = m_effectTypeCombo->currentData().toString();
    QString path;
    if (type == "script") {
        path = QFileDialog::getOpenFileName(this, "スクリプトファイルを選択", "", "Script Files (*.pl *.cgi *.php);;All Files (*)");
    } else {
        path = QFileDialog::getOpenFileName(this, "画像または動画ファイルを選択", "", "Media Files (*.png *.jpg *.jpeg *.gif *.webm *.mp4)");
    }
    if (!path.isEmpty()) {
        m_imagePathEdit->setText(QDir::toNativeSeparators(path));
    }
}

void RewardEditorWidget::selectAudioPath()
{
    QString path = QFileDialog::getOpenFileName(this, "効果音ファイルを選択", "", "Audio Files (*.mp3 *.wav)");
    if (!path.isEmpty()) {
        m_audioPathEdit->setText(path);
    }
}

void RewardEditorWidget::onSyncClicked()
{
    QString secretKey = "twitch_overlay_secret_key_2026";
    QString access = m_app->config()->loadSecureString("twitch_access_token", secretKey);
    QString broadcasterId = m_app->config()->get("twitch_broadcaster_id").toString();
    QString clientId = m_app->config()->get("twitch_client_id", TWITCH_GLOBAL_CLIENT_ID).toString();

    if (access.isEmpty() || broadcasterId.isEmpty()) {
        QMessageBox::warning(this, "未連携", "Twitch 連携が行われていません。「システム設定」タブから Twitch 連携を完了してください。");
        return;
    }

    QMessageBox::information(this, "同期中", "Twitch から最新のチャンネルポイント報酬情報を取得しています...");
    m_app->twitchAuth()->fetchCustomRewards(access, clientId, broadcasterId);
}

void RewardEditorWidget::onCustomRewardsFetched(const QJsonArray& rewards)
{
    // 現在のローカルデータベースの状態を取得
    QList<Reward> localList = m_app->rewardManager()->getAllRewards();
    QMap<QString, Reward> localMap;
    for (const auto& r : localList) {
        localMap.insert(r.id, r);
    }

    m_rewardsList->clear();

    // 取得したTwitchの報酬一覧をループ
    QSet<QString> twitchIds;
    for (int i = 0; i < rewards.size(); ++i) {
        QJsonObject rewardObj = rewards.at(i).toObject();
        QString id = rewardObj.value("id").toString();
        QString title = rewardObj.value("title").toString();
        int cost = rewardObj.value("cost").toInt();
        twitchIds.insert(id);

        if (localMap.contains(id)) {
            // ① ローカルに設定済みのTwitch報酬：緑色で表示
            auto* item = new QListWidgetItem(QString("🟢 [設定済] %1 (%2pt)").arg(title).arg(cost), m_rewardsList);
            item->setData(Qt::UserRole, id);
            item->setData(Qt::UserRole + 1, true); // 設定済み
            item->setData(Qt::UserRole + 2, title);
            item->setData(Qt::UserRole + 3, cost);
            item->setForeground(QBrush(QColor("#FFFFFF"))); // 明るい白文字
        } else {
            // ② ローカルに設定されていないTwitch報酬：グレー（薄色）で表示
            auto* item = new QListWidgetItem(QString("⚪ [未設定] %1 (%2pt)").arg(title).arg(cost), m_rewardsList);
            item->setData(Qt::UserRole, id);
            item->setData(Qt::UserRole + 1, false); // 未設定
            item->setData(Qt::UserRole + 2, title);
            item->setData(Qt::UserRole + 3, cost);
            item->setForeground(QBrush(QColor("#A9A9B2"))); // グレーアウト文字
        }
    }

    // ③ ローカルデータベースには存在するが、Twitch APIからは取得できなかったもの（オフライン状態等）：黄色
    for (const auto& r : localList) {
        if (!twitchIds.contains(r.id)) {
            auto* item = new QListWidgetItem(QString("🟡 [オフライン] %1 (%2pt)").arg(r.name).arg(r.cost), m_rewardsList);
            item->setData(Qt::UserRole, r.id);
            item->setData(Qt::UserRole + 1, true); // 設定自体はある
            item->setData(Qt::UserRole + 2, r.name);
            item->setData(Qt::UserRole + 3, r.cost);
            item->setForeground(QBrush(QColor("#E6A23C"))); // オレンジ/イエロー
        }
    }
}

void RewardEditorWidget::onCustomRewardsFetchFailed(const QString& errorMessage)
{
    // 取得に失敗した場合は、ローカルデータのみで確定表示にする
    QList<Reward> localList = m_app->rewardManager()->getAllRewards();
    m_rewardsList->clear();
    for (const auto& r : localList) {
        auto* item = new QListWidgetItem(QString("🟢 [設定済] %1 (%2pt)").arg(r.name).arg(r.cost), m_rewardsList);
        item->setData(Qt::UserRole, r.id);
        item->setData(Qt::UserRole + 1, true);
        item->setData(Qt::UserRole + 2, r.name);
        item->setData(Qt::UserRole + 3, r.cost);
        item->setForeground(QBrush(QColor("#FFFFFF")));
    }
    
    QMessageBox::warning(this, "同期エラー", errorMessage + "\nローカルの保存データのみを表示します。");
}

void RewardEditorWidget::onEffectSelectorChanged(int index)
{
    if (index < 0 || index >= m_editingEffects.size()) return;

    // 現在の演出をバッファに保存
    saveCurrentEffectToBuffer();

    // 新しく選択された演出をロード
    loadEffectFromBuffer(index);
}

void RewardEditorWidget::onAddEffectClicked()
{
    // 現在の入力を保存
    saveCurrentEffectToBuffer();

    // デフォルトの新しい演出を追加
    Effect eff;
    eff.type = "image";
    eff.duration = 5;
    m_editingEffects.append(eff);

    // インデックスを新しく追加されたものに進める
    m_currentEffectIndex = m_editingEffects.size() - 1;
    updateEffectSelectorCombo();
    loadEffectFromBuffer(m_currentEffectIndex);

    QMessageBox::information(this, "演出追加", QString("演出 %1 を新規追加しました。\n種類とアセットファイルを設定してください。").arg(m_editingEffects.size()));
}

void RewardEditorWidget::onDeleteEffectClicked()
{
    if (m_editingEffects.size() <= 1) {
        // 最後の1個は絶対に削除させない
        QMessageBox::warning(this, "削除不可", "報酬には最低でも1つの演出効果が必要です（最後のエフェクトは削除できません）。");
        return;
    }

    auto result = QMessageBox::question(this, "演出削除", QString("演出 %1 をリストから削除しますか？").arg(m_currentEffectIndex + 1), QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes) {
        m_editingEffects.removeAt(m_currentEffectIndex);

        // インデックスを範囲内に調整
        if (m_currentEffectIndex >= m_editingEffects.size()) {
            m_currentEffectIndex = m_editingEffects.size() - 1;
        }

        updateEffectSelectorCombo();
        loadEffectFromBuffer(m_currentEffectIndex);
    }
}

void RewardEditorWidget::saveCurrentEffectToBuffer()
{
    if (m_currentEffectIndex >= 0 && m_currentEffectIndex < m_editingEffects.size()) {
        Effect& eff = m_editingEffects[m_currentEffectIndex];
        eff.type = m_effectTypeCombo->currentData().toString();
        if (eff.type == "sound") {
            eff.filePath = ""; // 音響効果のみの場合は画像/動画パスをクリア
        } else {
            eff.filePath = m_imagePathEdit->text().trimmed();
        }
        eff.audioPath = m_audioPathEdit->text().trimmed();
        eff.duration = m_durationSpin->value();
        eff.scale = m_scaleSpin->value();
        eff.text = m_textEdit->text().trimmed();
        // 表示位置
        eff.position.preset  = m_positionPresetCombo->currentData().toString();
        eff.position.offsetX = m_positionXSpin->value();
        eff.position.offsetY = m_positionYSpin->value();
    }
}

void RewardEditorWidget::loadEffectFromBuffer(int index)
{
    if (index >= 0 && index < m_editingEffects.size()) {
        m_currentEffectIndex = index;
        const Effect& eff = m_editingEffects[index];
        
        int typeIndex = m_effectTypeCombo->findData(eff.type);
        if (typeIndex >= 0) m_effectTypeCombo->setCurrentIndex(typeIndex);
        
        m_imagePathEdit->setText(eff.filePath);
        m_audioPathEdit->setText(eff.audioPath);
        m_durationSpin->setValue(eff.duration);
        m_scaleSpin->setValue(eff.scale);
        m_textEdit->setText(eff.text);

        // 表示位置の復元
        // blockSignals でプリセット変更時の座標自動更新を抑制し、保存済みの座標を優先する
        m_positionPresetCombo->blockSignals(true);
        int posIndex = m_positionPresetCombo->findData(eff.position.preset);
        m_positionPresetCombo->setCurrentIndex(posIndex >= 0 ? posIndex : 0);
        m_positionPresetCombo->blockSignals(false);
        // 保存済み座標を復元（0 の場合はセンターのデフォルト値を使用）
        m_positionXSpin->setValue(eff.position.offsetX > 0 ? eff.position.offsetX : 960);
        m_positionYSpin->setValue(eff.position.offsetY > 0 ? eff.position.offsetY : 540);
        
        // 演出の種類に応じたUIの有効・無効化状態の更新
        onEffectTypeChanged(m_effectTypeCombo->currentIndex());

        // 最後の1個の場合は削除ボタンを無効にする（ダブル安全策）
        m_deleteEffectBtn->setEnabled(m_editingEffects.size() > 1);
    }
}

void RewardEditorWidget::updateEffectSelectorCombo()
{
    m_effectSelectorCombo->blockSignals(true);
    m_effectSelectorCombo->clear();
    for (int i = 0; i < m_editingEffects.size(); ++i) {
        const Effect& eff = m_editingEffects[i];
        QString typeLabel = "画像";
        if (eff.type == "video") typeLabel = "動画";
        else if (eff.type == "sound") typeLabel = "音響";
        else if (eff.type == "script") typeLabel = "スクリプト";
        
        m_effectSelectorCombo->addItem(QString("演出 %1: [%2]").arg(i + 1).arg(typeLabel));
    }
    if (m_currentEffectIndex >= 0 && m_currentEffectIndex < m_editingEffects.size()) {
        m_effectSelectorCombo->setCurrentIndex(m_currentEffectIndex);
    }
    m_effectSelectorCombo->blockSignals(false);
    
    m_deleteEffectBtn->setEnabled(m_editingEffects.size() > 1);
}

void RewardEditorWidget::onTestClicked()
{
    QString rewardId = m_idEdit->text().trimmed();
    if (rewardId.isEmpty()) {
        QMessageBox::warning(this, "テストエラー", "テスト再生する報酬が選択されていません。");
        return;
    }

    // 現在のUI入力内容（変更中のdurationなど）をバッファに反映
    saveCurrentEffectToBuffer();

    if (m_editingEffects.isEmpty()) {
        QMessageBox::warning(this, "テストエラー", "演出効果が設定されていません。");
        return;
    }

    // 現在の未保存状態を含むRewardをUIから直接組み立ててテスト再生
    Reward r;
    r.id   = rewardId;
    r.name = m_nameEdit->text().trimmed();
    r.mode = m_modeCombo->currentData().toString();
    r.effects = m_editingEffects;

    m_app->queueManager()->enqueueReward(r, "テスト配信者 (Test Streamer)", QDateTime::currentDateTime());
}

void RewardEditorWidget::onEffectTypeChanged(int index)
{
    Q_UNUSED(index);
    QString type = m_effectTypeCombo->currentData().toString();
    bool isSoundOnly = (type == "sound");
    bool isScript = (type == "script");

    // ラベルの動的切り替え
    if (isScript) {
        m_imagePathLabel->setText("スクリプトファイル (*.pl, *.cgi, *.php):");
    } else {
        m_imagePathLabel->setText("画像/動画ファイル:");
    }

    m_imagePathEdit->setEnabled(!isSoundOnly);
    m_imageSelectBtn->setEnabled(!isSoundOnly);
    
    // スクリプトまたは音響のみの場合は画面表示関連（スケール、位置、テキスト）を無効化
    bool isVisual = (!isSoundOnly && !isScript);
    m_scaleSpin->setEnabled(isVisual);
    m_positionPresetCombo->setEnabled(isVisual);
    m_positionXSpin->setEnabled(isVisual && m_positionPresetCombo->currentData().toString() == "custom");
    m_positionYSpin->setEnabled(isVisual && m_positionPresetCombo->currentData().toString() == "custom");
    m_textEdit->setEnabled(isVisual);
    m_audioPathEdit->setEnabled(!isScript); // スクリプト以外はオーディオ設定を許可

    if (isSoundOnly) {
        m_imagePathEdit->clear();
    }
}

void RewardEditorWidget::selectRewardAndEffect(const QString& rewardId, int effectIndex)
{
    for (int i = 0; i < m_rewardsList->count(); ++i) {
        QListWidgetItem* item = m_rewardsList->item(i);
        if (item && item->data(Qt::UserRole).toString() == rewardId) {
            m_rewardsList->setCurrentItem(item);
            onRewardSelected(item);

            if (effectIndex >= 0 && effectIndex < m_editingEffects.size()) {
                if (effectIndex < m_effectSelectorCombo->count()) {
                    m_effectSelectorCombo->setCurrentIndex(effectIndex);
                }
            }
            break;
        }
    }
}

