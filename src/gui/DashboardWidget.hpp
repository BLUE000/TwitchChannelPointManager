#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include "../core/Logger.hpp"

class Application;

class DashboardWidget : public QWidget {
    Q_OBJECT
private:
    Application* m_app;
    
    QLabel* m_statusLabel;
    QLabel* m_clientsLabel;
    QLabel* m_todayCountLabel;
    
    QPushButton* m_toggleConnButton;
    QPushButton* m_panicButton;
    QListWidget* m_logListWidget;

public:
    explicit DashboardWidget(Application* app, QWidget* parent = nullptr);
    ~DashboardWidget() = default;

    // GUIデータの最新状態更新
    void refreshStats();

private slots:
    void onToggleConnection();
    void onPanicClicked();
    void onNewLogMessage(LogLevel level, const QString& message);
    void updateClientCount(int count);

private:
    void setupUi();
};
