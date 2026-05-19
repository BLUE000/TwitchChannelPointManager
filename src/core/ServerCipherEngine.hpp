#pragma once

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QNetworkAccessManager>

class ServerCipherResult {
public:
    ServerCipherResult(bool success, const QByteArray& data, const QString& message)
        : m_success(success), m_data(data), m_message(message) {}

    bool isSuccess() const { return m_success; }
    QByteArray data() const { return m_data; }
    QString message() const { return m_message; }

private:
    bool m_success;
    QByteArray m_data;
    QString m_message;
};

class ServerCipherEngine {
public:
    static void configure(const QString& apiUrl, const QString& apiToken);
    
    // PHP版 TransCipher API (api.php) を呼び出して暗号化
    static ServerCipherResult encrypt(const QByteArray& data, const QString& key, int mode = 1);
    
    // PHP版 TransCipher API (api.php) を呼び出して復号
    static ServerCipherResult decrypt(const QByteArray& data, const QString& key);

private:
    static QString s_apiUrl;
    static QString s_apiToken;

    static ServerCipherResult sendRequest(const QJsonObject& payload);
};
