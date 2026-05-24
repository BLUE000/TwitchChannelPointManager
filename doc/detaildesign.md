# 詳細設計書

## 1. クラス設計 (Class Specifications)

### 1.1 Core モジュール

#### `core/Application`
アプリケーション全体の起動、ライフサイクル、主要マネージャーの初期化・終了処理を行います。

```cpp
#pragma once
#include <QApplication>
#include <memory>

class MainWindow;
class Database;
class Config;
class RewardManager;
class QueueManager;
class OverlayServer;

class Application : public QApplication {
    Q_OBJECT
private:
    std::unique_ptr<Database> m_database;
    std::unique_ptr<Config> m_config;
    std::unique_ptr<RewardManager> m_rewardManager;
    std::unique_ptr<QueueManager> m_queueManager;
    std::unique_ptr<OverlayServer> m_overlayServer;
    std::unique_ptr<MainWindow> m_mainWindow;

public:
    Application(int& argc, char** argv);
    ~Application();

    bool initialize();
    void shutdown();
};
```

#### `core/Config`
設定データ（JSON形式またはSQLite）の読み込み・保存を行います。

```cpp
#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>

class Config : public QObject {
    Q_OBJECT
private:
    QString m_configPath;
    QMap<QString, QVariant> m_settings;

public:
    explicit Config(const QString& path, QObject* parent = nullptr);
    
    bool load();
    bool save();
    
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void set(const QString& key, const QVariant& value);
};
```

#### `core/HTMLSanitizer`
安全性の高いサンドボックスHTML/CSS配信のため、アセットファイルおよびランキングHTMLを配信直前にクレンジングする静的ユーティリティクラス。

```cpp
#pragma once
#include <QString>
#include <QSet>

class HTMLSanitizer {
private:
    static const QSet<QString> s_allowedTags;
    static const QSet<QString> s_allowedAttributes;

public:
    HTMLSanitizer() = delete; // 静的ユーティリティクラス

    static QString sanitizeHtml(const QString& rawHtml);

private:
    static QString sanitizeTagAttributes(const QString& tagName, const QString& rawAttrs);
    static QString sanitizeCss(const QString& rawCss);
    static bool isValidLocalImageSource(const QString& src);
};
```


---

### 1.2 Twitch 連携モジュール

#### `twitch/TwitchAuth`
OAuth2認可フローを制御します。ローカルループバックサーバーを起動し、リダイレクトから認可コードを受信します。

```cpp
#pragma once
#include <QObject>
#include <QTcpServer>
#include <QString>

class TwitchAuth : public QObject {
    Q_OBJECT
private:
    QTcpServer* m_loopbackServer;
    int m_callbackPort;
    QString m_accessToken;
    QString m_refreshToken;

public:
    explicit TwitchAuth(int port = 28082, QObject* parent = nullptr);
    
    void startAuthFlow(); // 外部ブラウザを起動
    void stopLoopbackServer();

signals:
    void authSuccess(const QString& accessToken, const QString& refreshToken);
    void authFailed(const QString& errorMessage);

private slots:
    void handleIncomingConnection();
};
```

#### `twitch/TwitchEventSub`
Twitch EventSub WebSocketサーバーとの接続、サブスクリプション登録、イベントの受信・解析を行います。

```cpp
#pragma once
#include <QObject>
#include <QWebSocket>
#include <QString>
#include <QJsonObject>
#include "../../reward/Reward.hpp"

class TwitchEventSub : public QObject {
    Q_OBJECT
private:
    QWebSocket* m_webSocket;
    QString m_sessionId;
    QString m_accessToken;
    QString m_clientId;

public:
    explicit TwitchEventSub(QObject* parent = nullptr);
    
    void connectToServer(const QString& accessToken, const QString& clientId);
    void disconnectFromServer();

signals:
    void connected();
    void disconnected();
    void channelPointRedeemed(const QString& rewardId, const QString& username, const QDateTime& timestamp);

private slots:
    void onConnected();
    void onTextMessageReceived(const QString& message);
    void onDisconnected();
    
private:
    void registerSubscription(); // WebSocketセッション確立後にEventSubに購読要求
    void parseNotification(const QJsonObject& json);
};
```

