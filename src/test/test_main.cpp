#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebSocket>
#include <QThread>

#include "core/Config.hpp"
#include "database/Database.hpp"
#include "reward/RewardManager.hpp"
#include "reward/QueueManager.hpp"
#include "overlay/FileManager.hpp"
#include "overlay/OverlayServer.hpp"
#include "twitch/TwitchAuth.hpp"
#include "test/MockNetworkAccessManager.hpp"

// ==========================================
// 1. Config モジュールのテスト (TC_CFG_*)
// ==========================================
TEST(ConfigTest, LoadSaveBasic) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    Config config(path);
    config.set("test_key", "hello_world");
    config.set("port", 12345);
    ASSERT_TRUE(config.save());

    Config loadConfig(path);
    ASSERT_TRUE(loadConfig.load());
    EXPECT_EQ(loadConfig.get("test_key").toString(), "hello_world");
    EXPECT_EQ(loadConfig.get("port").toInt(), 12345);
}

TEST(ConfigTest, SecureStringEncryption) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    Config config(path);
    QString plainText = "twitch_oauth_token_2026_super_secret";
    QString key = "my_custom_secret_key";

    // 暗号化保存
    ASSERT_TRUE(config.saveSecureString("oauth_token", plainText, key));
    ASSERT_TRUE(config.save());

    // 正常な復号
    Config loadConfig(path);
    ASSERT_TRUE(loadConfig.load());
    QString restored = loadConfig.loadSecureString("oauth_token", key);
    EXPECT_EQ(restored, plainText);

    // 不正なキーによる復号失敗
    QString failedRestored = loadConfig.loadSecureString("oauth_token", "wrong_key");
    EXPECT_TRUE(failedRestored.isEmpty());
}

// ==========================================
// 2. Database モジュールのテスト (TC_DB_*)
// ==========================================
TEST(DatabaseTest, InMemoryTablesCRUD) {
    Database db;
    ASSERT_TRUE(db.open(":memory:"));

    // 1. 報酬の作成と保存
    Reward r;
    r.id = "reward_tanuki";
    r.name = "たぬきを投げる";
    r.cost = 300;
    r.cooldown = 15;
    r.mode = "sequential";
    r.enabled = true;
    r.allowedRoles.append("everyone");

    Effect eff;
    eff.type = "image";
    eff.filePath = "C:/assets/tanuki.png";
    eff.audioPath = "C:/assets/tanuki.mp3";
    eff.duration = 4;
    eff.text = "投げたぬき！";
    r.effects.append(eff);

    ASSERT_TRUE(db.saveReward(r));

    // 2. ロードと復元検証
    QList<Reward> list;
    ASSERT_TRUE(db.loadRewards(list));
    ASSERT_EQ(list.size(), 1);

    Reward loaded = list.first();
    EXPECT_EQ(loaded.id, r.id);
    EXPECT_EQ(loaded.name, r.name);
    EXPECT_EQ(loaded.cost, r.cost);
    EXPECT_EQ(loaded.cooldown, r.cooldown);
    EXPECT_EQ(loaded.mode, r.mode);
    EXPECT_EQ(loaded.enabled, r.enabled);
    ASSERT_EQ(loaded.effects.size(), 1);
    EXPECT_EQ(loaded.effects.first().type, eff.type);
    EXPECT_EQ(loaded.effects.first().filePath, eff.filePath);
    EXPECT_EQ(loaded.effects.first().audioPath, eff.audioPath);
    EXPECT_EQ(loaded.effects.first().duration, eff.duration);
    EXPECT_EQ(loaded.effects.first().text, eff.text);

    // 3. 削除
    ASSERT_TRUE(db.deleteReward("reward_tanuki"));
    QList<Reward> emptyList;
    ASSERT_TRUE(db.loadRewards(emptyList));
    EXPECT_TRUE(emptyList.isEmpty());
}

