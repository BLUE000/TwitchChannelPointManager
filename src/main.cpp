#include <QApplication>
#include "core/Application.hpp"
#include "gui/MainWindow.hpp"
#include "core/Logger.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    Application app;
    
    // コアシステムの初期化
    if (!app.initialize("TwitchOverlay.db", "config.json")) {
        std::cerr << "Fatal error: Failed to initialize C++ core services!" << std::endl;
        return -1;
    }

    // GUI メインウィンドウのインスタンス化と表示
    MainWindow w(&app);
    w.show();

    LOG_INFO("Twitch Overlay System and GUI successfully launched.");

    // Qt GUI イベントループの開始
    return a.exec();
}
