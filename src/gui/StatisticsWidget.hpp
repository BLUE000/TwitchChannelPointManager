#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>

class Application;

class StatisticsWidget : public QWidget {
    Q_OBJECT
private:
    Application* m_app;
    QTableWidget* m_rankingTable;
    QPushButton* m_refreshButton;

public:
    explicit StatisticsWidget(Application* app, QWidget* parent = nullptr);
    ~StatisticsWidget() = default;

    void refreshRanking();

private:
    void setupUi();
};
