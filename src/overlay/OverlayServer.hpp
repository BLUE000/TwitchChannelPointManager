#pragma once

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QHttpServer>
#include <QString>
#include <QList>
#include <QProcess>
#include "../reward/QueueManager.hpp"

class FileManager;
class Database;

class OverlayServer : public QObject {
    Q_OBJECT
private:
    QWebSocketServer* m_wsServer;
    QHttpServer* m_httpServer;
    FileManager* m_fileManager;
    Database* m_database;
    QList<QWebSocket*> m_clients;

    int m_wsPort;
    int m_httpPort;

    // 外部スクリプトプロセス管理リスト
    QList<QProcess*> m_runningScriptProcesses;

    // スクリプト実行タイムアウト秒数（php.ini の max_execution_time デフォルト値に準拠）
    static constexpr int SCRIPT_TIMEOUT_SEC = 30;

public:
    explicit OverlayServer(FileManager* fileManager, Database* database, QObject* parent = nullptr);
    ~OverlayServer();

    // サーバーの起動
    bool start(int wsPort = 28080, int httpPort = 28081);
    
    // サーバーの停止
    void stop();

signals:
    // OBS側から演出完了を受け取った際に発火
    void effectFinished(const QString& queueId);
    
    // 接続数更新シグナル
    void clientCountChanged(int count);

public slots:
    // QueueManager から演出指示を受けて、接続中のすべてのOBSクライアントへWebSocket送信する
    void sendEffect(const QueueItem& item, const Effect& effect);
    
    // 演出の中止、キュー消去指示をOBSへ一斉配信
    void broadcastStopAll();
    void broadcastClearQueue();

    // 実行中の外部スクリプトプロセスを全て強制終了（パニックボタン連携）
    void killScriptProcesses();

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onClientDisconnected();

private:
    void setupHttpRoutes();
    QString getMimeType(const QString& filepath) const;
    void broadcastMessage(const QString& message);
};

