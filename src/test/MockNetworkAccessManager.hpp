#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMap>
#include <QUrl>
#include <QBuffer>
#include <QTimer>
#include <QDebug>

// テスト用にHTTPレスポンスを偽装するカスタム QNetworkReply
class MockNetworkReply : public QNetworkReply {
    Q_OBJECT
private:
    QByteArray m_responseData;
    QBuffer* m_buffer;

public:
    MockNetworkReply(int statusCode, const QByteArray& contentType, const QByteArray& data, QObject* parent = nullptr)
        : QNetworkReply(parent)
        , m_responseData(data)
    {
        m_buffer = new QBuffer(&m_responseData, this);
        m_buffer->open(QIODevice::ReadOnly);

        setOperation(QNetworkAccessManager::PostOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, statusCode);
        setHeader(QNetworkRequest::ContentTypeHeader, contentType);

        setOpenMode(QIODevice::ReadOnly);
        setError(QNetworkReply::NoError, "No error");
        setFinished(true);

        // レプライが完了したことをスロットへ非同期通知するために即座にタイマー起動
        QTimer::singleShot(0, this, [this]() {
            qDebug() << "MockNetworkReply: SingleShot triggered! Emitting readyRead and finished.";
            emit readyRead();
            emit finished();
        });
    }

    ~MockNetworkReply() override = default;

    void abort() override {}
    qint64 bytesAvailable() const override {
        return m_buffer->bytesAvailable() + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char* data, qint64 maxlen) override {
        return m_buffer->read(data, maxlen);
    }
};

// 特定のURLに対して期待するHTTPレスポンスを事前に注入できるスタブ
class MockNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
private:
    // URL -> (HTTPステータスコード, レスポンスボディJSON)
    QMap<QUrl, QPair<int, QByteArray>> m_stubbedResponses;
    QByteArray m_defaultContentType;

public:
    explicit MockNetworkAccessManager(QObject* parent = nullptr)
        : QNetworkAccessManager(parent)
        , m_defaultContentType("application/json")
    {
    }

    // テスト前にURLごとの期待レスポンスを設定するメソッド
    void setExpectedResponse(const QUrl& url, int statusCode, const QByteArray& jsonResponse) {
        m_stubbedResponses.insert(url, qMakePair(statusCode, jsonResponse));
    }

    void setDefaultContentType(const QByteArray& contentType) {
        m_defaultContentType = contentType;
    }

    void clearResponses() {
        m_stubbedResponses.clear();
    }

protected:
    // リクエスト発生時に呼び出されるメソッドをオーバーライドしてインターセプト
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request, QIODevice* outgoingData = nullptr) override {
        Q_UNUSED(op);
        Q_UNUSED(outgoingData);

        QUrl url = request.url();
        qDebug() << "MockNetworkAccessManager::createRequest called for URL:" << url.toString();
        int statusCode = 404;
        QByteArray responseData = R"({"error": "Stubbed response not found"})";

        // 事前登録されたスタブレスポンスを引き当てる
        if (m_stubbedResponses.contains(url)) {
            auto pair = m_stubbedResponses.value(url);
            statusCode = pair.first;
            responseData = pair.second;
        }

        return new MockNetworkReply(statusCode, m_defaultContentType, responseData, this);
    }
};
