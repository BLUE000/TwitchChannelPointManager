# テスト仕様書

本仕様書は、Google Test (GTest) および Qt Test ユーティリティを用いて、アプリケーションの主要モジュールを検証するための単体テストおよび統合テストケースを定義します。
画面（GUI）からの要求は、シグナル発火およびモックオブジェクトによって再現し、ヘッドレス（画面非表示）環境でも検証可能な設計とします。

---

## 1. テスト環境・戦略 (Test Strategy)

### 1.1 使用フレームワーク
- **Google Test (GTest)**: ロジック、データ永続化、暗号化処理の検証。
- **Qt Test (QTest/QSignalSpy)**: シグナル・スロットの非同期バインディング、およびイベントループ連動処理の検証。

### 1.2 画面（GUI）からの要求の再現
ウィジェットの直接操作をシミュレートする代わりに、ウィジェットからコアに対して発火する Qt シグナルをテストドライバで直接発火させるか、モッククラスから要求メソッドを呼び出して挙動を検証します。

```text
[単体テストドライバ] ──── (直接シグナル発火/メソッド呼出) ───► [コアモジュール]
        ▲
        │ (QSignalSpyによる検証)
        └─────────────────────────────────────────────── [シグナル検知・アサート]
```

---

## 2. モジュール別テストケース定義

### 2.1 Core モジュール (`core/Config`)

暗号化設定ファイル（`TransCipher-Dist` 依存）の正常動作を検証します。

| テストケースID | テスト対象機能 | テスト内容・手順 | 期待される結果 (アサート) |
| :--- | :--- | :--- | :--- |
| `TC_CFG_001` | 基本設定ロード・保存 | 値をセットし、ファイルに `save()` した後、別のインスタンスで `load()` する。 | 保存したキーと値が完全に一致すること。 |
| `TC_CFG_002` | 暗号化保存 (TransCipher) | `saveSecureString()` にて Twitch アクセストークンを暗号化保存する。 | 保存された JSON 内の値が平文ではなく Base64 難読文字列であること。 |
| `TC_CFG_003` | 復号読み込み (TransCipher) | 保存された暗号トークンを正しいシークレットキーで `loadSecureString()` する。 | 元の平文トークンが完全に復元されること。 |
| `TC_CFG_004` | 復号失敗チェック | 間違ったシークレットキーを用いて `loadSecureString()` する。 | 復号エラーが発生し、デフォルト値（または空）が返却されること。 |
| `TC_CFG_005` | 運行ログクランプ表示 | `Logger::instance()->log()` でログメッセージを連続350回発行し、`DashboardWidget::m_logListWidget` の行数を検証する。 | 表示行数が最大 `300` に制限され、最古の50件のUIアイテムがメモリから安全に破棄されていること。 |

---

### 2.2 Database モジュール (`database/Database`)

SQLite を使用したデータ永続化、ログ履歴、マイグレーションを検証します。メモリDB（`:memory:`）を使用することで、ファイル汚染を防ぎます。

| テストケースID | テスト対象機能 | テスト内容・手順 | 期待される結果 (アサート) |
| :--- | :--- | :--- | :--- |
| `TC_DB_001` | テーブル自動生成 | メモリDBをオープンする。 | テーブル `rewards`, `usage_logs`, `settings` が自動生成され存在すること。 |
| `TC_DB_002` | 報酬データの保存・読込 (CRUD) | `Reward` 構造体（複数エフェクト含む）を作成し `saveReward()` し、`loadRewards()` で復元。 | 復元された報酬ID、名前、エフェクト定義、JSON配列データが完全に一致すること。 |
| `TC_DB_003` | ログ記録と統計 | 同一の報酬に対して `logUsage()` を3回実行し、`getTodayUsageCount()` を呼び出す。 | カウントが正確に `3` となること。 |
| `TC_DB_004` | 本日のランキング集計 | 報酬Aに3回、報酬Bに1回ログを記録し、`getTodayRanking()` を取得する。 | リストの先頭が報酬A（回数3）、2番目が報酬B（回数1）となること。 |
| `TC_DB_005` | ログクリーンアップ (DbViewer) | `usage_logs` にダミーデータを複数挿入後、特定の期間を指定して削除（DELETE）し、`VACUUM;` を実行する。 | 対象期間外の最新のログは保護され、かつデータベースファイルの物理サイズが縮小されること。 |

