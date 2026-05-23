#pragma once

#include <QString>
#include <QStringList>
#include <QSet>

class HTMLSanitizer {
private:
    // 許可されたHTMLタグのホワイトリスト（小文字）
    static const QSet<QString> s_allowedTags;
    // 許可された属性のホワイトリスト（小文字）
    static const QSet<QString> s_allowedAttributes;

public:
    HTMLSanitizer() = delete; // 静的ユーティリティクラス

    // HTMLファイルを読み込んでクレンジング（サニタイズ）するメイン関数
    static QString sanitizeHtml(const QString& rawHtml);

private:
    // タグの内側の属性をパース・クレンジングする
    static QString sanitizeTagAttributes(const QString& tagName, const QString& rawAttrs);

    // CSS（style属性やstyleタグ）の中身をクレンジングして外部参照を除去する
    static QString sanitizeCss(const QString& rawCss);

    // 画像の src 属性がローカルの許可されたファイルか検証する
    static bool isValidLocalImageSource(const QString& src);
};
