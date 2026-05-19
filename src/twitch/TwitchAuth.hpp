#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QUrlQuery>
#include <QNetworkAccessManager>

class TwitchAuth : public QObject {
    Q_OBJECT
private:
    QTcpServer* m_loopbackServer;
    int m_callbackPort;
    QString m_accessToken;
    QString m_refreshToken;

    // Twitch Developer Console で設定した情報
    QString m_clientId;
    QString m_clientSecret;

    QNetworkAccessManager* m_networkManager;

public:
    explicit TwitchAuth(const QString& clientId, const QString& clientSecret, int port = 28082, QObject* parent = nullptr);
    ~TwitchAuth();

    void setNetworkAccessManager(QNetworkAccessManager* manager) { m_networkManager = manager; }

    // 認可フローを開始（外部デフォルトブラウザでTwitchの認証ページを開く）
    void startAuthFlow();
    void stopLoopbackServer();

    // トークン取得・ゲッター
    QString accessToken() const { return m_accessToken; }
    QString refreshToken() const { return m_refreshToken; }

signals:
    void authSuccess(const QString& accessToken, const QString& refreshToken);
    void authFailed(const QString& errorMessage);

private slots:
    void handleIncomingConnection();
    void readClientData();

private:
    void sendHtmlResponse(QTcpSocket* socket, bool success);
    void exchangeCodeForToken(const QString& authCode);
};