TEST(DatabaseTest, UsageLoggingAndRanking) {
    Database db;
    ASSERT_TRUE(db.open(":memory:"));

    Reward rA;
    rA.id = "reward_A";
    rA.name = "reward_A";
    rA.enabled = true;
    db.saveReward(rA);

    Reward rB;
    rB.id = "reward_B";
    rB.name = "reward_B";
    rB.enabled = true;
    db.saveReward(rB);

    QDateTime now = QDateTime::currentDateTime();
    ASSERT_TRUE(db.logUsage("reward_A", "user1", now));
    ASSERT_TRUE(db.logUsage("reward_A", "user2", now));
    ASSERT_TRUE(db.logUsage("reward_B", "user1", now));

    int count = db.getTodayUsageCount();
    auto ranking = db.getRanking();
    ASSERT_EQ(ranking.size(), 2);
    EXPECT_EQ(ranking.first().first, "reward_A");
    EXPECT_EQ(ranking.first().second, 2);
    EXPECT_EQ(ranking.last().first, "reward_B");
    EXPECT_EQ(ranking.last().second, 1);
}

// ==========================================
// 3. Reward モジュールのテスト (TC_RWD_*, TC_QUE_*)
// ==========================================
TEST(RewardTest, CooldownAndValidation) {
    Database db;
    ASSERT_TRUE(db.open(":memory:"));

    Reward r;
    r.id = "reward_cooldown";
    r.name = "クールダウンテスト";
    r.cooldown = 10;
    r.enabled = true;
    db.saveReward(r);

    RewardManager rm(&db);
    ASSERT_TRUE(rm.loadAllRewards());

    QString reason;
    // 初回はバリデーション通る
    EXPECT_TRUE(rm.validateRedemption("reward_cooldown", "test_user", reason));

    // クールタイム発動
    rm.triggerCooldown("reward_cooldown");

    // クールダウン中は拒否される
    EXPECT_FALSE(rm.validateRedemption("reward_cooldown", "test_user", reason));
    EXPECT_TRUE(reason.contains("クールダウン中"));
}

TEST(RewardTest, QueuePipelineSequential) {
    Database db;
    ASSERT_TRUE(db.open(":memory:"));

    // 報酬にエフェクトを2個設定
    Reward r;
    r.id = "reward_double";
    r.name = "ダブル演出";
    r.enabled = true;
    r.mode = "sequential";

    Effect eff1;
    eff1.type = "image";
    eff1.filePath = "eff1.png";
    r.effects.append(eff1);

    Effect eff2;
    eff2.type = "sound";
    eff2.audioPath = "eff2.mp3";
    r.effects.append(eff2);

    db.saveReward(r);

    QueueManager qm(&db);
    QSignalSpy playSpy(&qm, &QueueManager::playEffectRequested);

    // 投入
    qm.enqueueRedemption("reward_double", "user1", QDateTime::currentDateTime());

    // 1番目の演出が要求されているか
    ASSERT_EQ(playSpy.count(), 1);
    QList<QVariant> args1 = playSpy.takeFirst();
    QueueItem item = args1.at(0).value<QueueItem>();
    Effect activeEff = args1.at(1).value<Effect>();
    EXPECT_EQ(item.rewardId, "reward_double");
    EXPECT_EQ(activeEff.filePath, "eff1.png");

    // 完了信号を送信 -> 2番目の演出が自動開始されること
    qm.onEffectCompleted(item.queueId);
    ASSERT_EQ(playSpy.count(), 1);
    QList<QVariant> args2 = playSpy.takeFirst();
    Effect activeEff2 = args2.at(1).value<Effect>();
    EXPECT_EQ(activeEff2.audioPath, "eff2.mp3");

    // 全て完了
    qm.onEffectCompleted(item.queueId);
    EXPECT_FALSE(qm.isPlaying());
}

TEST(RewardTest, QueueEmergencyPanicStop) {
    Database db;
    ASSERT_TRUE(db.open(":memory:"));

    Reward r;
    r.id = "reward_panic";
    r.name = "パニック";
    r.enabled = true;
    r.mode = "sequential";
    
    Effect eff;
    eff.type = "image";
    eff.filePath = "eff.png";
    r.effects.append(eff);
    db.saveReward(r);

    QueueManager qm(&db);
    QSignalSpy playSpy(&qm, &QueueManager::playEffectRequested);
    QSignalSpy stopSpy(&qm, &QueueManager::stopAllRequested);

    qm.enqueueRedemption("reward_panic", "user1", QDateTime::currentDateTime());
    ASSERT_TRUE(qm.isPlaying());

    // パニック緊急停止
    qm.stopAllEffects();
    EXPECT_FALSE(qm.isPlaying());
    EXPECT_EQ(stopSpy.count(), 1);
}

