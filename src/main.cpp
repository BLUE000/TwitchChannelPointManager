#include <QCoreApplication>
#include "core/Application.hpp"
#include "core/Logger.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Application app;
    
    // アプリケーション初期化
    if (!app.initialize("TwitchOverlay.db", "config.json")) {
        std::cerr << "Fatal error: Failed to initialize C++ core services!" << std::endl;
        return -1;
    }

    LOG_INFO("Twitch Overlay System successfully launched. Press Ctrl+C to terminate.");

    // Qtイベントループの開始
    return a.exec();
}
