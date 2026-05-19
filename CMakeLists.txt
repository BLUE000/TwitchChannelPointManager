cmake_minimum_required(VERSION 3.16)
project(TwitchChannelPointManager VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自動コンパイル設定 (MOC, UIC, RCC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6 パッケージの検索
find_package(Qt6 COMPONENTS 
    Core 
    Widgets 
    Network 
    WebSockets 
    HttpServer 
    Sql 
    REQUIRED
)

# ソースファイルのリスト化
set(SOURCES
    src/main.cpp
    
    # Core
    src/core/Application.hpp
    src/core/Application.cpp
    src/core/Config.hpp
    src/core/Config.cpp
    src/core/Logger.hpp
    src/core/Logger.cpp
    
    # Database
    src/database/Database.hpp
    src/database/Database.cpp
    
    # Twitch
    src/twitch/TwitchAuth.hpp
    src/twitch/TwitchAuth.cpp
    src/twitch/TwitchEventSub.hpp
    src/twitch/TwitchEventSub.cpp
    
    # Reward
    src/reward/Reward.hpp
    src/reward/QueueManager.hpp
    src/reward/QueueManager.cpp
    src/reward/RewardManager.hpp
    src/reward/RewardManager.cpp
    
    # Overlay
    src/overlay/FileManager.hpp
    src/overlay/FileManager.cpp
    src/overlay/OverlayServer.hpp
    src/overlay/OverlayServer.cpp
    
    # GUI
    src/gui/MainWindow.hpp
    src/gui/MainWindow.cpp
    src/gui/DashboardWidget.hpp
    src/gui/DashboardWidget.cpp
    src/gui/RewardEditorWidget.hpp
    src/gui/RewardEditorWidget.cpp
    src/gui/StatisticsWidget.hpp
    src/gui/StatisticsWidget.cpp
    src/gui/SettingsWidget.hpp
    src/gui/SettingsWidget.cpp
)

# 実行ファイルターゲットの定義
add_executable(TwitchChannelPointManager ${SOURCES})

# インクルードディレクトリの追加
target_include_directories(TwitchChannelPointManager PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/TransCipher-Dist/include
)

# リンク先ライブラリの指定
target_link_libraries(TwitchChannelPointManager PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::HttpServer
    Qt6::Sql
    # TransCipher-Dist リンカインポートライブラリへの直接リンク
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/TransCipher-Dist/lib/libTransCipher.dll.a
)

# ビルド後に DLL を実行ファイルと同ディレクトリへ自動コピーする設定（Windows環境向け）
if(WIN32)
    add_custom_command(TARGET TwitchChannelPointManager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/TransCipher-Dist/lib/libTransCipher.dll"
        $<TARGET_FILE_DIR:TwitchChannelPointManager>
    )
endif()