// ==========================================
// 4. Overlay モジュールのテスト (TC_OVL_*)
// ==========================================
TEST(OverlayTest, AssetPathHiding) {
    FileManager fm(28081);
    QString realPath = "C:/user/desktop/twitch_manager/assets/tanuki.png";

    // 秘匿アセットURLの生成
    QString assetUrl = fm.registerAsset(realPath);
    EXPECT_TRUE(assetUrl.startsWith("http://localhost:28081/assets/"));
    EXPECT_TRUE(assetUrl.endsWith(".png"));

    // UUIDの取得
    QString filename = assetUrl.section('/', -1);
    
    // 逆引き解決
    QString recovered = fm.getRealPath(filename);
    EXPECT_EQ(recovered, realPath);
}

// ==========================================
// 5. 外部通信スタブとDIのテスト
// ==========================================
TEST(TwitchAuthTest, ExchangeTokenWithMockHttp) {
    MockNetworkAccessManager mockManager;
    QUrl targetUrl("https://id.twitch.tv/oauth2/token");
    QByteArray expectedJson = R"({
        "access_token": "mocked_access_token_12345",
        "refresh_token": "mocked_refresh_token_67890",
        "expires_in": 3600,
        "scope": ["channel:read:redemptions"]
    })";
    mockManager.setExpectedResponse(targetUrl, 200, expectedJson);

    QUrl userUrl("https://api.twitch.tv/helix/users");
    QByteArray expectedUserJson = R"({
        "data": [{
            "id": "12345678",
            "login": "mocked_user",
            "display_name": "MockedUser"
        }]
    })";
    mockManager.setExpectedResponse(userUrl, 200, expectedUserJson);

    TwitchAuth auth("mock_client_id", "mock_client_secret");
    auth.setNetworkAccessManager(&mockManager);

    QSignalSpy successSpy(&auth, &TwitchAuth::authSuccess);
    QSignalSpy failedSpy(&auth, &TwitchAuth::authFailed);

    // トークン交換プロセスの呼び出し
    QMetaObject::invokeMethod(&auth, "exchangeCodeForToken", Q_ARG(QString, "mock_auth_code"));

    // 非同期処理がモックにより即座に完了するの確認
    for (int i = 0; i < 50 && successSpy.isEmpty() && failedSpy.isEmpty(); ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    if (successSpy.isEmpty()) {
        if (!failedSpy.isEmpty()) {
            FAIL() << "Auth failed with message: " << failedSpy.first().at(0).toString().toStdString();
        } else {
            FAIL() << "Auth success signal timed out, and no failed signal was received.";
        }
    }
    ASSERT_EQ(successSpy.count(), 1);

    QList<QVariant> args = successSpy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), "mocked_access_token_12345");
    EXPECT_EQ(args.at(1).toString(), "mocked_refresh_token_67890");
    EXPECT_EQ(args.at(2).toString(), "12345678");
}

TEST(TwitchAuthTest, RefreshAccessTokenSuccess) {
    MockNetworkAccessManager mockManager;
    QUrl tokenUrl("https://id.twitch.tv/oauth2/token");
    QByteArray expectedJson = R"({
        "access_token": "new_mocked_access_token",
        "refresh_token": "new_mocked_refresh_token",
        "expires_in": 14400,
        "scope": ["channel:read:redemptions"]
    })";
    mockManager.setExpectedResponse(tokenUrl, 200, expectedJson);

    TwitchAuth auth("mock_client_id", "mock_client_secret");
    auth.setNetworkAccessManager(&mockManager);

    QSignalSpy successSpy(&auth, &TwitchAuth::authSuccess);
    QSignalSpy failedSpy(&auth, &TwitchAuth::authFailedWithError);

    auth.refreshAccessToken("old_refresh_token", "broadcaster_123");

    for (int i = 0; i < 50 && successSpy.isEmpty() && failedSpy.isEmpty(); ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    ASSERT_EQ(successSpy.count(), 1);
    EXPECT_EQ(failedSpy.count(), 0);

    QList<QVariant> args = successSpy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), "new_mocked_access_token");
    EXPECT_EQ(args.at(1).toString(), "new_mocked_refresh_token");
    EXPECT_EQ(args.at(2).toString(), "broadcaster_123");
}

