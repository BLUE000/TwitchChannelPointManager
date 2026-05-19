#include "TwitchAuth.hpp"
#include "../core/Logger.hpp"
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

TwitchAuth::TwitchAuth(const QString& clientId, const QString& clientSecret, int port, QObject* parent)
    : QObject(parent)
    , m_loopbackServer(nullptr)
    , m_callbackPort(port)
    , m_clientId(clientId)
    , m_clientSecret(clientSecret)
    , m_networkManager(nullptr)
{
}

TwitchAuth::~TwitchAuth()
{
    stopLoopbackServer();
}

void TwitchAuth::startAuthFlow()
{
    stopLoopbackServer();

    m_loopbackServer = new QTcpServer(this);
    connect(m_loopbackServer, &QTcpServer::newConnection, this, &TwitchAuth::handleIncomingConnection);

    if (!m_loopbackServer->listen(QHostAddress::Any, m_callbackPort)) {
        LOG_ERROR(QString("Failed to start loopback server on port %1").arg(m_callbackPort));
        emit authFailed("ローカルサーバーの起動に失敗しました。");
        return;
    }

    LOG_INFO(QString("OAuth loopback server listening on port %1").arg(m_callbackPort));

    // Twitch認可URLの生成
    QString authUrl = QString(
        "https://id.twitch.tv/oauth2/authorize"
        "?client_id=%1"
        "&redirect_uri=http://localhost:%2/callback"
        "&response_type=code"
        "&scope=channel:read:redemptions"
    ).arg(m_clientId).arg(m_callbackPort);

    LOG_INFO("Opening Twitch authorization page in default browser.");
    QDesktopServices::openUrl(QUrl(authUrl));
}

void TwitchAuth::stopLoopbackServer()
{
    if (m_loopbackServer) {
        if (m_loopbackServer->isListening()) {
            m_loopbackServer->close();
        }
        m_loopbackServer->deleteLater();
        m_loopbackServer = nullptr;
        LOG_INFO("OAuth loopback server stopped.");
    }
}

