#include "FileManager.hpp"
#include "../core/Logger.hpp"
#include <QUuid>
#include <QFileInfo>
#include <QDir>

FileManager::FileManager(int httpPort, QObject* parent)
    : QObject(parent)
    , m_httpPort(httpPort)
{
}

QString FileManager::registerAsset(const QString& absolutePath)
{
    if (absolutePath.isEmpty()) {
        return "";
    }

    // パスの標準化
    QString cleanPath = QDir::cleanPath(absolutePath);

    QMutexLocker locker(&m_mutex);

    // 既に登録済みであれば、既存のアセットURLを返す
    if (m_filePathToUuid.contains(cleanPath)) {
        QString assetId = m_filePathToUuid.value(cleanPath);
        return QString("http://localhost:%1/assets/%2").arg(m_httpPort).arg(assetId);
    }

    // 拡張子の抽出
    QFileInfo fileInfo(cleanPath);
    QString suffix = fileInfo.suffix();
    
    // 新しい UUID を生成
    QString uuid = QUuid::createUuid().toString(QUuid::IdHeaderType::WithoutBraces);
    QString assetId = suffix.isEmpty() ? uuid : QString("%1.%2").arg(uuid).arg(suffix);

    // マップへの登録
    m_uuidToFilePath.insert(assetId, cleanPath);
    m_filePathToUuid.insert(cleanPath, assetId);

    LOG_INFO(QString("Registered asset: %1 -> served as /assets/%2").arg(cleanPath).arg(assetId));

    return QString("http://localhost:%1/assets/%2").arg(m_httpPort).arg(assetId);
}

QString FileManager::getRealPath(const QString& assetId) const
{
    QMutexLocker locker(&m_mutex);
    return m_uuidToFilePath.value(assetId, "");
}

bool FileManager::hasAsset(const QString& assetId) const
{
    QMutexLocker locker(&m_mutex);
    return m_uuidToFilePath.contains(assetId);
}

void FileManager::setHttpPort(int port)
{
    QMutexLocker locker(&m_mutex);
    m_httpPort = port;
}
