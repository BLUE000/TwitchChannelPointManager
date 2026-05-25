cmake_minimum_required(VERSION 3.16)
set(APP_VERSION_STRING "Ver2.0.1")
project(TwitchChannelPointManager VERSION 2.0.1 LANGUAGES CXX)

# ==========================================
# ローカル認証情報のインクルード（Git管理外）
# ==========================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/credentials.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/credentials.cmake")
endif()

if(NOT DEFINED TWITCH_GLOBAL_CLIENT_ID)
    set(TWITCH_GLOBAL_CLIENT_ID "MOCK_CLIENT_ID_XYZ123")
endif()

if(NOT DEFINED TWITCH_GLOBAL_CLIENT_SECRET)
    set(TWITCH_GLOBAL_CLIENT_SECRET "MOCK_CLIENT_SECRET_ABC789")
endif()

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
    src/core/ServerCipherEngine.hpp
    src/core/ServerCipherEngine.cpp
    src/core/HTMLSanitizer.hpp
    src/core/HTMLSanitizer.cpp
    
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

# ==========================================
# ビルド時のウォーターマーク（改変検知）機能
# ==========================================
set(BUILD_IS_CUSTOMIZED "0")
find_package(Git)
if(GIT_FOUND)
    # 未コミットのローカル変更があるかチェック
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # 公式タグからのズレ（追加コミットやdirty状態）があるかチェック
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    if(NOT "${GIT_STATUS}" STREQUAL "")
        set(BUILD_IS_CUSTOMIZED "1")
    elseif("${GIT_DESCRIBE}" MATCHES "-dirty" OR "${GIT_DESCRIBE}" MATCHES "-g[0-9a-f]+")
        # -dirtyが含まれる、または -gHASH が含まれる（タグ以降のコミットがある）場合
        set(BUILD_IS_CUSTOMIZED "1")
    endif()
endif()

# ==========================================
# 登録システム名 (system_name) の構築
# ==========================================
# 1. アプリ名 (PROJECT_NAME) からスペースや記号をすべてアンダーバー 「_」 に置換
string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SAFE_APP_NAME "${PROJECT_NAME}")

# 2. 文字数制限（最大50文字）を超える場合は、入るところまで切り詰める
string(LENGTH "${SAFE_APP_NAME}" APP_NAME_LEN)
if (APP_NAME_LEN GREATER 50)
    string(SUBSTRING "${SAFE_APP_NAME}" 0 50 TRANSCIPHER_SYSTEM_NAME)
else()
    set(TRANSCIPHER_SYSTEM_NAME "${SAFE_APP_NAME}")
endif()

message(STATUS "TransCipher System Name: ${TRANSCIPHER_SYSTEM_NAME}")

# ==========================================
# PHP Web APIからの自動トークン取得
# ==========================================
set(TRANSCIPHER_TOKEN_ISSUER_URL "https://streamers-tool.sakura.ne.jp/TransCipher/index.php" CACHE STRING "URL for automated token issuance")
set(TRANSCIPHER_BUILD_SECRET "BLUE000_BUILD_SECRET_KEY" CACHE STRING "Secret password for the build system to request tokens")

set(TRANSCIPHER_API_TOKEN "UNAUTHORIZED_TOKEN")

# JSON文字列の構築
set(CURL_JSON_PAYLOAD "{\"action\":\"generate\",\"system_name\":\"${TRANSCIPHER_SYSTEM_NAME}\",\"build_secret\":\"${TRANSCIPHER_BUILD_SECRET}\",\"build_hash\":\"${GIT_DESCRIBE}\",\"is_official\":")

if (BUILD_IS_CUSTOMIZED EQUAL 0)
    message(STATUS "Official Build: Requesting standard API token from ${TRANSCIPHER_TOKEN_ISSUER_URL}...")
    string(APPEND CURL_JSON_PAYLOAD "true}")
else()
    message(STATUS "Customized Build: Requesting restricted API token from ${TRANSCIPHER_TOKEN_ISSUER_URL}...")
    string(APPEND CURL_JSON_PAYLOAD "false}")
endif()

execute_process(
    COMMAND curl -s -X POST 
            -H "Content-Type: application/json"
            -d "${CURL_JSON_PAYLOAD}"
            ${TRANSCIPHER_TOKEN_ISSUER_URL}
    OUTPUT_VARIABLE FETCHED_JSON
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE CURL_RESULT
)

