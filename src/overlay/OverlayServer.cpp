#include "OverlayServer.hpp"
#include "FileManager.hpp"
#include "../core/Logger.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QHttpServerResponse>
#include <QTcpServer>

OverlayServer::OverlayServer(FileManager* fileManager, QObject* parent)
    : QObject(parent)
    , m_wsServer(nullptr)
    , m_httpServer(nullptr)
    , m_fileManager(fileManager)
    , m_wsPort(28080)
    , m_httpPort(28081)
{
}

OverlayServer::~OverlayServer()
{
    stop();
}

bool OverlayServer::start(int wsPort, int httpPort)
{
    stop();

    m_wsPort = wsPort;
    m_httpPort = httpPort;
    m_fileManager->setHttpPort(m_httpPort);

    // 1. WebSocket サーバーの起動
    m_wsServer = new QWebSocketServer("TwitchOverlayWS", QWebSocketServer::NonSecureMode, this);
    connect(m_wsServer, &QWebSocketServer::newConnection, this, &OverlayServer::onNewConnection);

    if (!m_wsServer->listen(QHostAddress::Any, m_wsPort)) {
        LOG_ERROR(QString("Failed to start WebSocket server on port %1").arg(m_wsPort));
        return false;
    }
    LOG_INFO(QString("WebSocket Server listening on port %1").arg(m_wsPort));

    // 2. HTTP アセットサーバーの起動
    m_httpServer = new QHttpServer(this);
    setupHttpRoutes();

    auto* tcpServer = new QTcpServer(this);
    if (!tcpServer->listen(QHostAddress::Any, m_httpPort)) {
        LOG_ERROR(QString("Failed to start HTTP server on port %1").arg(m_httpPort));
        m_wsServer->close();
        m_wsServer->deleteLater();
        m_wsServer = nullptr;
        return false;
    }
    m_httpServer->bind(tcpServer);
    LOG_INFO(QString("HTTP Asset Server listening on port %1").arg(m_httpPort));

    return true;
}

void OverlayServer::stop()
{
    // WebSocket サーバーの停止
    if (m_wsServer) {
        if (m_wsServer->isListening()) {
            m_wsServer->close();
        }
        for (auto* client : m_clients) {
            client->close();
            client->deleteLater();
        }
        m_clients.clear();
        m_wsServer->deleteLater();
        m_wsServer = nullptr;
        LOG_INFO("WebSocket Server stopped.");
    }

    // HTTP サーバーの停止
    if (m_httpServer) {
        m_httpServer->deleteLater();
        m_httpServer = nullptr;
        LOG_INFO("HTTP Asset Server stopped.");
    }
}

void OverlayServer::sendEffect(const QueueItem& item, const Effect& effect)
{
    if (m_clients.isEmpty()) {
        LOG_WARN("No OBS clients connected. Instantly auto-completing effect.");
        emit effectFinished(item.queueId);
        return;
    }

    LOG_INFO(QString("Broadcasting show_effect to OBS. QueueId: %1").arg(item.queueId));

    // 実ファイルパスから配信用 HTTP URL への変換
    QString servedFilePath = m_fileManager->registerAsset(effect.filePath);
    QString servedAudioPath = m_fileManager->registerAsset(effect.audioPath);

    // WebSocket 送信用 JSON の構築
    QJsonObject effectObj = effect.toJson();
    effectObj.insert("filePath", servedFilePath);
    effectObj.insert("audioPath", servedAudioPath);

    QJsonObject dataObj;
    dataObj.insert("queueId", item.queueId);
    dataObj.insert("effect", effectObj);

    QJsonObject rootObj;
    rootObj.insert("type", "show_effect");
    rootObj.insert("data", dataObj);

    QJsonDocument doc(rootObj);
    QString message = doc.toJson(QJsonDocument::Compact);

    // 接続中のクライアントへ送信
    for (auto* client : m_clients) {
        client->sendTextMessage(message);
    }
}

void OverlayServer::broadcastStopAll()
{
    LOG_INFO("Broadcasting stop_all (Emergency Panic) command to OBS.");
    QJsonObject rootObj;
    rootObj.insert("type", "stop_all");
    
    QJsonDocument doc(rootObj);
    QString message = doc.toJson(QJsonDocument::Compact);

    for (auto* client : m_clients) {
        client->sendTextMessage(message);
    }
}

void OverlayServer::broadcastClearQueue()
{
    LOG_INFO("Broadcasting clear_queue command to OBS.");
    QJsonObject rootObj;
    rootObj.insert("type", "clear_queue");
    
    QJsonDocument doc(rootObj);
    QString message = doc.toJson(QJsonDocument::Compact);

    for (auto* client : m_clients) {
        client->sendTextMessage(message);
    }
}

