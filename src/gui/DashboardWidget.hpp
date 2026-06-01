#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include "../core/Logger.hpp"

class Application;

class QGroupBox;

class DashboardWidget : public QWidget {
    Q_OBJECT
private:
    Application* m_app;
    
    QGroupBox* m_infoGroup;
    QGroupBox* m_logGroup;
    
    QLabel* m_esLabel;
    QLabel* m_obsLabel;
    QLabel* m_todayLabel;
    QLabel* m_statusLabel;
    QLabel* m_clientsLabel;
    QLabel* m_todayCountLabel;
    
    QPushButton* m_toggleConnButton;
    QPushButton* m_panicButton;
    QPushButton* m_clearQueueBtn;
    QListWidget* m_logListWidget;

public:
    explicit DashboardWidget(Application* app, QWidget* parent = nullptr);
    ~DashboardWidget() = default;

    // GUIデータの最新状態更新
    void refreshStats();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onToggleConnection();
    void onPanicClicked();
    void onNewLogMessage(LogLevel level, const QString& message);
    void updateClientCount(int count);

private:
    void setupUi();
    void retranslateUi();
};
