#include "Config.hpp"
#include "../../lib/TransCipher-Dist/include/cipher_engine.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

Config::Config(const QString& path, QObject* parent)
    : QObject(parent)
    , m_configPath(path)
{
}

bool Config::load()
{
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file for reading:" << m_configPath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull()) {
        qWarning() << "Failed to parse JSON config:" << parseError.errorString();
        return false;
    }

    QJsonObject obj = doc.object();
    m_settings.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_settings.insert(it.key(), it.value().toVariant());
    }

    return true;
}

bool Config::save()
{
    QJsonObject obj;
    for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }

    QJsonDocument doc(obj);
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << m_configPath;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QVariant Config::get(const QString& key, const QVariant& defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

void Config::set(const QString& key, const QVariant& value)
{
    m_settings.insert(key, value);
}

bool Config::saveSecureString(const QString& key, const QString& plainText, const QString& secretKey)
{
    if (plainText.isEmpty()) {
        set(key, "");
        return true;
    }

    QByteArray plainData = plainText.toUtf8();
    
    // TransCipher-Dist を用いた暗号化の実行
    CipherResult result = CipherEngine::encrypt(plainData, secretKey, AesMode::Mandatory);
    if (!result.isSuccess()) {
        qWarning() << "Failed to encrypt secure string for key:" << key << "-" << result.message();
        return false;
    }

    // 暗号化バイナリをBase64にエンコードして設定に保存
    QByteArray base64Data = result.data().toBase64();
    set(key, QString::fromUtf8(base64Data));
    return true;
}

QString Config::loadSecureString(const QString& key, const QString& secretKey, const QString& defaultValue) const
{
    QVariant val = get(key);
    if (!val.isValid() || val.toString().isEmpty()) {
        return defaultValue;
    }

    QByteArray base64Data = val.toString().toUtf8();
    QByteArray cipherData = QByteArray::fromBase64(base64Data);

    // TransCipher-Dist を用いた復号の実行
    CipherResult result = CipherEngine::decrypt(cipherData, secretKey);
    if (!result.isSuccess()) {
        qWarning() << "Failed to decrypt secure string for key:" << key << "-" << result.message();
        return defaultValue;
    }

    return QString::fromUtf8(result.data());
}