void OverlayServer::onNewConnection()
{
    QWebSocket* socket = m_wsServer->nextPendingConnection();
    if (socket) {
        LOG_INFO("New OBS Overlay client connected via WebSocket.");
        connect(socket, &QWebSocket::textMessageReceived, this, &OverlayServer::onTextMessageReceived);
        connect(socket, &QWebSocket::disconnected, this, &OverlayServer::onClientDisconnected);
        m_clients.append(socket);

        emit clientCountChanged(m_clients.size());
    }
}

void OverlayServer::onTextMessageReceived(const QString& message)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (doc.isNull()) {
        qWarning() << "Failed to parse OBS message:" << parseError.errorString();
        return;
    }

    QJsonObject root = doc.object();
    QString type = root.value("type").toString();
    QJsonObject data = root.value("data").toObject();

    if (type == "effect_completed") {
        QString queueId = data.value("queueId").toString();
        emit effectFinished(queueId);
    }
}

void OverlayServer::onClientDisconnected()
{
    QWebSocket* socket = qobject_cast<QWebSocket*>(sender());
    if (socket) {
        LOG_INFO("OBS Overlay client disconnected.");
        m_clients.removeOne(socket);
        socket->deleteLater();
        
        emit clientCountChanged(m_clients.size());
    }
}

