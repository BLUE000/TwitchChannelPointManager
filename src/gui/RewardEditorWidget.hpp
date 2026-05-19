#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include "../reward/Reward.hpp"

class Application;

class RewardEditorWidget : public QWidget {
    Q_OBJECT
private:
    Application* m_app;
    
    QListWidget* m_rewardsList;
    
    QLineEdit* m_idEdit;
    QLineEdit* m_nameEdit;
    QSpinBox* m_costSpin;
    QSpinBox* m_cooldownSpin;
    QComboBox* m_modeCombo;
    QCheckBox* m_enabledCheck;

    // 単一エフェクトの編集用フィールド (簡易統合UI)
    QComboBox* m_effectTypeCombo;
    QLineEdit* m_imagePathEdit;
    QLineEdit* m_audioPathEdit;
    QSpinBox* m_durationSpin;
    QLineEdit* m_textEdit;
    
    QPushButton* m_saveButton;
    QPushButton* m_deleteButton;
    QPushButton* m_newButton;
    
public:
    explicit RewardEditorWidget(Application* app, QWidget* parent = nullptr);
    ~RewardEditorWidget() = default;

    void reloadRewardsList();

private slots:
    void onRewardSelected(QListWidgetItem* item);
    void onSaveClicked();
    void onDeleteClicked();
    void onNewClicked();
    
    // パス参照ダイアログを開く
    void selectImagePath();
    void selectAudioPath();

private:
    void setupUi();
};
