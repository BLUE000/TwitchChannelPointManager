#include "HTMLSanitizer.hpp"
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

// 許可されたHTMLタグのホワイトリスト（小文字）
const QSet<QString> HTMLSanitizer::s_allowedTags = {
    "div", "span", "p", "br", "table", "tr", "td", "img", "style"
};

// 許可された属性のホワイトリスト（小文字）
const QSet<QString> HTMLSanitizer::s_allowedAttributes = {
    "id", "class", "style", "src", "colspan", "rowspan", "align", "valign"
};

QString HTMLSanitizer::sanitizeHtml(const QString& rawHtml)
{
    if (rawHtml.isEmpty()) {
        return "";
    }

    // 1. 全体に対して JavaScript スキームや怪しい記述を事前クレンジング
    QString processed = rawHtml;
    // <script> ... </script> ブロックを中身（JavaScriptコード自体）ごと完全に消去
    processed.replace(QRegularExpression("<script\\b[^>]*>[\\s\\S]*?</script>", QRegularExpression::CaseInsensitiveOption), "");
    processed.replace(QRegularExpression("javascript\\s*:", QRegularExpression::CaseInsensitiveOption), "clean-javascript:");

    // 2. HTMLタグを走査してホワイトリスト制限を適用
    // タグ全体をマッチする正規表現
    QRegularExpression tagRegex("<(/?[a-zA-Z0-9:]+)(\\s+[^>]*)?>");
    QRegularExpressionMatchIterator it = tagRegex.globalMatch(processed);
    
    // 文字列置換時にインデックスが変わらないように後ろから置換する
    struct TagReplacement {
        int offset;
        int length;
        QString text;
    };
    QList<TagReplacement> replacements;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString fullTag = match.captured(0);
        QString tagName = match.captured(1).trimmed().toLower();
        QString rawAttrs = match.captured(2);

        bool isClosingTag = tagName.startsWith('/');
        QString checkTagName = isClosingTag ? tagName.mid(1) : tagName;

        TagReplacement rep;
        rep.offset = match.capturedStart(0);
        rep.length = match.capturedLength(0);

        if (!s_allowedTags.contains(checkTagName)) {
            // ホワイトリストに含まれないタグは完全に除去
            rep.text = "";
        } else {
            // 閉じタグの場合はそのまま残す
            if (isClosingTag) {
                rep.text = fullTag;
            } else {
                // 開始タグの場合は属性をサニタイズ
                QString sanitizedAttrs = sanitizeTagAttributes(checkTagName, rawAttrs);
                if (sanitizedAttrs.isEmpty()) {
                    rep.text = QString("<%1>").arg(checkTagName);
                } else {
                    rep.text = QString("<%1 %2>").arg(checkTagName).arg(sanitizedAttrs);
                }
            }
        }
        replacements.append(rep);
    }

    // 後ろから置換適用
    for (int i = replacements.size() - 1; i >= 0; --i) {
        const auto& rep = replacements[i];
        processed.replace(rep.offset, rep.length, rep.text);
    }

    // 3. styleタグブロック内のCSS自体をサニタイズ
    QRegularExpression styleBlockRegex("(<style[^>]*>)([^<]*)(</style>)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator styleIt = styleBlockRegex.globalMatch(processed);
    QList<TagReplacement> styleReplacements;

    while (styleIt.hasNext()) {
        QRegularExpressionMatch match = styleIt.next();
        QString openTag = match.captured(1);
        QString cssContent = match.captured(2);
        QString closeTag = match.captured(3);

        TagReplacement rep;
        rep.offset = match.capturedStart(0);
        rep.length = match.capturedLength(0);
        rep.text = openTag + sanitizeCss(cssContent) + closeTag;
        styleReplacements.append(rep);
    }

    for (int i = styleReplacements.size() - 1; i >= 0; --i) {
        const auto& rep = styleReplacements[i];
        processed.replace(rep.offset, rep.length, rep.text);
    }

    return processed;
}