---

### 2.3 Reward マジュール (`reward/RewardManager` & `QueueManager`)

クールタイム、バリデーション、および非同期再生キューを検証します。GUIからの保存・削除要求は API 直接呼び出しで模擬します。

| テストケースID | テスト対象機能 | テスト内容・手順 | 期待される結果 (アサート) |
| :--- | :--- | :--- | :--- |
| `TC_RWD_001` | 有効性チェック | 無効化された報酬ID（`enabled = false`）を `validateRedemption()` でチェック。 | 戻り値が `false` であり、拒否理由が得られること。 |
| `TC_RWD_002` | クールダウン処理 | 演出発生時に `triggerCooldown()` を呼び出し、直後に再度 `validateRedemption()` を実行。 | クールダウン中のため `false` が返り、残り秒数が判定文字列に含まれること。 |
| `TC_QUE_001` | キュー制御 (Sequential) | 2つの演出を含むアイテムを `enqueueRedemption()` に投入。 | `playEffectRequested()` が1番目の演出パラメータで発火すること。 |
| `TC_QUE_002` | キュー遷移確認 (非同期) | 1番目の演出に対してモックOBS完了信号 `onEffectCompleted()` をスロットへ送信。 | 次の演出が自動的に取り出され、2番目の `playEffectRequested()` が発火すること。 |
| `TC_QUE_003` | パニック停止 (緊急割込) | 演出再生中に `stopAllEffects()` を実行。 | キューが瞬時に空となり、`isPlaying` が `false` になり、`stopAllRequested()` が発火すること。 |

---

### 2.4 Overlay モジュール (`overlay/FileManager` & `OverlayServer`)

パス暗号化変換と WebSocket 配信を検証します。

| テストケースID | テスト対象機能 | テスト内容・手順 | 期待される結果 (アサート) |
| :--- | :--- | :--- | :--- |
| `TC_OVL_001` | パス UUID 秘匿変換 | 絶対パス `/home/user/pic.png` を `registerAsset()` する。 | `http://localhost:28081/assets/{UUID}.png` 形式に変換され、実パスが隠蔽されること。 |
| `TC_OVL_002` | アセット逆引き | 変換されたアセットIDから `getRealPath()` で解決を図る。 | 元の絶対パス `/home/user/pic.png` が正確に復元されること。 |
| `TC_OVL_003` | WebSocket配信（結合） | モックの `QWebSocket` クライアントを接続し、`sendEffect()` を実行。 | クライアントが受信したテキストメッセージが、正しい JSON 構造の `show_effect` であること。 |
| `TC_OVL_004` | アセットのストリーム配信 | 大容量テストファイルを用意して `/assets/<arg>` からストリーム方式で要求し、ダウンロードされるデータを検証する。 | 大容量展開によるメモリの急激なスパイクが発生せず、かつ取得されたデータに一切の破損がないこと。 |

---

### 2.5 HTML/CSS サニタイザーモジュール (`core/HTMLSanitizer`)

配信HTMLに含まれる危険な要素を超厳格に排除（サニタイズ）し、ゼロトラスト静的表示を保証します。