# 返却されたJSONから jq なしで簡易的にトークンを抽出する (statusがsuccessの場合のみ)
if (CURL_RESULT EQUAL 0 AND FETCHED_JSON MATCHES "\"status\"[ \t\r\n]*:[ \t\r\n]*\"success\"" AND FETCHED_JSON MATCHES "\"token\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"")
    set(TRANSCIPHER_API_TOKEN "${CMAKE_MATCH_1}")
    message(STATUS "Successfully retrieved TransCipher API Token: ${TRANSCIPHER_API_TOKEN}")
else()
    message(WARNING "Failed to retrieve API Token. App may not function correctly. Response: ${FETCHED_JSON}")
endif()

# 実行ファイルターゲットの定義
add_executable(TwitchChannelPointManager ${SOURCES})

# コンパイル定義（ビルド時注入）
target_compile_definitions(TwitchChannelPointManager PRIVATE
    TWITCH_GLOBAL_CLIENT_ID="${TWITCH_GLOBAL_CLIENT_ID}"
    TWITCH_GLOBAL_CLIENT_SECRET="${TWITCH_GLOBAL_CLIENT_SECRET}"
    APP_VERSION_STRING="${APP_VERSION_STRING}"
    BUILD_IS_CUSTOMIZED=${BUILD_IS_CUSTOMIZED}
    TRANSCIPHER_API_TOKEN="${TRANSCIPHER_API_TOKEN}"
)

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
)

# ==========================================
# データベース ビューアー (スタンドアロン版) ターゲットの定義
# ==========================================
set(DB_VIEWER_SOURCES
    src/db_viewer_main.cpp
    src/gui/standalone/DbViewerWindow.hpp
    src/gui/standalone/DbViewerWindow.cpp
    src/core/Logger.hpp
    src/core/Logger.cpp
    src/database/Database.hpp
    src/database/Database.cpp
)

add_executable(TwitchDbViewer ${DB_VIEWER_SOURCES})

target_compile_definitions(TwitchDbViewer PRIVATE
    APP_VERSION_STRING="${APP_VERSION_STRING}"
)

target_include_directories(TwitchDbViewer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(TwitchDbViewer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
)


# ==========================================
# Google Test (GTest) 自動ビルド設定
# ==========================================
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# MSVC/MinGWランタイム競合の防止設定
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# テストに必要な追加 Qt6 コンポーネント (Test) のロード
find_package(Qt6 COMPONENTS Test REQUIRED)

set(TEST_SOURCES
    src/test/test_main.cpp
    src/test/MockNetworkAccessManager.hpp
    
    # Core
    src/core/Config.hpp
    src/core/Config.cpp
    src/core/Logger.hpp
    src/core/Logger.cpp
    src/core/ServerCipherEngine.hpp
    src/core/ServerCipherEngine.cpp
    src/core/HTMLSanitizer.hpp
    src/core/HTMLSanitizer.cpp
    
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
)

add_executable(TwitchChannelPointTest ${TEST_SOURCES})

# コンパイル定義（ビルド時注入）
target_compile_definitions(TwitchChannelPointTest PRIVATE
    TWITCH_GLOBAL_CLIENT_ID="${TWITCH_GLOBAL_CLIENT_ID}"
    TWITCH_GLOBAL_CLIENT_SECRET="${TWITCH_GLOBAL_CLIENT_SECRET}"
    TRANSCIPHER_API_TOKEN="${TRANSCIPHER_API_TOKEN}"
)

target_include_directories(TwitchChannelPointTest PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/TransCipher-Dist/include
)

target_link_libraries(TwitchChannelPointTest PRIVATE
    gtest
    gtest_main
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::HttpServer
    Qt6::Sql
    Qt6::Test
)

# ==========================================
# ビルド出力ディレクトリへの custom_html フォルダ自動コピー/作成
# ==========================================
add_custom_command(
    TARGET TwitchChannelPointManager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:TwitchChannelPointManager>/custom_html
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            ${CMAKE_CURRENT_SOURCE_DIR}/custom_html/README.txt 
            $<TARGET_FILE_DIR:TwitchChannelPointManager>/custom_html/README.txt
    COMMENT "Creating custom_html assets directory and copying README.txt..."
)

# ==========================================
# ビルド出力ディレクトリへの ranking フォルダ自動コピー/作成
# ==========================================
add_custom_command(
    TARGET TwitchChannelPointManager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:TwitchChannelPointManager>/ranking
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/ranking/README.txt
            $<TARGET_FILE_DIR:TwitchChannelPointManager>/ranking/README.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/ranking/default.html
            $<TARGET_FILE_DIR:TwitchChannelPointManager>/ranking/default.html
    COMMENT "Creating ranking overlay directory and copying default files..."
)

