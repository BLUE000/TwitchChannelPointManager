#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QJsonArray>
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

    // 複数演出用コントロール
    QComboBox* m_effectSelectorCombo;
    QPushButton* m_addEffectBtn;
    QPushButton* m_deleteEffectBtn;

    // 外部スクリプト専用モード用コントロール
    QCheckBox* m_isExternalScriptOnlyCb;
    QWidget* m_normalEffectConfigWidget;
    QWidget* m_externalScriptConfigWidget;

    QLineEdit* m_htmlPathEdit;
    QPushButton* m_htmlSelectBtn;
    QLineEdit* m_perlScriptPathEdit;
    QPushButton* m_perlScriptSelectBtn;
    QLineEdit* m_phpScriptPathEdit;
    QPushButton* m_phpScriptSelectBtn;

    QComboBox* m_effectTypeCombo;
    QLabel* m_imagePathLabel;
    QLineEdit* m_imagePathEdit;
    QPushButton* m_imageSelectBtn; // 画像参照ボタンへのポインタを追加
    QLineEdit* m_audioPathEdit;
    QSpinBox* m_durationSpin;
    QSpinBox* m_scaleSpin;    // 表示サイズ (1～100 %)
    QLineEdit* m_textEdit;

    // 表示位置コントロール
    QComboBox* m_positionPresetCombo;
    QSpinBox*  m_positionXSpin;
    QSpinBox*  m_positionYSpin;
    
    QPushButton* m_saveButton;
    QPushButton* m_deleteButton;
    QPushButton* m_newButton;
    QPushButton* m_syncButton;
    QPushButton* m_testButton;

    // 編集中の演出リストと現在選択中のインデックスのバッファ
    QList<Effect> m_editingEffects;
    int m_currentEffectIndex = -1;
    
public:
    explicit RewardEditorWidget(Application* app, QWidget* parent = nullptr);
    ~RewardEditorWidget() = default;
 
    void reloadRewardsList();
    void selectRewardAndEffect(const QString& rewardId, int effectIndex = 0);
 
private slots:
    void onRewardSelected(QListWidgetItem* item);
    void onSaveClicked();
    void onDeleteClicked();
    void onNewClicked();
    void onSyncClicked();
    void onTestClicked();
    void onCustomRewardsFetched(const QJsonArray& rewards);
    void onCustomRewardsFetchFailed(const QString& errorMessage);
    
    void onEffectSelectorChanged(int index);
    void onEffectTypeChanged(int index); // 「演出の種類」切り替えスロットを追加
    void onAddEffectClicked();
    void onDeleteEffectClicked();
    
    // パス参照ダイアログを開く
    void selectImagePath();
    void selectAudioPath();
    void selectHtmlPath();
    void selectPerlScriptPath();
    void selectPhpScriptPath();
    void onExternalScriptOnlyToggled(bool checked);

private:
    void setupUi();
    void saveCurrentEffectToBuffer();
    void loadEffectFromBuffer(int index);
    void updateEffectSelectorCombo();
};
