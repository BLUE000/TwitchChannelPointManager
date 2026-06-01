#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include "gui/standalone/DbViewerWindow.hpp"
#include "core/Config.hpp"
#include "core/Logger.hpp"
#include <iostream>
#include <QDir>

#include "core/TranslationReviewer.hpp"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. 設定ファイルから UI 言語をロード
    Config config("config.json");
    config.load();
    QString lang = config.get("ui_language", "auto").toString();
    if (lang == "auto") {
        lang = QLocale::system().name().left(2);
    }

    QTranslator translator;
    QStringList supportedLangs = {"de", "en", "es", "fr", "pt"};
    if (supportedLangs.contains(lang)) {
        if (translator.load(QString(":/translations/app_") + lang)) {
            a.installTranslator(&translator);
        }
    }

    // データベースファイルの検索：実行ファイルと同じフォルダ内の TwitchOverlay.db
    QString dbPath = "TwitchOverlay.db";
    
    // もしカレントディレクトリに存在せず、実行ファイルディレクトリにあればそこを使う
    if (!QFile::exists(dbPath)) {
        QString appDirDb = QDir(QCoreApplication::applicationDirPath()).filePath("TwitchOverlay.db");
        if (QFile::exists(appDirDb)) {
            dbPath = appDirDb;
        }
    }

    // 開発環境であれば翻訳レビュー用CSVを自動生成・更新する
    // TranslationReviewer::generateReviewCsv("translations", "translation_review.csv");

    DbViewerWindow w;
    if (!w.initializeDb(dbPath)) {
        std::cerr << "Fatal error: Failed to open TwitchOverlay.db!" << std::endl;
        return -1;
    }

    w.show();

    return a.exec();
}
