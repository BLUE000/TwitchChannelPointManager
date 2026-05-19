#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QComboBox>

class Application;

class StatisticsWidget : public QWidget {
    Q_OBJECT
private:
    Application* m_app;
    QTabWidget* m_tabWidget;
    QTableWidget* m_rankingTable;
    QTableWidget* m_userStatsTable;
    QComboBox* m_periodCombo;
    QPushButton* m_refreshButton;
    QPushButton* m_exportCsvButton;
    QPushButton* m_resetButton;

    QPushButton* m_rankingBgBtn;
    QPushButton* m_rankingColorBtn;
    QPushButton* m_userStatsBgBtn;
    QPushButton* m_userStatsColorBtn;

public:
    explicit StatisticsWidget(Application* app, QWidget* parent = nullptr);
    ~StatisticsWidget() = default;

    void refreshRanking();

private slots:
    void onExportCsvClicked();
    void onResetClicked();
    void onChangeRankingBg();
    void onChangeRankingColor();
    void onChangeUserStatsBg();
    void onChangeUserStatsColor();

private:
    void setupUi();
    void applyTableStyle(QTableWidget* table, const QString& prefix);
    void loadStyles();
};