void TwitchAuth::handleIncomingConnection()
{
    QTcpSocket* socket = m_loopbackServer->nextPendingConnection();
    if (socket) {
        connect(socket, &QTcpSocket::readyRead, this, &TwitchAuth::readClientData);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void TwitchAuth::readClientData()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray requestData = socket->readAll();
    QString requestStr = QString::fromUtf8(requestData);

    // GET リクエストから認可コードを抽出
    if (requestStr.startsWith("GET")) {
        QStringList lines = requestStr.split('\n');
        if (!lines.isEmpty()) {
            QString firstLine = lines.first();
            QStringList parts = firstLine.split(' ');
            if (parts.size() >= 2) {
                QString path = parts.at(1);
                QUrl url("http://localhost" + path);
                QUrlQuery query(url.query());

                if (query.hasQueryItem("code")) {
                    QString code = query.queryItemValue("code");
                    LOG_INFO("Authorization code successfully received from browser redirect.");
                    sendHtmlResponse(socket, true);
                    socket->disconnectFromHost();
                    stopLoopbackServer(); // 認証コードが取得できたのでサーバーを停止

                    // トークン取得処理へ
                    exchangeCodeForToken(code);
                    return;
                }
            }
        }
        
        LOG_WARN("Redirect received, but authorization code was missing or malformed.");
        sendHtmlResponse(socket, false);
        socket->disconnectFromHost();
        emit authFailed("認証コードの取得に失敗しました。");
    }
}

void TwitchAuth::sendHtmlResponse(QTcpSocket* socket, bool success)
{
    QTextStream os(socket);
    os.setAutoDetectUnicode(true);

    QString title = success ? "認証成功 - Twitch Manager" : "認証失敗";
    QString color = success ? "#4CAF50" : "#F44336";
    QString statusText = success ? "認証に成功しました！" : "認証に失敗しました。";
    QString subText = success 
        ? "アプリケーションに戻り、設定を確認してください。このウィンドウは閉じて構いません。" 
        : "もう一度最初から認証をやり直してください。";

    QString html = QString(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='utf-8'>"
        "<title>%1</title>"
        "<style>"
        "  body { font-family: 'Segoe UI', Arial, sans-serif; background-color: #121214; color: #E1E1E6; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }"
        "  .card { background-color: #1D1D22; padding: 40px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.5); text-align: center; max-width: 450px; border-top: 6px solid %2; }"
        "  h1 { color: #FFFFFF; font-size: 24px; margin-bottom: 16px; }"
        "  p { font-size: 15px; color: #A9A9B2; line-height: 1.6; margin-bottom: 24px; }"
        "  .badge { display: inline-block; padding: 8px 16px; border-radius: 20px; font-weight: bold; background-color: %2; color: #FFFFFF; margin-bottom: 20px; }"
        "</style>"
        "</head>"
        "<body>"
        "  <div class='card'>"
        "    <div class='badge'>Twitch Overlay Server</div>"
        "    <h1>%3</h1>"
        "    <p>%4</p>"
        "  </div>"
        "</body>"
        "</html>"
    ).arg(title).arg(color).arg(statusText).arg(subText);

    os << html;
}

void TwitchAuth::exchangeCodeForToken(const QString& authCode)
{
    LOG_INFO("Exchanging authorization code for OAuth tokens.");

    QNetworkAccessManager* manager = m_networkManager ? m_networkManager : new QNetworkAccessManager(this);
    QUrl tokenUrl("https://id.twitch.tv/oauth2/token");
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("client_id", m_clientId);
    postData.addQueryItem("client_secret", m_clientSecret);
    postData.addQueryItem("code", authCode);
    postData.addQueryItem("grant_type", "authorization_code");
    postData.addQueryItem("redirect_uri", QString("http://localhost:%1/callback").arg(m_callbackPort));

    QNetworkReply* reply = manager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

        connect(reply, &QNetworkReply::finished, [this, reply, manager]() {
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            if (manager != m_networkManager) {
                manager->deleteLater();
            }
            LOG_ERROR("HTTP error exchanging authorization code for token: " + reply->errorString());
            emit authFailed("トークンの交換に失敗しました: " + reply->errorString());
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

        if (doc.isNull()) {
            reply->deleteLater();
            if (manager != m_networkManager) {
                manager->deleteLater();
            }
            LOG_ERROR("JSON parsing failed for token exchange response: " + parseError.errorString());
            emit authFailed("レスポンスのパースに失敗しました。");
            return;
        }

        QJsonObject obj = doc.object();
        m_accessToken = obj.value("access_token").toString();
        m_refreshToken = obj.value("refresh_token").toString();

        if (m_accessToken.isEmpty()) {
            reply->deleteLater();
            if (manager != m_networkManager) {
                manager->deleteLater();
            }
            LOG_ERROR("Access token is empty in response.");
            emit authFailed("アクセストークンが空でした。");
            return;
        }

        LOG_INFO("OAuth tokens successfully exchanged. Fetching Twitch user profile...");

        // TwitchのHelix API（/helix/users）を呼び出してユーザーIDを自動取得
        QNetworkRequest userRequest(QUrl("https://api.twitch.tv/helix/users"));
        userRequest.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
        userRequest.setRawHeader("Client-Id", m_clientId.toUtf8());

        // 同じmanagerを使って非同期GETを送信
        QNetworkReply* userReply = manager->get(userRequest);
        connect(userReply, &QNetworkReply::finished, [this, userReply, reply, manager]() {
            userReply->deleteLater();
            reply->deleteLater();
            if (manager != m_networkManager) {
                manager->deleteLater();
            }

            if (userReply->error() != QNetworkReply::NoError) {
                LOG_ERROR("HTTP error fetching user info: " + userReply->errorString());
                emit authFailed("ユーザー情報の自動取得に失敗しました: " + userReply->errorString());
                return;
            }

            QByteArray userData = userReply->readAll();
            QJsonParseError userParseError;
            QJsonDocument userDoc = QJsonDocument::fromJson(userData, &userParseError);
            if (userDoc.isNull()) {
                LOG_ERROR("JSON parsing failed for user info response: " + userParseError.errorString());
                emit authFailed("ユーザー情報のパースに失敗しました。");
                return;
            }

            QJsonObject userObj = userDoc.object();
            QJsonArray dataArray = userObj.value("data").toArray();
            if (dataArray.isEmpty()) {
                LOG_ERROR("User data array is empty in response.");
                emit authFailed("ユーザーデータが見つかりませんでした。");
                return;
            }

            QJsonObject profile = dataArray.at(0).toObject();
            QString broadcasterId = profile.value("id").toString();
            QString displayName = profile.value("display_name").toString();

            LOG_INFO("Successfully authenticated as: " + displayName + " (ID: " + broadcasterId + ")");
            emit authSuccess(m_accessToken, m_refreshToken, broadcasterId);
        });
    });
}

void TwitchAuth::fetchCustomRewards(const QString& accessToken, const QString& clientId, const QString& broadcasterId)
{
    LOG_INFO("Fetching custom channel point rewards from Twitch Helix API...");

    QNetworkAccessManager* manager = m_networkManager ? m_networkManager : new QNetworkAccessManager(this);
    QUrl url(QString("https://api.twitch.tv/helix/channel_points/custom_rewards?broadcaster_id=%1&only_manageable_by_lt=false").arg(broadcasterId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
    request.setRawHeader("Client-Id", clientId.toUtf8());

    QNetworkReply* reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply, manager]() {
        reply->deleteLater();
        if (manager != m_networkManager) {
            manager->deleteLater();
        }

        if (reply->error() != QNetworkReply::NoError) {
            LOG_ERROR("HTTP error fetching custom rewards: " + reply->errorString());
            emit customRewardsFetchFailed("報酬の取得に失敗しました: " + reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (doc.isNull()) {
            LOG_ERROR("JSON parsing failed for custom rewards response: " + parseError.errorString());
            emit customRewardsFetchFailed("報酬データのパースに失敗しました。");
            return;
        }

        QJsonObject obj = doc.object();
        QJsonArray rewards = obj.value("data").toArray();
        LOG_INFO(QString("Successfully fetched %1 custom rewards from Twitch.").arg(rewards.size()));
        emit customRewardsFetched(rewards);
    });
}
