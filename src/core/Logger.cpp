#include "Logger.hpp"
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <iostream>

#include <QMutex>

Logger* Logger::s_instance = nullptr;
static QMutex s_instanceMutex;

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
    // スレッドセーフなシングルトン初期化
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) { // ダブルチェックロック
            s_instance = new Logger();
        }
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
    QString formattedMsg;

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
        formattedMsg = QString("[%1] [%2] %3").arg(timestamp).arg(levelStr).arg(message);

        // ファイルへ書き込み
        if (m_logFile.isOpen()) {
            QTextStream stream(&m_logFile);
            stream << formattedMsg << "\n";
            stream.flush();
        }

        // コンソール出力（UTF-8で正しく出力。mutexを保持したまま行う）
        QByteArray utf8 = (formattedMsg + "\n").toUtf8();
        if (level == LogLevel::Error || level == LogLevel::Warning) {
            std::cerr.write(utf8.constData(), utf8.size());
            std::cerr.flush();
        } else {
            std::cout.write(utf8.constData(), utf8.size());
            std::cout.flush();
        }
    } // ← mutex をここで解放してから emit

    // emit は mutex 解放後に行う（スロットが log() を再呼出ししてもデッドロックしない）
    emit newLogMessage(level, formattedMsg);
}
