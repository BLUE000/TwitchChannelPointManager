#include "RewardEditorWidget.hpp"
#include "../core/Application.hpp"
#include "../reward/RewardManager.hpp"
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

    m_newButton = new QPushButton("新規報酬の登録", this);
    m_newButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    connect(m_newButton, &QPushButton::clicked, this, &RewardEditorWidget::onNewClicked);
    leftLayout->addWidget(m_newButton);

    mainLayout->addLayout(leftLayout);

    // 2. 右側: 詳細編集フォーム
    auto* rightLayout = new QVBoxLayout();
    auto* formGroup = new QGroupBox("報酬情報の設定", this);
    auto* formLayout = new QFormLayout(formGroup);

    m_idEdit = new QLineEdit(this);
    m_idEdit->setPlaceholderText("Twitchのカスタム報酬IDをコピペ");
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

    m_effectTypeCombo = new QComboBox(this);
    m_effectTypeCombo->addItem("画像のみ (image)", "image");
    m_effectTypeCombo->addItem("動画（透過WebMなど） (video)", "video");
    m_effectTypeCombo->addItem("音響効果のみ (sound)", "sound");
    effectLayout->addRow("演出の種類:", m_effectTypeCombo);

    // アセットファイル選択
    auto* imgLayout = new QHBoxLayout();
    m_imagePathEdit = new QLineEdit(this);
    auto* imgSelectBtn = new QPushButton("参照...", this);
    imgLayout->addWidget(m_imagePathEdit);
    imgLayout->addWidget(imgSelectBtn);
    effectLayout->addRow("画像/動画ファイル:", imgLayout);
    connect(imgSelectBtn, &QPushButton::clicked, this, &RewardEditorWidget::selectImagePath);

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

    m_textEdit = new QLineEdit(this);
    m_textEdit->setPlaceholderText("例: {user}がたぬきを投げた！");
    effectLayout->addRow("吹き出し表示文字列:", m_textEdit);

    rightLayout->addWidget(effectGroup);

    // 保存＆削除アクション
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
    QList<Reward> list = m_app->rewardManager()->getAllRewards();
    for (const auto& r : list) {
        auto* item = new QListWidgetItem(QString("%1 (%2)").arg(r.name).arg(r.cost), m_rewardsList);
        item->setData(Qt::UserRole, r.id);
    }
}

void RewardEditorWidget::onRewardSelected(QListWidgetItem* item)
{
    if (!item) return;

    QString rewardId = item->data(Qt::UserRole).toString();
    Reward r;
    if (m_app->rewardManager()->getReward(rewardId, r)) {
        m_idEdit->setText(r.id);
        m_nameEdit->setText(r.name);
        m_costSpin->setValue(r.cost);
        m_cooldownSpin->setValue(r.cooldown);
        m_enabledCheck->setChecked(r.enabled);

        int modeIndex = m_modeCombo->findData(r.mode);
        if (modeIndex >= 0) m_modeCombo->setCurrentIndex(modeIndex);

        // エフェクトがあれば簡易ロード
        if (!r.effects.isEmpty()) {
            Effect eff = r.effects.first();
            int typeIndex = m_effectTypeCombo->findData(eff.type);
            if (typeIndex >= 0) m_effectTypeCombo->setCurrentIndex(typeIndex);
            
            m_imagePathEdit->setText(eff.filePath);
            m_audioPathEdit->setText(eff.audioPath);
            m_durationSpin->setValue(eff.duration);
            m_textEdit->setText(eff.text);
        } else {
            m_imagePathEdit->clear();
            m_audioPathEdit->clear();
            m_durationSpin->setValue(5);
            m_textEdit->clear();
        }
    }
}

void RewardEditorWidget::onSaveClicked()
{
    QString rewardId = m_idEdit->text().trimmed();
    QString name = m_nameEdit->text().trimmed();

    if (rewardId.isEmpty() || name.isEmpty()) {
        QMessageBox::warning(this, "入力エラー", "報酬IDと報酬名は必ず入力してください。");
        return;
    }

    Reward r;
    r.id = rewardId;
    r.name = name;
    r.cost = m_costSpin->value();
    r.cooldown = m_cooldownSpin->value();
    r.mode = m_modeCombo->currentData().toString();
    r.enabled = m_enabledCheck->isChecked();
    r.allowedRoles.append("everyone"); // デフォルト値

    // 単一エフェクトの構築
    Effect eff;
    eff.type = m_effectTypeCombo->currentData().toString();
    eff.filePath = m_imagePathEdit->text().trimmed();
    eff.audioPath = m_audioPathEdit->text().trimmed();
    eff.duration = m_durationSpin->value();
    eff.text = m_textEdit->text().trimmed();
    
    // 画像/動画/音声のいずれかがセットされていること
    if (eff.filePath.isEmpty() && eff.audioPath.isEmpty() && eff.text.isEmpty()) {
        QMessageBox::warning(this, "エフェクト未設定", "画像、効果音、テキストのいずれか一つは設定してください。");
        return;
    }

    r.effects.append(eff);

    if (m_app->rewardManager()->saveReward(r)) {
        QMessageBox::information(this, "成功", "報酬設定をデータベースに保存しました。");
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
}

void RewardEditorWidget::selectImagePath()
{
    QString path = QFileDialog::getOpenFileName(this, "画像または動画ファイルを選択", "", "Media Files (*.png *.jpg *.jpeg *.gif *.webm *.mp4)");
    if (!path.isEmpty()) {
        m_imagePathEdit->setText(path);
    }
}

void RewardEditorWidget::selectAudioPath()
{
    QString path = QFileDialog::getOpenFileName(this, "効果音ファイルを選択", "", "Audio Files (*.mp3 *.wav)");
    if (!path.isEmpty()) {
        m_audioPathEdit->setText(path);
    }
}
