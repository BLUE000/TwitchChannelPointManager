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
    ~Config() = default;

    // 基本的な設定の読み込み・書き込み
    bool load();
    bool save();
    
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void set(const QString& key, const QVariant& value);
    void remove(const QString& key) { m_settings.remove(key); }

    // TransCipher-Dist を使用した暗号化保存/復号読み込み
    bool saveSecureString(const QString& key, const QString& plainText, const QString& secretKey);
    QString loadSecureString(const QString& key, const QString& secretKey, const QString& defaultValue = "") const;
};