TEST(TwitchAuthTest, RefreshAccessTokenTemporaryError) {
    MockNetworkAccessManager mockManager;
    QUrl tokenUrl("https://id.twitch.tv/oauth2/token");
    mockManager.setExpectedResponse(tokenUrl, 503, "Service Unavailable");

    TwitchAuth auth("mock_client_id", "mock_client_secret");
    auth.setNetworkAccessManager(&mockManager);

    QSignalSpy successSpy(&auth, &TwitchAuth::authSuccess);
    QSignalSpy failedSpy(&auth, &TwitchAuth::authFailedWithError);

    auth.refreshAccessToken("old_refresh_token", "broadcaster_123");

    for (int i = 0; i < 50 && successSpy.isEmpty() && failedSpy.isEmpty(); ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    EXPECT_EQ(successSpy.count(), 0);
    ASSERT_EQ(failedSpy.count(), 1);

    QList<QVariant> args = failedSpy.takeFirst();
    QString errMsg = args.at(0).toString();
    bool isFatal = args.at(1).toBool();
    
    EXPECT_TRUE(errMsg.contains("再取得に失敗しました"));
    EXPECT_FALSE(isFatal);
}

TEST(TwitchAuthTest, RefreshAccessTokenFatalError) {
    MockNetworkAccessManager mockManager;
    QUrl tokenUrl("https://id.twitch.tv/oauth2/token");
    QByteArray errorJson = R"({
        "error": "Bad Request",
        "status": 400,
        "message": "Invalid client"
    })";
    mockManager.setExpectedResponse(tokenUrl, 400, errorJson);

    TwitchAuth auth("mock_client_id", "mock_client_secret");
    auth.setNetworkAccessManager(&mockManager);

    QSignalSpy successSpy(&auth, &TwitchAuth::authSuccess);
    QSignalSpy failedSpy(&auth, &TwitchAuth::authFailedWithError);

    auth.refreshAccessToken("old_refresh_token", "broadcaster_123");

    for (int i = 0; i < 50 && successSpy.isEmpty() && failedSpy.isEmpty(); ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    EXPECT_EQ(successSpy.count(), 0);
    ASSERT_EQ(failedSpy.count(), 1);

    QList<QVariant> args = failedSpy.takeFirst();
    QString errMsg = args.at(0).toString();
    bool isFatal = args.at(1).toBool();
    
    EXPECT_TRUE(errMsg.contains("再取得に失敗しました"));
    EXPECT_TRUE(isFatal);
}

// ==========================================
// 6. TwitchEventSub 重複排除機能のテスト
// ==========================================
#include "twitch/TwitchEventSub.hpp"

TEST(TwitchEventSubTest, DuplicateMessageDeduplication) {
    TwitchEventSub eventSub;
    QSignalSpy redeemedSpy(&eventSub, &TwitchEventSub::channelPointRedeemed);

    // テスト用のメッセージを作成 (有効なJSONかつ通知メッセージ)
    // message_id が同一のメッセージを2回送り、2回目は無視されることを確認する。
    QString testMessage1 = R"({
        "metadata": {
            "message_id": "test-msg-id-12345",
            "message_type": "notification",
            "subscription_type": "channel.channel_points_custom_reward_redemption.add",
            "subscription_version": "1"
        },
        "payload": {
            "subscription": {
                "type": "channel.channel_points_custom_reward_redemption.add"
            },
            "event": {
                "reward": {
                    "id": "reward-id-abc"
                },
                "user_name": "test_user",
                "redeemed_at": "2026-05-22T07:23:45.000Z"
            }
        }
    })";

    // 1回目：処理されるべき
    QMetaObject::invokeMethod(&eventSub, "onTextMessageReceived", Q_ARG(QString, testMessage1));
    EXPECT_EQ(redeemedSpy.count(), 1);

    // 2回目（同一メッセージID）：無視されるべき
    QMetaObject::invokeMethod(&eventSub, "onTextMessageReceived", Q_ARG(QString, testMessage1));
    EXPECT_EQ(redeemedSpy.count(), 1); // 追加でトリガーされない

    // 3回目（異なるメッセージID）：処理されるべき
    QString testMessage2 = R"({
        "metadata": {
            "message_id": "test-msg-id-67890",
            "message_type": "notification",
            "subscription_type": "channel.channel_points_custom_reward_redemption.add",
            "subscription_version": "1"
        },
        "payload": {
            "subscription": {
                "type": "channel.channel_points_custom_reward_redemption.add"
            },
            "event": {
                "reward": {
                    "id": "reward-id-abc"
                },
                "user_name": "test_user",
                "redeemed_at": "2026-05-22T07:23:45.000Z"
            }
        }
    })";

    QMetaObject::invokeMethod(&eventSub, "onTextMessageReceived", Q_ARG(QString, testMessage2));
    EXPECT_EQ(redeemedSpy.count(), 2);
}

