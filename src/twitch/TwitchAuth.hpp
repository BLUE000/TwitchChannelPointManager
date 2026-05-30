#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QJsonArray>

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
    void setCredentials(const QString& clientId, const QString& clientSecret) {
        m_clientId = clientId;
        m_clientSecret = clientSecret;
    }

    // 認可フローを開始（外部デフォルトブラウザでTwitchの認証ページを開く）
    void startAuthFlow();
    void stopLoopbackServer();

    // トークン取得・ゲッター
    QString accessToken() const { return m_accessToken; }
    QString refreshToken() const { return m_refreshToken; }

    // チャンネルポイント一覧の動的取得
    void fetchCustomRewards(const QString& accessToken, const QString& clientId, const QString& broadcasterId);

    // リフレッシュトークンを用いたアクセストークンの再取得（リアクティブ更新）
    void refreshAccessToken(const QString& refreshToken, const QString& broadcasterId);

signals:
    void authSuccess(const QString& accessToken, const QString& refreshToken, const QString& broadcasterId);
    void authFailed(const QString& errorMessage);
    void authFailedWithError(const QString& errorMessage, bool isFatal);
    void customRewardsFetched(const QJsonArray& rewards);
    void customRewardsFetchFailed(const QString& errorMessage);

private slots:
    void handleIncomingConnection();
    void readClientData();
    void exchangeCodeForToken(const QString& authCode);

private:
    void sendHtmlResponse(QTcpSocket* socket, bool success);
};