QString HTMLSanitizer::sanitizeTagAttributes(const QString& tagName, const QString& rawAttrs)
{
    if (rawAttrs.isEmpty()) {
        return "";
    }

    // 属性名=値 のペアを抽出する正規表現
    QRegularExpression attrRegex("([a-zA-Z0-9\\-]+)(?:\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]*)))?");
    QRegularExpressionMatchIterator it = attrRegex.globalMatch(rawAttrs);

    QStringList sanitizedList;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString attrName = match.captured(1).trimmed().toLower();
        
        // 引用符に応じた属性値の抽出
        QString attrVal = match.captured(2);
        if (attrVal.isEmpty()) attrVal = match.captured(3);
        if (attrVal.isEmpty()) attrVal = match.captured(4);
        attrVal = attrVal.trimmed();

        // 1. 属性名がホワイトリストにない、または on で始まるイベントハンドラは完全排除
        if (!s_allowedAttributes.contains(attrName) || attrName.startsWith("on")) {
            continue;
        }

        // 2. imgタグ以外の src 属性は安全のため排除
        if (attrName == "src" && tagName != "img") {
            continue;
        }

        // 3. imgのsrc属性に対する厳格なローカル制限
        if (attrName == "src" && tagName == "img") {
            if (!isValidLocalImageSource(attrVal)) {
                // 不正な画像ソースの場合は属性ごとスキップ（または空文字列に設定）
                continue;
            }
        }

        // 4. style属性のサニタイズ（外部参照の除去）
        if (attrName == "style") {
            attrVal = sanitizeCss(attrVal);
        }

        // サニライズされた属性の組み立て
        if (attrVal.isEmpty()) {
            sanitizedList.append(QString("%1=\"\"").arg(attrName));
        } else {
            // ダブルクォーテーションのエスケープを考慮
            QString escapedVal = attrVal;
            escapedVal.replace("\"", "&quot;");
            sanitizedList.append(QString("%1=\"%2\"").arg(attrName).arg(escapedVal));
        }
    }

    return sanitizedList.join(" ");
}

QString HTMLSanitizer::sanitizeCss(const QString& rawCss)
{
    if (rawCss.isEmpty()) {
        return "";
    }

    QString cleanCss = rawCss;

    // 1. @import の完全排除
    cleanCss.replace(QRegularExpression("@import\\s+[^;]+;?", QRegularExpression::CaseInsensitiveOption), "");

    // 2. url(...) の完全排除 (フォントや画像の外部読み込み、SVGデータ埋め込みを防止)
    cleanCss.replace(QRegularExpression("url\\s*\\(\\s*[^\\)]*\\)", QRegularExpression::CaseInsensitiveOption), "none");

    // 3. style内の式 (expression) や挙動定義 (behavior) などの非標準で危険なコードの除去
    cleanCss.replace(QRegularExpression("expression\\s*\\(\\s*[^\\)]*\\)", QRegularExpression::CaseInsensitiveOption), "");
    cleanCss.replace(QRegularExpression("behavior\\s*:", QRegularExpression::CaseInsensitiveOption), "clean-behavior:");

    return cleanCss;
}

bool HTMLSanitizer::isValidLocalImageSource(const QString& src)
{
    if (src.isEmpty()) {
        return false;
    }

    QString cleanSrc = src.trimmed();

    // 1. 外部接続（http://, https://）は100%禁止
    if (cleanSrc.startsWith("http://", Qt::CaseInsensitive) || 
        cleanSrc.startsWith("https://", Qt::CaseInsensitive) ||
        cleanSrc.startsWith("//")) {
        return false;
    }

    // 2. dataスキーム（SVGなどのインライン埋め込み）は禁止
    if (cleanSrc.startsWith("data:", Qt::CaseInsensitive)) {
        return false;
    }

    // 3. 許可された画像拡張子（PNG, JPG, JPEG, GIF）のみ通す（SVGは除外）
    // クエリパラメータやハッシュ付きのURL対策として、純粋なパス部分だけを抽出
    int queryIdx = cleanSrc.indexOf('?');
    if (queryIdx >= 0) {
        cleanSrc = cleanSrc.left(queryIdx);
    }
    int hashIdx = cleanSrc.indexOf('#');
    if (hashIdx >= 0) {
        cleanSrc = cleanSrc.left(hashIdx);
    }

    QFileInfo fileInfo(cleanSrc);
    QString ext = fileInfo.suffix().toLower();

    static const QSet<QString> allowedExtensions = { "png", "jpg", "jpeg", "gif" };
    return allowedExtensions.contains(ext);
}
