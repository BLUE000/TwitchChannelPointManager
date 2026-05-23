#include "OverlayServer.hpp"
#include "FileManager.hpp"
#include "../core/Logger.hpp"
#include "../core/Application.hpp"
#include "../database/Database.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QHttpServerResponse>
#include <QTcpServer>
#include <QProcess>
#include <QCoreApplication>
#include <QTextStream>
#include <QHttpServerRequest>
#include <QUrlQuery>
#include <QJsonArray>

OverlayServer::OverlayServer(FileManager* fileManager, Database* database, QObject* parent)
    : QObject(parent)
    , m_wsServer(nullptr)
    , m_httpServer(nullptr)
    , m_fileManager(fileManager)
    , m_database(database)
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
    if (effect.type == "script") {
        QString scriptPath = effect.filePath;
        QString username = item.username;
        QString rewardId = item.rewardId;
        QString timestampStr = item.timestamp.toString(Qt::ISODate);

        if (m_database) {
            QString phpPath = m_database->getSetting("php_interpreter_path", "");
            QString perlPath = m_database->getSetting("perl_interpreter_path", "");

            QString interpreter;
            QStringList arguments;

            if (scriptPath.endsWith(".pl", Qt::CaseInsensitive) || scriptPath.endsWith(".cgi", Qt::CaseInsensitive)) {
                if (perlPath.isEmpty()) {
                    LOG_WARN("Perl interpreter path is not configured. Script execution skipped. Please set it in Settings.");
                } else {
                    interpreter = perlPath;
                    arguments << scriptPath << username << rewardId << timestampStr;
                }
            } else if (scriptPath.endsWith(".php", Qt::CaseInsensitive)) {
                if (phpPath.isEmpty()) {
                    LOG_WARN("PHP interpreter path is not configured. Script execution skipped. Please set it in Settings.");
                } else {
                    interpreter = phpPath;
                    arguments << scriptPath << username << rewardId << timestampStr;
                }
            }

            if (!interpreter.isEmpty()) {
                LOG_INFO(QString("Executing external script: %1 %2").arg(interpreter).arg(scriptPath));
                bool success = QProcess::startDetached(interpreter, arguments);
                if (!success) {
                    LOG_ERROR("Failed to start external script process.");
                }
            } else {
                LOG_WARN("No interpreter matched for script: " + scriptPath);
            }
        }
        
        emit effectFinished(item.queueId);
        return;
    }

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
    
    // {user} などのプレースホルダー置換
    QString processedText = effect.text;
    processedText.replace("{user}", item.username);
    processedText.replace("{reward_id}", item.rewardId);
    processedText.replace("{time}", item.timestamp.toLocalTime().toString("yyyy-MM-dd HH:mm:ss"));
    effectObj.insert("text", processedText);

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

    broadcastMessage(message);
}

void OverlayServer::broadcastStopAll()
{
    LOG_INFO("Broadcasting stop_all (Emergency Panic) command to OBS.");
    QJsonObject rootObj;
    rootObj.insert("type", "stop_all");
    
    QJsonDocument doc(rootObj);
    QString message = doc.toJson(QJsonDocument::Compact);

    broadcastMessage(message);
}