void OverlayServer::setupHttpRoutes()
{
    // アセット配信用ルート
    // NOTE: <arg> を使用。<path> はルートパス全体を取得するQtの挙動があり、
    //       UUIDファイル名がスラッシュを含まないため <arg> で正確にマッチする。
    m_httpServer->route("/assets/<arg>", [this](const QString& rawFilename) {
        // 先頭スラッシュなど余分な文字を除去して正規化
        QString filename = rawFilename;
        while (filename.startsWith('/')) {
            filename = filename.mid(1);
        }

        LOG_INFO("Asset requested: " + filename);
        QString realPath = m_fileManager->getRealPath(filename);
        if (realPath.isEmpty()) {
            LOG_WARN("Asset not found in registry: " + filename);
            return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
        }

        QFile file(realPath);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_ERROR("Failed to open real asset file for serving: " + realPath);
            return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
        }

        QByteArray fileData = file.readAll();
        file.close();

        // レスポンスの構築（CORS ヘッダー & キャッシュ防止ヘッダー付与）
        QHttpServerResponse response(getMimeType(realPath).toUtf8(), fileData);
        QHttpHeaders headers;
        headers.append("Access-Control-Allow-Origin", "*");
        headers.append("Cache-Control", "no-cache, no-store, must-revalidate");
        response.setHeaders(headers);
        return response;
    });

    // テスト兼OBS登録用のデフォルトオーバーレイHTMLページ
    m_httpServer->route("/overlay", [this]() {
        QString html = QString(R"HTML(
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="utf-8">
    <title>OBS Twitch Overlay</title>
    <style>
        body { margin: 0; background-color: transparent; overflow: hidden; }
        #overlay-container { width: 100vw; height: 100vh; position: relative; }
        .overlay-item { position: absolute; text-align: center; }
        .overlay-text { font-weight: bold; margin-top: 10px; text-shadow: 2px 2px 4px #000; }
    </style>
</head>
<body>
    <div id="overlay-container"></div>
    <script>
        const ws = new WebSocket("ws://localhost:%1/overlay");

        ws.onopen = () => {
            console.log("Connected to Twitch Overlay WebSocket Server");
        };

        ws.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            
            if (msg.type === "show_effect") {
                const data = msg.data;
                const container = document.getElementById("overlay-container");
                
                // コンテナを初期化
                container.innerHTML = "";

                // ラッパーエレメント
                const wrapper = document.createElement("div");
                wrapper.className = "overlay-item";
                
                // 位置の決定（プリセットまたはカスタム座標）
                const preset = (data.effect.position && data.effect.position.preset)
                    ? data.effect.position.preset : "center";

                // 全リセット
                wrapper.style.left = "";
                wrapper.style.top = "";
                wrapper.style.right = "";
                wrapper.style.bottom = "";
                wrapper.style.transform = "";

                if (preset === "center") {
                    wrapper.style.left = "50%";
                    wrapper.style.top  = "50%";
                    wrapper.style.transform = "translate(-50%, -50%)";
                } else if (preset === "top_left") {
                    wrapper.style.left = "5%";
                    wrapper.style.top  = "5%";
                } else if (preset === "top_right") {
                    wrapper.style.right = "5%";
                    wrapper.style.top   = "5%";
                } else if (preset === "bottom_left") {
                    wrapper.style.left   = "5%";
                    wrapper.style.bottom = "5%";
                } else if (preset === "bottom_right") {
                    wrapper.style.right  = "5%";
                    wrapper.style.bottom = "5%";
                } else {
                    // custom: offsetX/offsetY はオーバーレイ左上からのピクセル座標
                    wrapper.style.left = (data.effect.position.offsetX || 0) + "px";
                    wrapper.style.top  = (data.effect.position.offsetY || 0) + "px";
                    wrapper.style.transform = "translate(-50%, -50%)";
                }

                // 演出種類別（画像/動画）
                if (data.effect.type === "image" && data.effect.filePath) {
                    const img = document.createElement("img");
                    img.src = data.effect.filePath;
                    img.style.maxWidth = "100%";
                    img.style.maxHeight = "80vh";
                    wrapper.appendChild(img);
                } else if (data.effect.type === "video" && data.effect.filePath) {
                    const vid = document.createElement("video");
                    vid.src = data.effect.filePath;
                    vid.autoplay = true;
                    vid.style.maxWidth = "100%";
                    vid.style.maxHeight = "80vh";
                    // ビデオ終了で完了（duration はフォールバック上限）
                    vid.onended = () => completeEffect();
                    vid.onerror = () => completeEffect();
                    wrapper.appendChild(vid);
                }

                // テスト用のテキスト描画
                if (data.effect.text) {
                    const txt = document.createElement("div");
                    txt.className = "overlay-text";
                    txt.innerText = data.effect.text;
                    txt.style.fontFamily = data.effect.textStyle.font || 'Arial';
                    txt.style.fontSize = (data.effect.textStyle.size || 32) + 'px';
                    txt.style.color = data.effect.textStyle.color || '#FFFFFF';
                    txt.style.webkitTextStroke = `${data.effect.textStyle.borderWidth || 2}px ${data.effect.textStyle.borderColor || '#000000'}`;
                    wrapper.appendChild(txt);
                }

                container.appendChild(wrapper);

                // 演出完了ハンドラ（二重呼び出し防止付き）
                let effectDone = false;
                let currentAudio = null;
                const completeEffect = () => {
                    if (effectDone) return;
                    effectDone = true;
                    if (currentAudio) {
                        currentAudio.pause();
                        currentAudio = null;
                    }
                    wrapper.remove();
                    ws.send(JSON.stringify({
                        type: "effect_completed",
                        data: { queueId: data.queueId }
                    }));
                };

                // 音声再生
                // sound タイプ: filePath が音声ファイル本体（onended で完了を検知）
                // image/video タイプ: audioPath が補助効果音（duration で管理）
                const audioSrc = (data.effect.type === "sound")
                    ? data.effect.filePath
                    : data.effect.audioPath;

                if (audioSrc) {
                    currentAudio = new Audio(audioSrc);
                    currentAudio.volume = (data.effect.volume || 80) / 100;
                    if (data.effect.type === "sound") {
                        // sound タイプ: 音声終了で完了（duration は上限タイムアウト）
                        currentAudio.onended = completeEffect;
                        currentAudio.onerror = completeEffect;
                    }
                    currentAudio.play().catch(e => {
                        console.log("Audio play blocked:", e);
                        if (data.effect.type === "sound") completeEffect();
                    });
                }

                // タイムアウト: image は duration 厳守、video/sound は上限フォールバック
                setTimeout(completeEffect, (data.effect.duration || 5) * 1000);
            } else if (msg.type === "stop_all") {
                document.getElementById("overlay-container").innerHTML = "";
            }
        };
    </script>
</body>
</html>
        )HTML").arg(m_wsPort);

        QHttpServerResponse response("text/html; charset=utf-8", html.toUtf8());
        QHttpHeaders headers;
        headers.append("Access-Control-Allow-Origin", "*");
        response.setHeaders(headers);
        return response;
    });
}

QString OverlayServer::getMimeType(const QString& filepath) const
{
    QString ext = QFileInfo(filepath).suffix().toLower();
    if (ext == "png")  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif")  return "image/gif";
    if (ext == "mp3")  return "audio/mpeg";
    if (ext == "wav")  return "audio/wav";
    if (ext == "webm") return "video/webm";
    if (ext == "mp4")  return "video/mp4";
    return "application/octet-stream";
}
