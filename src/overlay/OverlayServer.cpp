#include "OverlayServer.hpp"
#include "../core/HTMLSanitizer.hpp"
#include "FileManager.hpp"
#include "../core/Logger.hpp"
#include "../core/Application.hpp"
#include "../database/Database.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHttpServerResponse>
#include <QTcpServer>
#include <QProcess>
#include <QCoreApplication>
#include <QTextStream>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QHttpHeaders>
#include <QTimer>
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
    if (effect.isCustomHtmlOnly) {
        // 1. HTML演出の配信（OBSへ送信）
        if (!effect.htmlPath.isEmpty()) {
            QString appDir = QCoreApplication::applicationDirPath();
            // 相対パスを絶対パスへ変換
            QString absoluteHtmlPath = QDir::toNativeSeparators(appDir + "/" + effect.htmlPath);
            QString servedHtmlUrl = m_fileManager->registerAsset(absoluteHtmlPath);
            
            QJsonObject payload;
            payload.insert("type", "show_custom_html");
            payload.insert("url", servedHtmlUrl);
            payload.insert("duration", effect.duration);
            payload.insert("queueId", item.queueId);

            QJsonDocument doc(payload);
            QString msg = doc.toJson(QJsonDocument::Compact);
            
            LOG_INFO("Broadcasting custom HTML overlay to OBS: " + servedHtmlUrl);
            for (auto* client : m_clients) {
                client->sendTextMessage(msg);
            }
        }

        // 表示時間経過後に完了を通知し、演出順次キューの同期をとる
        int waitTime = qMax(1, effect.duration);
        QTimer::singleShot(waitTime * 1000, this, [this, item]() {
            emit effectFinished(item.queueId);
        });
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
    m_httpServer->route("/assets/<arg>", [this](const QString& rawFilename, QHttpServerResponder& responder) {
        // 先頭スラッシュなど余分な文字を除去して正規化
        QString filename = rawFilename;
        while (filename.startsWith('/')) {
            filename = filename.mid(1);
        }

        LOG_INFO("Asset requested: " + filename);
        QString realPath = m_fileManager->getRealPath(filename);
        if (realPath.isEmpty()) {
            LOG_WARN("Asset not found in registry: " + filename);
            responder.write(QHttpServerResponder::StatusCode::NotFound);
            return;
        }

        // HTML/htm アセットの場合はサニタイズ処理のためメモリ上に一旦展開
        QString mime = getMimeType(realPath);
        if (mime == "text/html" || realPath.endsWith(".html", Qt::CaseInsensitive) || realPath.endsWith(".htm", Qt::CaseInsensitive)) {
            QFile file(realPath);
            if (!file.open(QIODevice::ReadOnly)) {
                LOG_ERROR("Failed to open real HTML asset file for serving: " + realPath);
                responder.write(QHttpServerResponder::StatusCode::InternalServerError);
                return;
            }
            QByteArray fileData = file.readAll();
            file.close();

            QString rawHtml = QString::fromUtf8(fileData);
            QString sanitizedHtml = HTMLSanitizer::sanitizeHtml(rawHtml);
            fileData = sanitizedHtml.toUtf8();

            QHttpHeaders headers;
            headers.append("Content-Type", mime.toUtf8());
            headers.append("Access-Control-Allow-Origin", "*");
            headers.append("Cache-Control", "no-cache, no-store, must-revalidate");
            responder.write(fileData, headers, QHttpServerResponder::StatusCode::Ok);
            return;
        }

        // 画像、動画、効果音アセットは大容量の可能性があるため、QHttpServerResponderを用いてストリーム配信
        auto* file = new QFile(realPath);
        if (!file->open(QIODevice::ReadOnly)) {
            LOG_ERROR("Failed to open real asset file for streaming: " + realPath);
            delete file;
            responder.write(QHttpServerResponder::StatusCode::InternalServerError);
            return;
        }

        QHttpHeaders headers;
        headers.append("Content-Type", mime.toUtf8());
        headers.append("Access-Control-Allow-Origin", "*");
        headers.append("Cache-Control", "no-cache, no-store, must-revalidate");

        // QFileの所有権は QHttpServerResponder が引き受け、配信完了時に自動で削除される
        responder.write(file, headers, QHttpServerResponder::StatusCode::Ok);
    });

    // テスト兼OBS登録用のデフォルトオーバーレイHTMLページ
    m_httpServer->route("/overlay", [this]() {
        QString appDir = QCoreApplication::applicationDirPath();
        QString filePath = appDir + "/overlay.html";
        QFile file(filePath);
        QString html;

        // デフォルトのHTMLテンプレートを構築
        html = QString(R"HTML(<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="utf-8">
    <title>OBS Twitch Overlay</title>
    <style>
        body { margin: 0; background-color: transparent; overflow: hidden; }
        #overlay-container { width: 100vw; height: 100vh; position: relative; }
        /* position: absolute のままで width: max-content にし、left: 0; top: 0; に配置することで折り返し幅を一定に保つ */
        .overlay-item { position: absolute; text-align: center; width: max-content; max-width: 80vw; box-sizing: border-box; }
        .overlay-item img, .overlay-item video { max-width: 80vw; max-height: 80vh; display: block; }
        /* 長文テキストの見切れを防ぐために max-width: 600px を設定し、自動で折り返されるようにする */
        .overlay-text {
            font-weight: bold;
            margin: 10px auto 0 auto;
            text-shadow: 2px 2px 4px #000;
            max-width: 600px;
            word-wrap: break-word;
            overflow-wrap: break-word;
            display: inline-block;
        }
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
                
                if (msg.type === "show_custom_html") {
                    const data = msg;
                    clearCurrentEffect();

                    const container = document.getElementById("overlay-container");
                    if (!container) return;

                    const iframe = document.createElement("iframe");
                    iframe.src = data.url;
                    iframe.style.width = "100vw";
                    iframe.style.height = "100vh";
                    iframe.style.border = "none";
                    iframe.style.backgroundColor = "transparent";
                    iframe.style.position = "absolute";
                    iframe.style.left = "0";
                    iframe.style.top = "0";

                    container.appendChild(iframe);

                    let effectDone = false;
                    const completeEffect = () => {
                        if (effectDone) return;
                        effectDone = true;
                        try {
                            iframe.remove();
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
                        wrapper: iframe,
                        audio: null,
                        timeoutId: setTimeout(completeEffect, (data.duration || 5) * 1000)
                    };
                } else if (msg.type === "show_effect") {
                    const data = msg.data;
                    
                    clearCurrentEffect();

                    const container = document.getElementById("overlay-container");
                    if (!container) return;

                    const wrapper = document.createElement("div");
                    wrapper.className = "overlay-item";
                    
                    const posX = data.effect.position.offsetX || 960;
                    const posY = data.effect.position.offsetY || 540;
                    // left, top は 0px に固定し、要素の折り返し幅が表示位置に依存しないようにする
                    wrapper.style.left      = "0px";
                    wrapper.style.top       = "0px";
                    const scaleVal = (data.effect.scale !== undefined ? data.effect.scale : 100) / 100.0;
                    // transform の中で平行移動と中央揃えを同時に適用し、サイズを一定に維持する
                    wrapper.style.transform = `translate(calc(${posX}px - 50%), calc(${posY}px - 50%)) scale(${scaleVal})`;

                    const hasValidFilePath = data.effect.filePath && !data.effect.filePath.endsWith('/assets/') && !data.effect.filePath.endsWith('/assets');
                    if (data.effect.type === "image" && hasValidFilePath) {
                        const img = document.createElement("img");
                        img.src = data.effect.filePath;
                        img.style.maxWidth  = "80vw";
                        img.style.maxHeight = "80vh";
                        wrapper.appendChild(img);
                    } else if (data.effect.type === "video" && hasValidFilePath) {
                        const vid = document.createElement("video");
                        vid.src = data.effect.filePath;
                        vid.autoplay = true;
                        vid.style.maxWidth  = "80vw";
                        vid.style.maxHeight = "80vh";
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

        // 常に最新のデフォルトテンプレートをファイルに書き出し同期する
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            out << html;
            file.close();
            LOG_INFO("Synced/Updated default overlay.html template in application directory: " + filePath);
        } else {
            LOG_ERROR("Failed to write/sync default overlay.html template: " + filePath);
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

    // ランキング表示用HTMLページ（/ranking は /ranking/default.html へフォールバック）
    m_httpServer->route("/ranking", [this]() {
        QString rankingDir = QCoreApplication::applicationDirPath() + "/ranking";
        QString filePath = rankingDir + "/default.html";
        QFile file(filePath);
        QString html;

        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            in.setEncoding(QStringConverter::Utf8);
            html = in.readAll();
            file.close();
        }

        if (html.isEmpty()) {
            html = "<html><body style='color:white;background:#121214;font-family:sans-serif;padding:20px'>"
                   "<h2>⚠️ ranking/default.html が見つかりません</h2>"
                   "<p>アプリを再インストールするか、ranking/ フォルダに default.html を配置してください。</p>"
                   "</body></html>";
        }

        // {{HTTP_PORT}} を実際のポート番号へ置換
        html.replace("{{HTTP_PORT}}", QString::number(m_httpPort));

        QHttpServerResponse response("text/html; charset=utf-8", html.toUtf8());
        QHttpHeaders headers;
        headers.append("Access-Control-Allow-Origin", "*");
        headers.append("Cache-Control", "no-cache, no-store, must-revalidate");
        response.setHeaders(headers);
        return response;
    });

    // ランキング用ファイル名指定ルート（HTML静的配信 / PHP・CGI実行対応）
    m_httpServer->route("/ranking/<arg>", [this](const QString& fileName) {
        QString rankingDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/ranking");

        // ファイル名からパストラバーサル攻撃を防ぐ（ranking/ フォルダ外へのアクセスを禁止）
        QString requestedPath = QDir::cleanPath(rankingDir + "/" + fileName);
        QString canonicalRankingDir = QDir(rankingDir).canonicalPath();
        if (!canonicalRankingDir.isEmpty() && !requestedPath.startsWith(canonicalRankingDir)) {
            LOG_WARN(QString("Path traversal attempt blocked: %1").arg(fileName));
            return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);
        }

        QFileInfo fileInfo(requestedPath);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            LOG_WARN(QString("Ranking file not found: %1").arg(requestedPath));
            return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
        }

        QString ext = fileInfo.suffix().toLower();

        // ──────────────────────────────────────────────
        // HTML: 静的ファイルとして配信
        // ──────────────────────────────────────────────
        if (ext == "html" || ext == "htm") {
            QFile file(requestedPath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
            }
            QTextStream in(&file);
            in.setEncoding(QStringConverter::Utf8);
            QString html = in.readAll();
            file.close();

            // 配信前に超厳格サニタイズ処理を通す
            html = HTMLSanitizer::sanitizeHtml(html);

            // {{HTTP_PORT}} を実際のポート番号へ置換（カスタムHTMLでも使えるようにする）
            html.replace("{{HTTP_PORT}}", QString::number(m_httpPort));

            QHttpServerResponse response("text/html; charset=utf-8", html.toUtf8());
            QHttpHeaders headers;
            headers.append("Access-Control-Allow-Origin", "*");
            headers.append("Cache-Control", "no-cache, no-store, must-revalidate");
            response.setHeaders(headers);
            return response;
        }


        // 対応していない拡張子は404
        LOG_WARN(QString("Unsupported file type in ranking/: %1").arg(fileName));
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
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

