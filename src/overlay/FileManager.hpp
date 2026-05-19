#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QMutex>

class FileManager : public QObject {
    Q_OBJECT
private:
    QMap<QString, QString> m_uuidToFilePath; // UUID -> 実ファイルパス
    QMap<QString, QString> m_filePathToUuid; // 実ファイルパス -> UUID
    int m_httpPort;
    mutable QMutex m_mutex;

public:
    explicit FileManager(int httpPort = 28081, QObject* parent = nullptr);
    ~FileManager() = default;

    // 実ファイルパスをアセット管理に登録し、配信用のHTTP URLを返す
    QString registerAsset(const QString& absolutePath);

    // アセットID(またはアセットファイル名)から実ファイルパスを解決
    QString getRealPath(const QString& assetId) const;

    // 登録チェック
    bool hasAsset(const QString& assetId) const;

    // ポート変更時の更新用
    void setHttpPort(int port);
};
