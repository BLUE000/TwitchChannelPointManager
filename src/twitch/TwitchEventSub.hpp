#pragma once

#include <QObject>
#include <QWebSocket>
#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QSet>
#include <QQueue>

#include <QNetworkAccessManager>

class TwitchEventSub : public QObject {
    Q_OBJECT
private:
    QWebSocket* m_webSocket;
    QNetworkAccessManager* m_networkManager;
    QString m_sessionId;
    
    QString m_accessToken;
    QString m_clientId;
    QString m_broadcasterId;

    bool m_isConnected;

public:
    explicit TwitchEventSub(QObject* parent = nullptr);
    ~TwitchEventSub();

    void connectToServer(const QString& accessToken, const QString& clientId, const QString& broadcasterId);
    void disconnectFromServer();

    bool isConnected() const { return m_isConnected; }

    void setNetworkAccessManager(QNetworkAccessManager* manager) {
        if (m_networkManager && m_networkManager->parent() == this) {
            m_networkManager->deleteLater();
        }
        m_networkManager = manager;
    }

signals:
    void connected();
    void disconnected();
    void channelPointRedeemed(const QString& rewardId, const QString& username, const QDateTime& timestamp);
    void subscriptionSuccess();
    void subscriptionFailed(const QString& error);
    void tokenExpired();

private slots:
    void onConnected();
    void onTextMessageReceived(const QString& message);
    void onDisconnected();
    void onSslErrors(const QList<QSslError>& errors);
    void onWebSocketError(QAbstractSocket::SocketError error);

private:
    void handleWelcomeMessage(const QJsonObject& payload);
    void handleNotificationMessage(const QJsonObject& payload);
    void registerSubscription();

    QSet<QString> m_processedMessageIds;
    QQueue<QString> m_messageIdQueue;
};
