#include <QApplication>
#include "gui/standalone/DbViewerWindow.hpp"
#include "core/Logger.hpp"
#include <iostream>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // データベースファイルの検索：実行ファイルと同じフォルダ内の TwitchOverlay.db
    QString dbPath = "TwitchOverlay.db";
    
    // もしカレントディレクトリに存在せず、実行ファイルディレクトリにあればそこを使う
    if (!QFile::exists(dbPath)) {
        QString appDirDb = QDir(QCoreApplication::applicationDirPath()).filePath("TwitchOverlay.db");
        if (QFile::exists(appDirDb)) {
            dbPath = appDirDb;
        }
    }

    DbViewerWindow w;
    if (!w.initializeDb(dbPath)) {
        std::cerr << "Fatal error: Failed to open TwitchOverlay.db!" << std::endl;
        return -1;
    }

    w.show();

    return a.exec();
}
