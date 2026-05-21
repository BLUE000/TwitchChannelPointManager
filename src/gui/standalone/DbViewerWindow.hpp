#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QList>
#include "../../reward/Reward.hpp"

class Database;

class DbViewerWindow : public QMainWindow {
    Q_OBJECT
private:
    Database* m_db;
    QList<Reward> m_rewards;

    // 現在選択されている報酬IDと演出インデックス
    QString m_selectedRewardId;
    int m_selectedEffectIndex;

    // GUI部品 - 左ペイン (一覧 & 検索)
    QTableWidget* m_tableWidget;
    QLineEdit* m_searchEdit;
    QPushButton* m_refreshButton;
    QPushButton* m_newRewardButton;

    // GUI部品 - 右ペイン (編集フォーム)
    QGroupBox* m_formGroup;
    QLineEdit* m_rewardIdEdit;
    QLineEdit* m_rewardNameEdit;
    QSpinBox* m_rewardCostSpin;
    QSpinBox* m_rewardCooldownSpin;
    QComboBox* m_rewardModeCombo;
    QCheckBox* m_rewardEnabledCheck;

    QComboBox* m_effectTypeCombo;
    QLineEdit* m_filePathEdit;
    QPushButton* m_filePathSelectBtn;
    QLineEdit* m_audioPathEdit;
    QPushButton* m_audioPathSelectBtn;
    QSpinBox* m_durationSpin;
    QSpinBox* m_volumeSpin;
    QSpinBox* m_scaleSpin;
    QLineEdit* m_textEdit;
    QComboBox* m_positionPresetCombo;
    QSpinBox* m_positionXSpin;
    QSpinBox* m_positionYSpin;
    QComboBox* m_animationCombo;

    QPushButton* m_saveButton;
    QPushButton* m_addEffectButton;
    QPushButton* m_deleteEffectButton;
    QPushButton* m_deleteRewardButton;

public:
    explicit DbViewerWindow(QWidget* parent = nullptr);
    ~DbViewerWindow();

    bool initializeDb(const QString& dbPath);

private slots:
    void refreshData();
    void onSearchTextChanged(const QString& text);
    void onTableSelectionChanged();
    
    // アクション
    void onNewRewardClicked();
    void onAddEffectClicked();
    void onSaveClicked();
    void onDeleteEffectClicked();
    void onDeleteRewardClicked();
    
    // ダイアログ
    void selectFilePath();
    void selectAudioPath();
    void onEffectTypeChanged(int index);

private:
    void setupUi();
    void clearForm();
    void loadRewardToForm(const Reward& r, int effectIndex);
    Reward* findRewardById(const QString& id);
};
