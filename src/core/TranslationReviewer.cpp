#include "TranslationReviewer.hpp"
#include "Logger.hpp"
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>
#include <QMap>
#include <QTextStream>

// CSVエスケープ処理用ヘルパー
static QString escapeCsvField(const QString& text)
{
    QString escaped = text;
    // ダブルクォーテーションを二重化
    escaped.replace("\"", "\"\"");
    // カンマ、ダブルクォーテーション、または改行が含まれる場合は全体をダブルクォーテーションで囲む
    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n') || escaped.contains('\r')) {
        return "\"" + escaped + "\"";
    }
    return escaped;
}

bool TranslationReviewer::generateReviewCsv(const QString& translationsDir, const QString& outputPath)
{
    QDir dir(translationsDir);
    if (!dir.exists()) {
        // 開発環境以外（配布先など）では translations ディレクトリが存在しないため、静かに無視する
        return false;
    }

    LOG_INFO(QString("TranslationReviewer: Generating review CSV from directory %1").arg(translationsDir).toStdString().c_str());

    // 解析対象の言語コードと対応するCSV列の並び順（昇順）
    // de, en, es, fr, pt
    QStringList langCodes = {"de", "en", "es", "fr", "pt"};

    // 日本語 (source) -> { 言語コード -> 翻訳文 }
    QMap<QString, QMap<QString, QString>> translationsMap;

    // 各言語の TS ファイルをパース
    for (const QString& lang : langCodes) {
        QString tsFileName = QString("app_%1.ts").arg(lang);
        QString tsFilePath = dir.filePath(tsFileName);

        if (!QFile::exists(tsFilePath)) {
            LOG_WARN(QString("Translation file not found: %1").arg(tsFilePath).toStdString().c_str());
            continue;
        }

        QFile file(tsFilePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            LOG_WARN(QString("Failed to open translation file: %1").arg(tsFilePath).toStdString().c_str());
            continue;
        }

        QXmlStreamReader xml(&file);
        QString currentSource;
        QString currentTranslation;

        while (!xml.atEnd() && !xml.hasError()) {
            QXmlStreamReader::TokenType token = xml.readNext();

            if (token == QXmlStreamReader::StartElement) {
                if (xml.name() == QLatin1String("source")) {
                    currentSource = xml.readElementText();
                } else if (xml.name() == QLatin1String("translation")) {
                    // type="unfinished" 等の属性があっても中身を取得する
                    currentTranslation = xml.readElementText();
                }
            } else if (token == QXmlStreamReader::EndElement) {
                if (xml.name() == QLatin1String("message")) {
                    if (!currentSource.isEmpty()) {
                        // マップに登録（空の翻訳テキストもそのまま登録）
                        translationsMap[currentSource][lang] = currentTranslation;
                    }
                    currentSource.clear();
                    currentTranslation.clear();
                }
            }
        }

        if (xml.hasError()) {
            LOG_ERROR(QString("XML Parse Error in %1: %2").arg(tsFileName).arg(xml.errorString()).toStdString().c_str());
        }

        file.close();
    }

    // CSV ファイルへの書き込み
    QFile csvFile(outputPath);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QString("Failed to open output CSV file for writing: %1").arg(outputPath).toStdString().c_str());
        return false;
    }

    // Excelの文字化けを防ぐため、UTF-8 BOMを書き出す
    csvFile.write("\xEF\xBB\xBF");

    QTextStream out(&csvFile);
    out.setEncoding(QStringConverter::Utf8);

    // CSV ヘッダー出力 (日本語, ドイツ語, 英語, スペイン語, フランス語, ポルトガル語)
    // 昇順に則り、de, en, es, fr, pt の順に並べる
    out << escapeCsvField("日本語 (Original)") << ","
        << escapeCsvField("ドイツ語 (German)") << ","
        << escapeCsvField("英語 (English)") << ","
        << escapeCsvField("スペイン語 (Spanish)") << ","
        << escapeCsvField("フランス語 (French)") << ","
        << escapeCsvField("ポルトガル語 (Portuguese)") << "\n";

    // データ行の出力
    for (auto it = translationsMap.begin(); it != translationsMap.end(); ++it) {
        const QString& sourceText = it.key();
        const QMap<QString, QString>& langMap = it.value();

        out << escapeCsvField(sourceText);

        // 各言語の翻訳文を順番に出力
        for (const QString& lang : langCodes) {
            QString translation = langMap.value(lang, "");
            out << "," << escapeCsvField(translation);
        }
        out << "\n";
    }

    csvFile.close();
    LOG_INFO(QString("TranslationReviewer: Successfully generated %1 with %2 items.").arg(outputPath).arg(translationsMap.size()).toStdString().c_str());
    return true;
}