---

### 1.3 報酬・演出管理モジュール

#### `reward/QueueManager`
演出要求を順番にキュー処理します。OBSブラウザソースからの完了シグナルを待機して次へと進みます。

```cpp
#pragma once
#include <QObject>
#include <QQueue>
#include <QDateTime>
#include "Reward.hpp"

struct QueueItem {
    QString queueId;
    QString rewardId;
    QString username;
    QDateTime timestamp;
    QList<Effect> effects;
    int currentEffectIndex;
};

class QueueManager : public QObject {
    Q_OBJECT
private:
    QQueue<QueueItem> m_queue;
    bool m_isPlaying;

public:
    explicit QueueManager(QObject* parent = nullptr);
    
    void enqueueRedemption(const QString& rewardId, const QString& username, const QDateTime& timestamp);
    void clearQueue();
    void stopAllEffects(); // パニックボタン用

signals:
    void queueUpdated(int pendingCount);
    void playEffectRequested(const QueueItem& item, const Effect& effect);

public slots:
    void onEffectCompleted(const QString& queueId);

private:
    void processNext();
};
```

---

### 1.4 Overlay（WebSocket/HTTP）モジュール

#### `overlay/OverlayServer`
OBSブラウザソースと双方向の通信を行うWebSocketサーバーと、ローカルアセット配信用のHTTPサーバーを起動します。

```cpp
#pragma once
#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QHttpServer>
#include <QString>
#include <QList>
#include "../reward/QueueManager.hpp"

class FileManager;
class Database;

class OverlayServer : public QObject {
    Q_OBJECT
private:
    QWebSocketServer* m_wsServer;    // WebSocket サーバー
    QHttpServer* m_httpServer;       // HTTP アセットサーバー
    FileManager* m_fileManager;      // ファイル管理
    Database* m_database;            // データベース
    QList<QWebSocket*> m_clients;
    int m_wsPort;
    int m_httpPort;

public:
    explicit OverlayServer(FileManager* fileManager, Database* database, QObject* parent = nullptr);
    ~OverlayServer();

    bool start(int wsPort, int httpPort);
    void stop();
    void sendEffect(const QueueItem& item, const Effect& effect);

signals:
    void effectFinished(const QString& queueId);
    void clientCountChanged(int count);

public slots:
    void broadcastStopAll();
    void broadcastClearQueue();

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onClientDisconnected();
    
private:
    void setupHttpRoutes();
    QString getMimeType(const QString& filepath) const;
    void broadcastMessage(const QString& message);
};
```


---

### 1.5 Database モジュール

#### `database/Database`
SQLite接続を管理し、データ永続化と履歴の書き込みを行います。

```cpp
#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>
#include "../reward/Reward.hpp"

class Database : public QObject {
    Q_OBJECT
private:
    QSqlDatabase m_db;

public:
    explicit Database(QObject* parent = nullptr);
    ~Database();

    bool open(const QString& dbPath);
    void close();

    // 報酬データ操作
    bool saveReward(const Reward& reward);
    bool loadRewards(QList<Reward>& rewards);
    bool deleteReward(const QString& rewardId);

    // ログ記録
    bool logUsage(const QString& rewardId, const QString& username, const QDateTime& timestamp);
    int getTodayUsageCount();
};
```

---

## 2. 処理詳細フロー (Detailed Sequences)

### 2.1 チャンネルポイント演出のライフサイクル

