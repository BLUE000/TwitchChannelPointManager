#include "TwitchEventSub.hpp"
#include "../core/Logger.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>

TwitchEventSub::TwitchEventSub(QObject* parent)
    : QObject(parent)
    , m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_isConnected(false)
{
    connect(m_webSocket, &QWebSocket::connected, this, &TwitchEventSub::onConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &TwitchEventSub::onDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &TwitchEventSub::onTextMessageReceived);
    connect(m_webSocket, &QWebSocket::sslErrors, this, &TwitchEventSub::onSslErrors);
    
    // エラーハンドリング
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &TwitchEventSub::onWebSocketError);
}

TwitchEventSub::~TwitchEventSub()
{
    disconnectFromServer();
}

void TwitchEventSub::connectToServer(const QString& accessToken, const QString& clientId, const QString& broadcasterId)
{
    if (m_webSocket && (m_webSocket->state() == QAbstractSocket::ConnectedState || 
                        m_webSocket->state() == QAbstractSocket::ConnectingState)) {
        LOG_WARN("Twitch EventSub WebSocket is already connected or connecting. Ignoring connection request.");
        return;
    }

    m_accessToken = accessToken;
    m_clientId = clientId;
    m_broadcasterId = broadcasterId;

    LOG_INFO("Connecting to Twitch EventSub WebSocket server...");
    m_webSocket->open(QUrl("wss://eventsub.wss.twitch.tv/ws"));
}

void TwitchEventSub::disconnectFromServer()
{
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        LOG_INFO("Disconnecting from Twitch EventSub server.");
        m_webSocket->close();
    }
    m_isConnected = false;
    m_processedMessageIds.clear();
    m_messageIdQueue.clear();
}

void TwitchEventSub::onConnected()
{
    LOG_INFO("Twitch EventSub WebSocket connection established.");
    m_isConnected = true;
    emit connected();
}

void TwitchEventSub::onDisconnected()
{
    LOG_WARN("Twitch EventSub WebSocket connection closed.");
    m_isConnected = false;
    m_sessionId.clear();
    emit disconnected();
}

void TwitchEventSub::onSslErrors(const QList<QSslError>& errors)
{
    for (const auto& err : errors) {
        LOG_ERROR("SSL Error in EventSub WebSocket connection: " + err.errorString());
    }
    // SSLエラーを無視して接続を維持したい場合は m_webSocket->ignoreSslErrors() を実行
    m_webSocket->ignoreSslErrors();
}

void TwitchEventSub::onWebSocketError(QAbstractSocket::SocketError error)
{
    LOG_ERROR(QString("WebSocket Error (Code %1): %2").arg(static_cast<int>(error)).arg(m_webSocket->errorString()));
}

void TwitchEventSub::onTextMessageReceived(const QString& message)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (doc.isNull()) {
        LOG_ERROR("Failed to parse EventSub raw message as JSON: " + parseError.errorString());
        return;
    }

    QJsonObject root = doc.object();
    QJsonObject metadata = root.value("metadata").toObject();

    // 重複メッセージの排除 (Deduplication)
    QString messageId = metadata.value("message_id").toString();
    if (!messageId.isEmpty()) {
        if (m_processedMessageIds.contains(messageId)) {
            LOG_WARN("Twitch EventSub: Ignored duplicate message with ID: " + messageId);
            return;
        }
        m_processedMessageIds.insert(messageId);
        m_messageIdQueue.enqueue(messageId);
        if (m_messageIdQueue.size() > 200) {
            QString oldest = m_messageIdQueue.dequeue();
            m_processedMessageIds.remove(oldest);
        }
    }

    QString msgType = metadata.value("message_type").toString();
    QJsonObject payload = root.value("payload").toObject();

    if (msgType == "session_welcome") {
        handleWelcomeMessage(payload);
    } else if (msgType == "notification") {
        handleNotificationMessage(payload);
    } else if (msgType == "session_keepalive") {
        // キープアライブメッセージは正常な動作の一部なので無視またはログ記録
        LOG_DEBUG("Twitch EventSub Keepalive received.");
    } else if (msgType == "session_reconnect") {
        LOG_WARN("Twitch requested reconnect to a new URL.");
        QString reconnectUrl = payload.value("session").toObject().value("reconnect_url").toString();
        if (!reconnectUrl.isEmpty()) {
            disconnectFromServer();
            m_webSocket->open(QUrl(reconnectUrl));
        }
    } else {
        LOG_WARN("Unhandled EventSub message type: " + msgType);
    }
}

void TwitchEventSub::handleWelcomeMessage(const QJsonObject& payload)
{
    QJsonObject session = payload.value("session").toObject();
    m_sessionId = session.value("id").toString();
    LOG_INFO("Twitch Welcome message received. Session ID: " + m_sessionId);

    // セッションIDを取得できたので、EventSub API を叩いてチャンネルポイント購読を登録
    registerSubscription();
}

void TwitchEventSub::handleNotificationMessage(const QJsonObject& payload)
{
    QJsonObject subscription = payload.value("subscription").toObject();
    QString subType = subscription.value("type").toString();

    // チャンネルポイント消費イベントか確認
    if (subType == "channel.channel_points_custom_reward_redemption.add") {
        QJsonObject event = payload.value("event").toObject();
        QString rewardId = event.value("reward").toObject().value("id").toString();
        QString username = event.value("user_name").toString(); // 表示用。 user_login も利用可能。
        
        QString redeemedAtStr = event.value("redeemed_at").toString();
        QDateTime timestamp = QDateTime::fromString(redeemedAtStr, Qt::ISODateWithMs);
        if (!timestamp.isValid()) {
            timestamp = QDateTime::currentDateTime();
        }

        LOG_INFO(QString("Channel Point Redeemed: User '%1' used reward '%2'").arg(username).arg(rewardId));
        emit channelPointRedeemed(rewardId, username, timestamp);
    }
}

void TwitchEventSub::registerSubscription()
{
    if (m_sessionId.isEmpty()) {
        LOG_ERROR("Cannot register EventSub subscription: empty Session ID.");
        return;
    }

    LOG_INFO("Registering channel points redemption subscription with Twitch API...");

    QUrl url("https://api.twitch.tv/helix/eventsub/subscriptions");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());

    // 購読パラメーターの設定
    QJsonObject body;
    body.insert("type", "channel.channel_points_custom_reward_redemption.add");
    body.insert("version", "1");

    QJsonObject condition;
    condition.insert("broadcaster_user_id", m_broadcasterId);
    body.insert("condition", condition);

    QJsonObject transport;
    transport.insert("method", "websocket");
    transport.insert("session_id", m_sessionId);
    body.insert("transport", transport);

    QJsonDocument doc(body);
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 401) {
            LOG_WARN("Twitch EventSub API returned 401 Unauthorized. Access token might be expired. Triggering tokenExpired signal.");
            disconnectFromServer();
            emit tokenExpired();
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            QByteArray errorData = reply->readAll();
            LOG_ERROR("Failed to register EventSub subscription API: " + reply->errorString() + " - " + errorData);
            emit subscriptionFailed(reply->errorString());
            return;
        }

        LOG_INFO("Successfully registered EventSub subscription! Now listening for Twitch Channel Point events.");
        emit subscriptionSuccess();
    });
}