// ==========================================
// 7. HTML/CSS サニタイザーのテスト
// ==========================================
#include "core/HTMLSanitizer.hpp"

TEST(HTMLSanitizerTest, BasicSanitization) {
    // 1. スクリプトの除去
    QString scriptHtml = "<div>Hello <script>alert(1);</script>World</div>";
    QString cleanedScript = HTMLSanitizer::sanitizeHtml(scriptHtml);
    EXPECT_FALSE(cleanedScript.contains("<script>"));
    EXPECT_TRUE(cleanedScript.contains("<div>Hello World</div>"));

    // 2. イベントハンドラの除去
    // サニタイズされると onclick 属性自体が綺麗に取り除かれるか検証
    QString onclickHtml = "<div onclick=\"runMaliciousCode()\" class=\"test\">Click here</div>";
    QString cleanedOnclick = HTMLSanitizer::sanitizeHtml(onclickHtml);
    EXPECT_FALSE(cleanedOnclick.contains("onclick"));
    EXPECT_TRUE(cleanedOnclick.contains("class=\"test\""));

    // 3. ホワイトリスト外タグの除去 (iframe, object等)
    QString tagHtml = "<div><iframe src='http://evil.com'></iframe><p>Allowed</p></div>";
    QString cleanedTag = HTMLSanitizer::sanitizeHtml(tagHtml);
    EXPECT_FALSE(cleanedTag.contains("iframe"));
    EXPECT_TRUE(cleanedTag.contains("<p>Allowed</p>"));

    // 4. 画像のローカル画像制限
    // 外部画像 -> 除去
    QString extImgHtml = "<img src=\"https://evil.com/pic.png\" class=\"avatar\">";
    QString cleanedExtImg = HTMLSanitizer::sanitizeHtml(extImgHtml);
    EXPECT_FALSE(cleanedExtImg.contains("src=\"https://evil.com/pic.png\""));
    // SVG埋め込み -> 除去
    QString svgImgHtml = "<img src=\"data:image/svg+xml;base64,1234\">";
    QString cleanedSvgImg = HTMLSanitizer::sanitizeHtml(svgImgHtml);
    EXPECT_FALSE(cleanedSvgImg.contains("data:image"));
    // ローカル画像 (PNG) -> 許可
    QString localImgHtml = "<img src=\"custom_html/assets/icon.png\" id=\"img1\">";
    QString cleanedLocalImg = HTMLSanitizer::sanitizeHtml(localImgHtml);
    EXPECT_TRUE(cleanedLocalImg.contains("src=\"custom_html/assets/icon.png\""));

    // 5. CSS内の外部参照除去
    QString styleHtml = "<style>@import 'http://evil.com/style.css'; div { background: url('https://evil.com/bg.png'); color: red; }</style>";
    QString cleanedStyle = HTMLSanitizer::sanitizeHtml(styleHtml);
    EXPECT_FALSE(cleanedStyle.contains("@import"));
    EXPECT_FALSE(cleanedStyle.contains("url("));
    EXPECT_TRUE(cleanedStyle.contains("color: red;"));
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

