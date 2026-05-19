#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>

// 演出用の位置設定
struct EffectPosition {
    QString preset = "center"; // "center", "top_left", "top_right", "bottom_left", "bottom_right", "custom"
    int offsetX = 0;
    int offsetY = 0;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert("preset", preset);
        obj.insert("offsetX", offsetX);
        obj.insert("offsetY", offsetY);
        return obj;
    }

    static EffectPosition fromJson(const QJsonObject& obj) {
        EffectPosition pos;
        pos.preset = obj.value("preset").toString();
        if (pos.preset.isEmpty()) pos.preset = "center"; // 空文字はデフォルトのcenterに
        pos.offsetX = obj.value("offsetX").toInt(0);
        pos.offsetY = obj.value("offsetY").toInt(0);
        return pos;
    }
};

// 演出テキストの装飾スタイル
struct TextStyle {
    QString font = "Arial";
    int size = 32;
    QString color = "#FFFFFF";
    QString borderColor = "#000000";
    int borderWidth = 2;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert("font", font);
        obj.insert("size", size);
        obj.insert("color", color);
        obj.insert("borderColor", borderColor);
        obj.insert("borderWidth", borderWidth);
        return obj;
    }

    static TextStyle fromJson(const QJsonObject& obj) {
        TextStyle style;
        style.font = obj.value("font").toString("Arial");
        style.size = obj.value("size").toInt(32);
        style.color = obj.value("color").toString("#FFFFFF");
        style.borderColor = obj.value("borderColor").toString("#000000");
        style.borderWidth = obj.value("borderWidth").toInt(2);
        return style;
    }
};

// 個々の演出データ
struct Effect {
    QString type;      // "image", "video", "sound", "text"
    QString filePath;  // 画像/動画の実ファイルパス
    QString audioPath; // 音声の実ファイルパス
    int duration = 5;  // 演出時間（秒）
    EffectPosition position;
    QString animation = "fade"; // "fade", "slide", "bounce", "none"
    int volume = 80;            // 音量 (0 - 100)
    QString text;               // 表示用テキスト
    TextStyle textStyle;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert("type", type);
        obj.insert("filePath", filePath);
        obj.insert("audioPath", audioPath);
        obj.insert("duration", duration);
        obj.insert("position", position.toJson());
        obj.insert("animation", animation);
        obj.insert("volume", volume);
        obj.insert("text", text);
        obj.insert("textStyle", textStyle.toJson());
        return obj;
    }

    static Effect fromJson(const QJsonObject& obj) {
        Effect eff;
        eff.type = obj.value("type").toString();
        eff.filePath = obj.value("filePath").toString();
        eff.audioPath = obj.value("audioPath").toString();
        eff.duration = obj.value("duration").toInt(5);
        eff.position = EffectPosition::fromJson(obj.value("position").toObject());
        eff.animation = obj.value("animation").toString("fade");
        eff.volume = obj.value("volume").toInt(80);
        eff.text = obj.value("text").toString();
        eff.textStyle = TextStyle::fromJson(obj.value("textStyle").toObject());
        return eff;
    }
};

// チャンネルポイント報酬データ
struct Reward {
    QString id;                 // Twitch 報酬ID
    QString name;               // 報酬名
    int cost = 0;               // 必要ポイント数
    int cooldown = 0;           // クールタイム（秒）
    QList<QString> allowedRoles; // 利用許可ロール ("everyone", "subscriber", "vip")
    bool enabled = true;
    QString mode = "sequential"; // 演出の再生モード: "sequential"（全て再生） / "random"（ランダムで1つ）
    QList<Effect> effects;       // 設定された演出のリスト

    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert("id", id);
        obj.insert("name", name);
        obj.insert("cost", cost);
        obj.insert("cooldown", cooldown);
        
        QJsonArray rolesArr;
        for (const auto& role : allowedRoles) {
            rolesArr.append(role);
        }
        obj.insert("allowedRoles", rolesArr);
        
        obj.insert("enabled", enabled);
        obj.insert("mode", mode);

        QJsonArray effectsArr;
        for (const auto& effect : effects) {
            effectsArr.append(effect.toJson());
        }
        obj.insert("effects", effectsArr);

        return obj;
    }

    static Reward fromJson(const QJsonObject& obj) {
        Reward r;
        r.id = obj.value("id").toString();
        r.name = obj.value("name").toString();
        r.cost = obj.value("cost").toInt(0);
        r.cooldown = obj.value("cooldown").toInt(0);
        
        QJsonArray rolesArr = obj.value("allowedRoles").toArray();
        for (const auto& val : rolesArr) {
            r.allowedRoles.append(val.toString());
        }
        
        r.enabled = obj.value("enabled").toBool(true);
        r.mode = obj.value("mode").toString("sequential");

        QJsonArray effectsArr = obj.value("effects").toArray();
        for (const auto& val : effectsArr) {
            r.effects.append(Effect::fromJson(val.toObject()));
        }

        return r;
    }
};