void OverlayServer::broadcastClearQueue()
{
    LOG_INFO("Broadcasting clear_queue command to OBS.");
    QJsonObject rootObj;
    rootObj.insert("type", "clear_queue");
    
    QJsonDocument doc(rootObj);
    QString message = doc.toJson(QJsonDocument::Compact);

    broadcastMessage(message);
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
        QString appDir = QCoreApplication::applicationDirPath();
        QString filePath = appDir + "/overlay.html";
        QFile file(filePath);
        QString html;

        if (file.exists()) {
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                in.setEncoding(QStringConverter::Utf8);
                html = in.readAll();
                file.close();
            }
        }

        if (html.isEmpty()) {
            // デフォルトのHTMLテンプレートを構築
            html = QString(R"HTML(<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="utf-8">
    <title>OBS Twitch Overlay</title>
    <style>
        body { margin: 0; background-color: transparent; overflow: hidden; }
        #overlay-container { width: 100vw; height: 100vh; position: relative; }
        /* max-width を vw で指定することで、位置によらず常に同じサイズで表示 */
        .overlay-item { position: absolute; text-align: center; max-width: 80vw; box-sizing: border-box; }
        .overlay-item img, .overlay-item video { max-width: 80vw; max-height: 80vh; display: block; }
        .overlay-text { font-weight: bold; margin-top: 10px; text-shadow: 2px 2px 4px #000; }
    </style>
</head>
<body>
    <div id="overlay-container"></div>
    <script>
        let currentEffect = null;

        function clearCurrentEffect() {
            if (currentEffect) {
                if (currentEffect.timeoutId) {
                    clearTimeout(currentEffect.timeoutId);
                }
                if (currentEffect.audio) {
                    try {
                        currentEffect.audio.pause();
                        currentEffect.audio.src = "";
                        currentEffect.audio.load();
                    } catch (e) {
                        console.error("Error clearing audio:", e);
                    }
                }
                if (currentEffect.wrapper) {
                    try {
                        currentEffect.wrapper.remove();
                    } catch (e) {
                        console.error("Error removing wrapper:", e);
                    }
                }
                currentEffect = null;
            }

            const container = document.getElementById("overlay-container");
            if (container) {
                container.innerHTML = "";
            }

            document.querySelectorAll("#overlay-container video").forEach(vid => {
                try {
                    vid.pause();
                    vid.src = "";
                    vid.load();
                    vid.remove();
                } catch(e){}
            });
        }

        // ポートはアプリ側の設定に合わせて自動置換されます（手動固定する場合は直接ポート番号を記述してください）
        const ws = new WebSocket("ws://localhost:{{WS_PORT}}/overlay");

        ws.onopen = () => {
            console.log("Connected to Twitch Overlay WebSocket Server");
        };

        ws.onmessage = (event) => {
            try {
                const msg = JSON.parse(event.data);
                
                if (msg.type === "show_effect") {
                    const data = msg.data;
                    
                    clearCurrentEffect();

                    const container = document.getElementById("overlay-container");
                    if (!container) return;

                    const wrapper = document.createElement("div");
                    wrapper.className = "overlay-item";
                    
                    const posX = data.effect.position.offsetX || 960;
                    const posY = data.effect.position.offsetY || 540;
                    wrapper.style.left      = posX + "px";
                    wrapper.style.top       = posY + "px";
                    const scaleVal = (data.effect.scale !== undefined ? data.effect.scale : 100) / 100.0;
                    wrapper.style.transform = `translate(-50%, -50%) scale(${scaleVal})`;

                    const clampToViewport = () => {
                        try {
                            const rect = wrapper.getBoundingClientRect();
                            const vw   = window.innerWidth;
                            const vh   = window.innerHeight;
                            let newX = posX;
                            let newY = posY;
                            if (rect.left   < 0)  newX += (-rect.left);
                            if (rect.top    < 0)  newY += (-rect.top);
                            if (rect.right  > vw) newX -= (rect.right  - vw);
                            if (rect.bottom > vh) newY -= (rect.bottom - vh);
                            if (newX !== posX || newY !== posY) {
                                wrapper.style.left = newX + "px";
                                wrapper.style.top  = newY + "px";
                            }
                        } catch (e) {
                            console.error("Error in clampToViewport:", e);
                        }
                    };

                    const hasValidFilePath = data.effect.filePath && !data.effect.filePath.endsWith('/assets/') && !data.effect.filePath.endsWith('/assets');
                    if (data.effect.type === "image" && hasValidFilePath) {
                        const img = document.createElement("img");
                        img.src = data.effect.filePath;
                        img.style.maxWidth  = "80vw";
                        img.style.maxHeight = "80vh";
                        img.onload  = clampToViewport;
                        img.onerror = clampToViewport;
                        wrapper.appendChild(img);
                    } else if (data.effect.type === "video" && hasValidFilePath) {
                        const vid = document.createElement("video");
                        vid.src = data.effect.filePath;
                        vid.autoplay = true;
                        vid.style.maxWidth  = "80vw";
                        vid.style.maxHeight = "80vh";
                        vid.onloadedmetadata = clampToViewport;
                        vid.onended = () => completeEffect();
                        vid.onerror = () => completeEffect();
                        wrapper.appendChild(vid);
                    }

                    if (data.effect.text) {
                        const txt = document.createElement("div");
                        txt.className = "overlay-text";
                        txt.innerText = data.effect.text;
                        const textStyle = data.effect.textStyle || {};
                        txt.style.fontFamily = textStyle.font || 'Arial';
                        txt.style.fontSize = (textStyle.size || 32) + 'px';
                        txt.style.color = textStyle.color || '#FFFFFF';
                        txt.style.webkitTextStroke = `${textStyle.borderWidth || 2}px ${textStyle.borderColor || '#000000'}`;
                        wrapper.appendChild(txt);
                    }

                    container.appendChild(wrapper);

                    let effectDone = false;
                    const completeEffect = () => {
                        if (effectDone) return;
                        effectDone = true;

                        if (currentEffect && currentEffect.audio) {
                            try {
                                currentEffect.audio.pause();
                            } catch(e){}
                        }

                        try {
                            wrapper.remove();
                        } catch(e){}

                        try {
                            if (ws.readyState === WebSocket.OPEN) {
                                ws.send(JSON.stringify({
                                    type: "effect_completed",
                                    data: { queueId: data.queueId }
                                }));
                            }
                        } catch (e) {
                            console.error("Failed to send effect_completed:", e);
                        }
                    };

                    currentEffect = {
                        queueId: data.queueId,
                        wrapper: wrapper,
                        audio: null,
                        timeoutId: null
                    };

                    const audioSrc = (data.effect.type === "sound")
                        ? (data.effect.audioPath || data.effect.filePath)
                        : data.effect.audioPath;

                    if (audioSrc) {
                        try {
                            const audio = new Audio(audioSrc);
                            currentEffect.audio = audio;
                            audio.volume = (data.effect.volume || 80) / 100;
                            if (data.effect.type === "sound") {
                                audio.onended = completeEffect;
                                audio.onerror = completeEffect;
                            }
                            audio.play().catch(e => {
                                console.log("Audio play blocked:", e);
                                if (data.effect.type === "sound") completeEffect();
                            });
                        } catch (e) {
                            console.error("Failed to play audio:", e);
                            if (data.effect.type === "sound") completeEffect();
                        }
                    }

                    currentEffect.timeoutId = setTimeout(completeEffect, (data.effect.duration || 5) * 1000);
                } else if (msg.type === "stop_all") {
                    clearCurrentEffect();
                }
            } catch (e) {
                console.error("Error processing message:", e);
            }
        };
    </script>
</body>
</html>
)HTML");

            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out.setEncoding(QStringConverter::Utf8);
                out << html;
                file.close();
                LOG_INFO("Created default overlay.html template in application directory: " + filePath);
            }
        }

        // WebSocketのポート設定を動的に注入
        html.replace("{{WS_PORT}}", QString::number(m_wsPort));

        QHttpServerResponse response("text/html; charset=utf-8", html.toUtf8());
        QHttpHeaders headers;
        headers.append("Access-Control-Allow-Origin", "*");
        headers.append("Cache-Control", "no-cache, no-store, must-revalidate");
        response.setHeaders(headers);
        return response;
    });

    // ランキングデータ取得用API (JSON)
    m_httpServer->route("/api/ranking", [this](const QHttpServerRequest& request) {
        int period = 0; // デフォルトは今日 (Today)
        
        // クエリパラメータからperiod（期間）を取得
        QUrlQuery query(request.url().query());
        if (query.hasQueryItem("period")) {
            period = query.queryItemValue("period").toInt();
        }

        QJsonArray arr;
        if (m_database) {
            QList<QPair<QString, int>> ranking = m_database->getRanking(period);
            for (const auto& pair : ranking) {
                QJsonObject obj;
                obj.insert("name", pair.first);
                obj.insert("count", pair.second);
                arr.append(obj);
            }
        }

        QJsonDocument doc(arr);
        QHttpServerResponse response("application/json; charset=utf-8", doc.toJson(QJsonDocument::Compact));
        QHttpHeaders headers;
        headers.append("Access-Control-Allow-Origin", "*");
        headers.append("Cache-Control", "no-cache, no-store, must-revalidate");
        response.setHeaders(headers);
        return response;
    });

    // ランキング表示用HTMLページ
    m_httpServer->route("/ranking", [this]() {
        QString appDir = QCoreApplication::applicationDirPath();
        QString filePath = appDir + "/ranking.html";
        QFile file(filePath);
        QString html;

        if (file.exists()) {
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                in.setEncoding(QStringConverter::Utf8);
                html = in.readAll();
                file.close();
            }
        }

        if (html.isEmpty()) {
            // デフォルトのランキングHTMLテンプレートを自動生成
            html = QString(R"HTML(<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="utf-8">
    <title>Twitch Channel Point Leaderboard</title>
    <style>
        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            margin: 0;
            background-color: transparent;
            color: #FFFFFF;
            overflow: hidden;
        }
        .leaderboard-card {
            background: rgba(29, 29, 34, 0.85);
            backdrop-filter: blur(10px);
            border-top: 4px solid #6441A5;
            border-radius: 12px;
            padding: 20px;
            width: 320px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.5);
            margin: 20px;
        }
        h2 {
            font-size: 18px;
            margin-top: 0;
            margin-bottom: 15px;
            color: #E1E1E6;
            text-align: center;
            letter-spacing: 1px;
            border-bottom: 1px solid rgba(255,255,255,0.1);
            padding-bottom: 8px;
        }
        .ranking-list {
            list-style: none;
            padding: 0;
            margin: 0;
        }
        .ranking-item {
            display: flex;
            align-items: center;
            padding: 8px 10px;
            border-bottom: 1px solid rgba(255,255,255,0.05);
            transition: all 0.3s;
        }
        .ranking-item:last-child {
            border-bottom: none;
        }
        .rank {
            font-weight: bold;
            font-size: 16px;
            width: 30px;
            text-align: center;
        }
        .rank-1 { color: #FFD700; }
        .rank-2 { color: #C0C0C0; }
        .rank-3 { color: #CD7F32; }
        .name {
            flex-grow: 1;
            font-size: 14px;
            padding-left: 10px;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .count {
            font-weight: bold;
            color: #6441A5;
            background: rgba(100, 65, 165, 0.2);
            padding: 2px 8px;
            border-radius: 12px;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <div class="leaderboard-card">
        <h2>🏆 本日の演出使用数ランキング</h2>
        <ul id="leaderboard" class="ranking-list">
            <!-- JavaScriptで動的に生成 -->
        </ul>
    </div>
    <script>
        async function fetchRanking() {
            try {
                // ポート番号はアプリ側で自動置換されます
                const res = await fetch("http://localhost:{{HTTP_PORT}}/api/ranking?period=0");
                const data = await res.json();
                
                const list = document.getElementById("leaderboard");
                list.innerHTML = "";
                
                if (data.length === 0) {
                    list.innerHTML = "<li class='ranking-item' style='justify-content: center; color: #aaa;'>データがありません</li>";
                    return;
                }

                data.forEach((item, index) => {
                    const li = document.createElement("li");
                    li.className = "ranking-item";
                    
                    const rankNum = index + 1;
                    let rankClass = `rank rank-${rankNum}`;
                    if (rankNum > 3) rankClass = "rank";

                    li.innerHTML = `
                        <span class="${rankClass}">${rankNum}</span>
                        <span class="name">${item.name}</span>
                        <span class="count">${item.count}回</span>
                    `;
                    list.appendChild(li);
                });
            } catch (e) {
                console.error("Failed to fetch ranking:", e);
            }
        }

        // 5秒ごとに最新ランキングを自動更新
        fetchRanking();
        setInterval(fetchRanking, 5000);
    </script>
</body>
</html>
)HTML");

            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out.setEncoding(QStringConverter::Utf8);
                out << html;
                file.close();
                LOG_INFO("Created default ranking.html template in application directory: " + filePath);
            }
        }

        // HTTPサーバーのポート設定を動的に注入
        html.replace("{{HTTP_PORT}}", QString::number(m_httpPort));

        QHttpServerResponse response("text/html; charset=utf-8", html.toUtf8());
        QHttpHeaders headers;
        headers.append("Access-Control-Allow-Origin", "*");
        headers.append("Cache-Control", "no-cache, no-store, must-revalidate");
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

void OverlayServer::broadcastMessage(const QString& message)
{
    QList<QWebSocket*> activeClients;
    for (auto* client : m_clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->sendTextMessage(message);
            activeClients.append(client);
        } else {
            if (client) {
                client->deleteLater();
            }
        }
    }
    if (m_clients.size() != activeClients.size()) {
        m_clients = activeClients;
        emit clientCountChanged(m_clients.size());
    }
}

