#include "Logger.hpp"
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <iostream>

Logger* Logger::s_instance = nullptr;

Logger::Logger(QObject* parent)
    : QObject(parent)
{
}

Logger::~Logger()
{
    shutdown();
}

Logger* Logger::instance()
{
    if (!s_instance) {
        s_instance = new Logger();
    }
    return s_instance;
}

bool Logger::initialize(const QString& logPath)
{
    QMutexLocker locker(&m_mutex);
    m_logPath = logPath;
    m_logFile.setFileName(m_logPath);

    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed to open log file:" << m_logPath;
        return false;
    }

    return true;
}

void Logger::shutdown()
{
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void Logger::log(LogLevel level, const QString& message)
{
    QMutexLocker locker(&m_mutex);

    QString levelStr;
    switch (level) {
        case LogLevel::Debug:   levelStr = "DEBUG";   break;
        case LogLevel::Info:    levelStr = "INFO ";   break;
        case LogLevel::Warning: levelStr = "WARN ";   break;
        case LogLevel::Error:   levelStr = "ERROR";   break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString formattedMsg = QString("[%1] [%2] %3").arg(timestamp).arg(levelStr).arg(message);

    // ファイルへ書き込み
    if (m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << formattedMsg << "\n";
        stream.flush();
    }

    // 標準出力（デバッグ用コンソール）へ出力
    if (level == LogLevel::Error || level == LogLevel::Warning) {
        std::cerr << formattedMsg.toStdString() << std::endl;
    } else {
        std::cout << formattedMsg.toStdString() << std::endl;
    }

    emit newLogMessage(level, formattedMsg);
}
