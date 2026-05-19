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

public:
    explicit StatisticsWidget(Application* app, QWidget* parent = nullptr);
    ~StatisticsWidget() = default;

    void refreshRanking();

private slots:
    void onExportCsvClicked();
    void onResetClicked();

private:
    void setupUi();
};