| テストケースID | テスト対象機能 | テスト内容・手順 | 期待される結果 (アサート) |
| :--- | :--- | :--- | :--- |
| `TC_SAN_001` | JavaScriptコード除去 | `<script>alert(1);</script>` を含むHTMLをサニタイズする。 | `<script>` タグおよびその中身のJSコードが丸ごと完全消去されること。 |
| `TC_SAN_002` | イベントハンドラ除去 | イベント属性 `<div onclick="run()">` を含むHTMLをサニタイズする。 | `onclick` などの `on` で始まるイベント属性のみが綺麗に除去されること。 |
| `TC_SAN_003` | 非ホワイトリストタグ除去 | `<iframe src="evil.com">` などのタグを含むHTMLをサニタイズする。 | ホワイトリスト以外のタグ（iframe, object等）がタグごと消去され、`div`, `span`, `p` などのみ残ること。 |
| `TC_SAN_004` | 画像ローカル制限 | `<img>` タグの `src` に外部URLやSVGデータスキーム、ローカル画像を指定する。 | 外部接続やSVGデータは除去され、ローカルの `png`/`jpg`/`jpeg`/`gif` のみが許可されること。 |
| `TC_SAN_005` | CSS外部接続遮断 | `<style>` 内に `@import` や `url(...)` 記述を含めてサニタイズする。 | `@import` や `url(...)` などの外部接続コードのみが除去され、通常のスタイル（`color: red;` 等）は残ること。 |

---

### 2.6 Twitch 連携モジュール (`twitch/TwitchAuth`)

OAuth トークンのバックグラウンド自動ライフサイクル（定期的リフレッシュおよび一時的/恒久的エラー判定）を検証します。

| テストケースID | テスト対象機能 | テスト内容・手順 | 期待される結果 (アサート) |
| :--- | :--- | :--- | :--- |
| `TC_ATH_001` | 定期的トークン自動リフレッシュ | `Application` 起動後、リフレッシュ成功のモックを介して2時間の定期的リフレッシュタイマーが発火するのをシミュレートする。 | `refreshAccessToken()` が呼び出され、新しいアクセストークンが設定ファイルにセキュア保存され、EventSub接続が維持されること。 |
| `TC_ATH_002` | 一時的エラー時の自動リトライ | `refreshAccessToken()` 実行時、一時的ネットワーク不通（5xxエラーやタイムアウト）のレスポンスをモックで返却する。 | `isFatal=false` として検知され、警告ダイアログ等は非表示でログにのみ記録され、5分後にリトライタイマーがスケジュールされること。また、5回連続で失敗した場合はそれ以上のリトライが自動停止されること。 |
| `TC_ATH_003` | 恒久的エラー時の検知と停止 | `refreshAccessToken()` 実行時、認証情報不正（400/401エラーやリフレッシュトークン無効）のレスポンスをモックで返却する。 | `isFatal=true` として検知され、定期/リトライタイマーが即座に停止し、UI上に警告表示（再連携の促し）が通知されること。 |

---

## 3. シグナルとスロットによる結合テスト構造

テストドライバにおいて、以下のように実クラス間でシグナルとスロットを接続し、QSignalSpyを用いてフローを追跡します。

```cpp
// 結合テストコード例 (Google Test + Qt Test)
TEST_F(OverlayIntegrationTest, TestFullSignalFlow) {
    Database db;
    db.open(":memory:");
    
    // テスト用の報酬データを用意して登録
    Reward r;
    r.id = "reward_tanuki";
    r.name = "たぬき投げ";
    r.enabled = true;
    r.cooldown = 10;
    
    Effect eff;
    eff.type = "image";
    eff.filePath = "C:/assets/tanuki.png";
    r.effects.append(eff);
    db.saveReward(r);

    RewardManager rewardManager(&db);
    rewardManager.loadAllRewards();

    QueueManager queueManager(&db);
    FileManager fileManager(28081);
    OverlayServer overlayServer(&fileManager);

    // シグナル・スロット結合
    QObject::connect(&queueManager, &QueueManager::playEffectRequested,
                     &overlayServer, &OverlayServer::sendEffect);

    // シグナルのスパイを設定
    QSignalSpy playSpy(&queueManager, &QueueManager::playEffectRequested);
    QSignalSpy serverSpy(&overlayServer, &OverlayServer::effectFinished);

    // 1. GUIからの要求（EventSub経由のトリガーを模擬）
    QString reason;
    ASSERT_TRUE(rewardManager.validateRedemption("reward_tanuki", "ViewerA", reason));
    rewardManager.triggerCooldown("reward_tanuki");
    queueManager.enqueueRedemption("reward_tanuki", "ViewerA", QDateTime::currentDateTime());

    // playEffectRequested シグナルが正しく発火されたかアサート
    ASSERT_EQ(playSpy.count(), 1);
    
    // 送信された情報を取り出して検証
    QList<QVariant> arguments = playSpy.takeFirst();
    QueueItem playedItem = arguments.at(0).value<QueueItem>();
    ASSERT_EQ(playedItem.rewardId, "reward_tanuki");
    ASSERT_EQ(playedItem.username, "ViewerA");

    // 2. OBSからの再生完了信号（画面のモック）を受信したと仮定
    overlayServer.onTextMessageReceived(R"({"type": "effect_completed", "data": {"queueId": ")" + playedItem.queueId + R"("}})");

    // キューが正常に進み、完了ログが残ったことを検証
    ASSERT_EQ(db.getTodayUsageCount(), 1);
}
```