```text
+---------+          +-----------------+          +--------------+          +---------------+          +---------------+          +------------+
| Twitch  |          | TwitchEventSub  |          | QueueManager |          | OverlayServer |          |  FileManager  |          | OBS Overlay|
+----+----+          +--------+--------+          +------+-------+          +-------+-------+          +-------+-------+          +-----+------+
     |                        |                          |                          |                          |                        |
     | [EventSub (WSS)]       |                          |                          |                          |                        |
     |----(通知受信)--------->|                          |                          |                          |                        |
     |                        |-- (通知解析)             |                          |                          |                        |
     |                        |                          |                          |                          |                        |
     |                        |-- channelPointRedeemed ->|                          |                          |                        |
     |                        |   (rewardId, user)       |                          |                          |                        |
     |                        |                          |-- (キュー追加)           |                          |                        |
     |                        |                          |                          |                          |                        |
     |                        |                          |-- playNext() ----------- |                          |                        |
     |                        |                          |   (QueueItem, Effect)    |                          |                        |
     |                        |                          |                          |-- convertToHttpUrl() --->|                        |
     |                        |                          |                          |                           |-- (UUID生成&マップ)   |
     |                        |                          |                          |<-- (HTTP URL返却) --------|                        |
     |                        |                          |                          |                          |                        |
     |                        |                          |                          |-- WebSocket (show_effect) ----------------------->|
     |                        |                          |                          |   [URL化されたアセット]                             |-- (演出再生)
     |                        |                          |                          |                          |                        |-- (ローカルHTTP)
     |                        |                          |                          |                          |<====== GET アセット====| (abc123.html)
     |                        |                          |                          |                          |   [HTMLサニタイズ処理] |
     |                        |                          |                          |                          |====== データを返却 ===>|
     |                        |                          |                          |                          |   (安全なクレンジング済) |
     |                        |                          |                          |                          |                        |
     |                        |                          |                          |<-- WebSocket (completed) -------------------------|
     |                        |                          |                          |   (queueId)              |                        |
     |                        |                          |<-- effectFinished -------|                          |                        |
     |                        |                          |   (queueId)              |                          |                        |
     |                        |                          |                          |                          |                        |
     |                        |                          |-- (完了・ログ記録)       |                          |                        |
     |                        |                          |                          |                          |                        |
     |                        |                          |-- playNext() ----------- |                          |                        |
     |                        |                          |   (次のキュー)           |                          |                        |
```

---

## 3. シグナル・スロット設計 (Qt Signals & Slots)

```text
[TwitchEventSub] 
   └── channelPointRedeemed(QString, QString, QDateTime)
         └──  [QueueManager]::enqueueRedemption(QString, QString, QDateTime) ──【キュー挿入】

[QueueManager]
   └── playEffectRequested(QueueItem, Effect)
         └──  [OverlayServer]::sendEffect(QueueItem, Effect) ────────────────【OBSへ送信】

[OverlayServer] (WebSocket受信イベント)
   └── onTextMessageReceived("effect_completed")
         └──  emit effectFinished(QString)
               └──  [QueueManager]::onEffectCompleted(QString) ───────【次のキューへ】
```

---

## 4. データベース実装詳細

### 4.1 テーブル定義 SQL

#### `rewards` テーブル
```sql
CREATE TABLE IF NOT EXISTS rewards (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    cost INTEGER NOT NULL,
    cooldown INTEGER NOT NULL,
    allowed_roles TEXT NOT NULL, -- JSON配列文字列 e.g., ["everyone"]
    enabled BOOLEAN NOT NULL DEFAULT 1,
    mode TEXT NOT NULL DEFAULT 'sequential', -- 'sequential' / 'random'
    effects TEXT NOT NULL -- JSON配列文字列 (Effect構造体のシリアライズ)
);
```

#### `usage_logs` テーブル
```sql
CREATE TABLE IF NOT EXISTS usage_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    reward_id TEXT NOT NULL,
    username TEXT NOT NULL,
    timestamp TEXT NOT NULL, -- ISO-8601 形式 'YYYY-MM-DD HH:MM:SS'
    FOREIGN KEY(reward_id) REFERENCES rewards(id) ON DELETE CASCADE
);
```

#### `settings` テーブル（新規追加）
```sql
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
```
