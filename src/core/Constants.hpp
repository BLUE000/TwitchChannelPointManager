#pragma once

#include <QString>

namespace Constants {
    // ネットワークデフォルトポート
    constexpr int DEFAULT_WEBSOCKET_PORT = 28080;
    constexpr int DEFAULT_ASSET_HTTP_PORT = 28081;
    constexpr int DEFAULT_AUTH_CALLBACK_PORT = 28082;

    // 設定キー名
    inline const QString SETTING_WS_PORT = "websocket_port";
    inline const QString SETTING_HTTP_PORT = "asset_server_port";
    inline const QString SETTING_CLIENT_ID = "twitch_client_id";
    inline const QString SETTING_CLIENT_SECRET = "twitch_client_secret";
    inline const QString SETTING_BROADCASTER_ID = "twitch_broadcaster_id";

    // データベースのカラムインデックス定義
    enum RewardColumn {
        COL_REWARD_ID = 0,
        COL_REWARD_NAME = 1,
        COL_REWARD_COST = 2,
        COL_REWARD_COOLDOWN = 3,
        COL_REWARD_ALLOWED_ROLES = 4,
        COL_REWARD_ENABLED = 5,
        COL_REWARD_MODE = 6,
        COL_REWARD_EFFECTS = 7
    };
}
