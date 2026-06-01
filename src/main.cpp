#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include "core/Application.hpp"
#include "core/Config.hpp"
#include "gui/MainWindow.hpp"
#include "core/Logger.hpp"
#include <iostream>

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

    Application app;
    
    // コアシステムの初期化
    if (!app.initialize("TwitchOverlay.db", "config.json")) {
        std::cerr << "Fatal error: Failed to initialize C++ core services!" << std::endl;
        return -1;
    }

    // 開発環境であれば翻訳レビュー用CSVを自動生成・更新する
    TranslationReviewer::generateReviewCsv("translations", "translation_review.csv");

    // GUI メインウィンドウのインスタンス化と表示
    MainWindow w(&app);
    w.show();

    LOG_INFO("Twitch Overlay System and GUI successfully launched.");

    // Qt GUI イベントループの開始
    return a.exec();
}