---

## 4. 外部通信のスタブ化 (Mocking External Communications)

テスト時にTwitch APIや実Webサーバーと通信すると、ネットワーク環境や認証期限に依存し、テストが不安定（Flaky）になります。そのため、通信箇所をモックオブジェクトに差し替え可能な依存性注入（Dependency Injection）構造を採用しています。

### 4.1 ネットワーク要求のインターセプト (`MockNetworkAccessManager`)
`TwitchAuth` および `TwitchEventSub` には、任意の `QNetworkAccessManager` を外部からセット可能な `setNetworkAccessManager()` を実装しています。テスト時に `MockNetworkAccessManager` を注入することで、任意のURLに対して意図したレスポンスを動的に返却できます。

```cpp
TEST_F(TwitchAuthTest, TestTokenExchangeWithMockHttp) {
    // 1. スタブマネージャーの作成
    MockNetworkAccessManager mockManager;
    
    // 期待するURLと、それに対するステータスコード・JSONレスポンスを事前登録
    QUrl targetUrl("https://id.twitch.tv/oauth2/token");
    QByteArray expectedJson = R"({
        "access_token": "mocked_access_token_12345",
        "refresh_token": "mocked_refresh_token_67890",
        "expires_in": 3600,
        "scope": ["channel:read:redemptions"]
    })";
    mockManager.setExpectedResponse(targetUrl, 200, expectedJson);

    // 2. テスト対象にスタブを注入
    TwitchAuth auth("test_client_id", "test_client_secret");
    auth.setNetworkAccessManager(&mockManager);

    // 3. 非同期シグナルの監視
    QSignalSpy successSpy(&auth, &TwitchAuth::authSuccess);

    // 4. トリガー発火（ローカルサーバーからリダイレクトがあったと仮定してメソッドを直接叩く）
    // 内部で manager->post() が呼ばれるが、モックによって偽装レスポンスが即座に返る
    QMetaObject::invokeMethod(&auth, "exchangeCodeForToken", Q_ARG(QString, "mock_auth_code"));

    // 5. アサート（HTTP通信がスタブされ、即座に成功シグナルが発火すること）
    ASSERT_TRUE(successSpy.wait(2000)); // 非同期完了を最大2秒待機
    ASSERT_EQ(successSpy.count(), 1);

    // 取得したトークンの検証
    QList<QVariant> args = successSpy.takeFirst();
    ASSERT_EQ(args.at(0).toString(), "mocked_access_token_12345");
    ASSERT_EQ(args.at(1).toString(), "mocked_refresh_token_67890");
}
```

### 4.2 WebSocket のスタブ検証（モックサーバー）
`TwitchEventSub` が接続する `wss://eventsub.wss.twitch.tv` の検証では、ローカルホスト上で `QWebSocketServer` をポートを一時指定して立ち上げ、接続先をテスト環境のみ `ws://localhost:PORT` へ切り替えることで、実WebSocket通信を完全にローカルでエミュレートします。
テストサーバーから `session_welcome` メッセージや `notification`（チャンネルポイント消費）JSONメッセージをソケット経由でクライアントにプッシュし、クライアント側が正しくシグナルを発火するかを検証します。

