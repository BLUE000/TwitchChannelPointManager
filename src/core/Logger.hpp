#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QMutex>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger : public QObject {
    Q_OBJECT
private:
    QString m_logPath;
    mutable QMutex m_mutex;
    QFile m_logFile;

    static Logger* s_instance;

    explicit Logger(QObject* parent = nullptr);

public:
    static Logger* instance();
    ~Logger();

    bool initialize(const QString& logPath);
    void shutdown();

    void log(LogLevel level, const QString& message);

signals:
    void newLogMessage(LogLevel level, const QString& formattedMessage);
};

// ログ出力のショートカットマクロ
#define LOG_DEBUG(msg) Logger::instance()->log(LogLevel::Debug, msg)
#define LOG_INFO(msg)  Logger::instance()->log(LogLevel::Info, msg)
#define LOG_WARN(msg)  Logger::instance()->log(LogLevel::Warning, msg)
#define LOG_ERROR(msg) Logger::instance()->log(LogLevel::Error, msg)
