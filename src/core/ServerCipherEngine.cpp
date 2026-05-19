#include "ServerCipherEngine.hpp"
#include "Logger.hpp"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QTimer>

QString ServerCipherEngine::s_apiUrl = "https://streamers-tool.sakura.ne.jp/TransCipher/api.php";
QString ServerCipherEngine::s_apiToken = "";

void ServerCipherEngine::configure(const QString& apiUrl, const QString& apiToken)
{
    s_apiUrl = apiUrl;
    if (!apiToken.isEmpty()) {
        s_apiToken = apiToken;
    }
}

ServerCipherResult ServerCipherEngine::encrypt(const QByteArray& data, const QString& key, int mode)
{
    if (key.isEmpty()) return ServerCipherResult(false, QByteArray(), "Key is empty");

    QJsonObject payload;
    payload["action"] = "encrypt";
    payload["token"] = s_apiToken;
    payload["key"] = key;
    payload["data"] = QString::fromLatin1(data.toBase64());
    payload["mode"] = mode;

    return sendRequest(payload);
}

ServerCipherResult ServerCipherEngine::decrypt(const QByteArray& data, const QString& key)
{
    if (key.isEmpty()) return ServerCipherResult(false, QByteArray(), "Key is empty");

    QJsonObject payload;
    payload["action"] = "decrypt";
    payload["token"] = s_apiToken;
    payload["key"] = key;
    payload["data"] = QString::fromLatin1(data.toBase64());

    return sendRequest(payload);
}

ServerCipherResult ServerCipherEngine::sendRequest(const QJsonObject& payload)
{
    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(s_apiUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QByteArray jsonData = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = manager.post(request, jsonData);

    // QEventLoopを使って同期通信にする（設定読み込み時はブロックしても問題ないため）
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    // タイムアウト設定 (10秒)
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(10000);

    loop.exec();

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        reply->deleteLater();
        return ServerCipherResult(false, QByteArray(), "Request timed out");
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        reply->deleteLater();
        return ServerCipherResult(false, QByteArray(), errorMsg);
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    if (doc.isNull()) {
        return ServerCipherResult(false, QByteArray(), "Failed to parse JSON response");
    }

    QJsonObject obj = doc.object();
    if (obj.value("status").toString() == "success") {
        QString base64Result = obj.value("result").toString();
        return ServerCipherResult(true, QByteArray::fromBase64(base64Result.toLatin1()), "Success");
    }

    QString apiMsg = obj.value("message").toString();
    return ServerCipherResult(false, QByteArray(), apiMsg.isEmpty() ? "API Error" : apiMsg);
}
