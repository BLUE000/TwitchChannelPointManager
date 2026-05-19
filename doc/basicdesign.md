1. システム構成
1.1 全体アーキテクチャ

```text
┌─────────────────────────────────────────────────┐
│                  Twitch                         │
│  (OAuth認証 / EventSub / チャンネルポイント)      │
└─────────────────┬───────────────────────────────┘
                  │ HTTPS / WebSocket (WSS)
                  │
┌─────────────────▼───────────────────────────────┐
│         デスクトップアプリ (Qt + C++20)           │
│                                                 │
│  ┌─────────────┐  ┌──────────────┐             │
│  │ GUI Layer   │  │ WebSocket    │             │
│  │ (Qt Widgets)│  │ Server       │             │
│  └──────┬──────┘  └──────┬───────┘             │
│         │                │                      │
│  ┌──────▼────────────────▼───────┐             │
│  │   Business Logic Layer        │             │
│  │ (報酬管理/キュー管理/統計)      │             │
│  └──────┬────────────────────────┘             │
│         │                                       │
│  ┌──────▼────────┐  ┌──────────┐              │
│  │ Data Layer    │  │ File     │              │
│  │ (SQLite)      │  │ Manager  │              │
│  └───────────────┘  └──────────┘              │
└─────────────────┬───────────────────────────────┘
                  │ WebSocket & HTTP (localhost)
                  │
┌─────────────────▼───────────────────────────────┐
│              OBS ブラウザソース                   │
│         (HTML/CSS/JavaScript)                   │
│      http://localhost:PORT/overlay              │
└─────────────────────────────────────────────────┘
```

2. モジュール構成
2.1 主要モジュール構成図

```text
┌─────────────────────────────────────────────────┐
│                  Application                    │
└───┬─────────────────────────────────────────┬───┘
    │                                         │
┌───▼──────────┐                      ┌───────▼────┐
│  GUI Layer   │                      │  Core      │
├──────────────┤                      ├────────────┤
│ MainWindow   │                      │ Config     │
│ Dashboard    │                      │ Logger     │
│ RewardEditor │                      └────────────┘
│ Statistics   │
│ Settings     │
└───┬──────────┘
    │
┌───▼──────────────────────────────────────────────┐
│            Business Logic Layer                  │
├──────────────┬──────────────┬────────────────────┤
│ Twitch       │ Reward       │ Statistics         │
│ ├─Auth       │ ├─Manager    │ ├─Manager          │
│ ├─EventSub   │ ├─Queue      │ └─UsageLog         │
│ └─API        │ └─Effect     │                    │
└──────┬───────┴──────┬───────┴──────┬─────────────┘
       │              │              │
┌──────▼──────┐  ┌───▼──────┐  ┌────▼─────────┐
│ Overlay     │  │ Database │  │ File Manager │
│ ├─Server    │  │ (SQLite) │  │              │
│ └─Renderer  │  └──────────┘  └──────────────┘
└─────────────┘
```

2.2 ディレクトリ構成

```text
src/
├── main.cpp
├── core/                          # コア機能
│   ├── Application                # アプリケーション本体
│   ├── Config                     # 設定管理
│   └── Logger                     # ログ出力
├── twitch/                        # Twitch連携
│   ├── TwitchAuth                 # OAuth認証
│   ├── TwitchEventSub             # EventSub接続
│   └── TwitchAPI                  # API呼び出し
├── reward/                        # 報酬・演出管理
│   ├── RewardManager              # 報酬管理
│   ├── Reward                     # 報酬データ構造
│   ├── Effect                     # 演出データ構造
│   └── QueueManager               # キュー管理
├── overlay/                       # オーバーレイ
│   ├── OverlayServer              # WebSocketサーバー
│   ├── EffectRenderer             # 演出制御
│   └── resources/                 # HTML/CSS/JS
├── statistics/                    # 統計
│   ├── StatisticsManager          # 統計管理
│   └── UsageLog                   # 使用履歴
├── database/                      # データベース
│   ├── Database                   # SQLite接続
│   └── schema.sql                 # DBスキーマ
├── gui/                           # GUI
│   ├── MainWindow                 # メインウィンドウ
│   ├── DashboardWidget            # ダッシュボード
│   ├── RewardEditorWidget         # 報酬編集
│   ├── StatisticsWidget           # 統計画面
│   └── SettingsWidget             # 設定画面
└── utils/                         # ユーティリティ
    ├── FileUtils                  # ファイル操作
    └── NetworkUtils               # ネットワーク補助
```

## 3. データ構造

### 3.1 報酬データ (Reward)

| 項目 | 型 | 説明 |
| :--- | :--- | :--- |
| id | String | 一意ID |
| name | String | 報酬名 |
| cost | Integer | ポイント数 |
| cooldown | Integer | クールタイム(秒) |
| allowedRoles | `List<String>` | 使用可能ロール |
| enabled | Boolean | 有効/無効 |
| mode | Enum | Sequential / Random |
| effects | `List<Effect>` | 演出リスト |

### 3.2 演出データ (Effect)

| 項目 | 型 | 説明 |
| :--- | :--- | :--- |
| type | Enum | Image / Video / Audio / Text |
| filePath | String | ファイルパス |
| duration | Integer | 表示時間(秒) |
| scale | Integer | 表示サイズ(1-100%) |
| position | Object | 表示位置 |
| ├─ preset | String | プリセット名 |
| ├─ offsetX | Integer | X オフセット |
| └─ offsetY | Integer | Y オフセット |
| animation | String | アニメーション種類 |
| audioPath | String | 音声ファイル(オプション) |
| volume | Integer | 音量 (0-100) |
| text | String | テキスト(オプション) |
| probability | Integer | 確率 (ランダムモード時) |

### 3.3 キューアイテム (QueueItem)

| 項目 | 型 | 説明 |
| :--- | :--- | :--- |
| queueId | String | キューID |
| rewardId | String | 報酬ID |
| username | String | 使用ユーザー |
| timestamp | DateTime | 使用日時 |
| effects | `List<Effect>` | 実行する演出リスト |
| currentEffectIndex | Integer | 現在の演出インデックス |

## 4. データベース設計

### 4.1 テーブル構成

#### rewards テーブル

| カラム | 型 | 説明 |
| :--- | :--- | :--- |
| id | TEXT | PRIMARY KEY |
| name | TEXT | 報酬名 |
| cost | INTEGER | ポイント数 |
| cooldown | INTEGER | クールタイム |
| allowed_roles | TEXT | JSON配列 |
| enabled | BOOLEAN | 有効/無効 |
| mode | TEXT | sequential / random |
| effects | TEXT | JSON配列 |

#### usage_logs テーブル

| カラム | 型 | 説明 |
| :--- | :--- | :--- |
| id | INTEGER | PRIMARY KEY |
| reward_id | TEXT | 報酬ID (FK) |
| username | TEXT | ユーザー名 |
| timestamp | DATETIME | 使用日時 |

#### cooldowns テーブル

| カラム | 型 | 説明 |
| :--- | :--- | :--- |
| reward_id | TEXT | 報酬ID |
| username | TEXT | ユーザー名 |
| expires_at | DATETIME | 有効期限 |

## 5. 通信プロトコル

### 5.1 WebSocket通信 (アプリ ⇔ OBSブラウザソース)

* WebSocket: `ws://localhost:28080/overlay`
* Asset Server (HTTP): `http://localhost:28081/assets/`

#### 演出表示メッセージ（修正版）

```json
{
  "type": "show_effect",
  "data": {
    "queueId": "queue_12345",
    "effect": {
      "type": "image",
      "filePath": "http://localhost:28081/assets/abc123.png",
      "audioPath": "http://localhost:28081/assets/sound456.mp3",
      "duration": 5,
      "scale": 100,
      "position": {
        "preset": "center",
        "offsetX": 0,
        "offsetY": 0
      },
      "animation": "fade",
      "volume": 80,
      "text": "たぬきが投げられた！",
      "textStyle": {
        "font": "Arial",
        "size": 32,
        "color": "#FF0000",
        "borderColor": "#FFFFFF",
        "borderWidth": 2
      }
    }
  }
}
```

#### 演出完了レスポンス

```json
{
  "type": "effect_completed",
  "data": {
    "queueId": "queue_12345"
  }
}
```

#### パニックボタン

```json
{
  "type": "stop_all"
}
```

#### キュー全クリア

```json
{
  "type": "clear_queue"
}
```

### 5.2 Asset Server (HTTP)

#### エンドポイント

* ベースURL: `http://localhost:28081`
* アセット配信: `/assets/{filename}`

#### リクエスト例

```bash
GET http://localhost:28081/assets/abc123.png
GET http://localhost:28081/assets/sound456.mp3
GET http://localhost:28081/assets/video789.webm
```

#### レスポンス

* Content-Type: 自動判定（画像/音声/動画）
* CORS: `Access-Control-Allow-Origin: *`（ローカル環境用）
* Cache-Control: `no-cache`（開発時の更新反映のため）

#### ファイル管理

* アプリ内部で実ファイルパスとUUID（またはハッシュ）を紐付け
* `FileManager` がパス変換を担当
  * 内部パス: `/home/user/assets/tanuki.png`
  * 配信URL: `http://localhost:28081/assets/abc123.png`

### 5.3 パス変換フロー

```markdown
1. ユーザーが報酬編集画面でファイル選択
   ↓
2. FileManager が実ファイルパスを保存
   - DB: "/home/user/assets/tanuki.png"
   ↓
3. 演出実行時、QueueManager が FileManager に問い合わせ
   ↓
4. FileManager が UUID を生成（またはキャッシュから取得）
   - UUID: "abc123"
   ↓
5. Asset Server 用の URL に変換
   - "http://localhost:28081/assets/abc123.png"
   ↓
6. WebSocket で OBS に送信
   ↓
7. OBS ブラウザソースが HTTP 経由で安全にロード
```

### 5.4 Overlay Module の構成（修正版）

```cpp
// overlay/OverlayServer.hpp
class OverlayServer {
private:
    QWebSocketServer* wsServer;      // WebSocket サーバー
    QHttpServer* assetServer;        // HTTP アセットサーバー
    FileManager* fileManager;        // ファイル管理

public:
    void startWebSocketServer(int port = 28080);
    void startAssetServer(int port = 28081);
    void sendEffect(const QueueItem& item);
    QString getAssetUrl(const QString& filePath);  // パス変換
};
```

### 5.5 設定項目の追加

#### Settings テーブル（追加項目）

| 項目 | デフォルト値 | 説明 |
| :--- | :--- | :--- |
| `websocket_port` | 28080 | WebSocket ポート |
| `asset_server_port` | 28081 | Asset Server ポート |
| `asset_cache_enabled` | true | アセットキャッシュ有効化 |


## 6. 処理フロー

### 6.1 チャンネルポイント使用時

```markdown
1. Twitch EventSubからイベント受信
   ↓
2. RewardManagerで報酬確認
   ↓
3. 使用可能かチェック
   - 報酬有効性
   - ロール制限
   - クールタイム
   ↓
4. QueueManagerにキュー追加
   ↓
5. 使用履歴をDB記録
   ↓
6. クールタイム設定
   ↓
7. キューから順次取り出し
   ↓
8. 演出モードに応じて選択
   - Sequential: 順番に全て
   - Random: 確率で1つ
   ↓
9. WebSocketでOBS送信
   ↓
10. 演出完了待ち
    ↓
11. 次の演出 or 次のキューへ
```

7. 画面構成
7.1 メインウィンドウ

```text
┌────────────────────────────────────────────┐
│  [ダッシュボード] [報酬管理] [統計] [設定]  │
├────────────────────────────────────────────┤
│                                            │
│            (各タブの内容)                   │
│                                            │
│                                            │
│                                            │
└────────────────────────────────────────────┘
```

7.2 ダッシュボード

```text
┌─────────────────────────────────────┐
│ リアルタイム情報                     │
│ キュー: 3件待ち                      │
│ 今日の使用: 47回                     │
│                                     │
│ [パニックボタン] [キュー全クリア]    │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│ 報酬別ランキング（今日）             │
│ 1. たぬき投げ      25回             │
│ 2. 爆発エフェクト   15回             │
└─────────────────────────────────────┘
```

7.3 報酬演出管理画面 (Left/Right 統合パネル形式)

```text
┌────────────────────────────────────────────────────────────────────────┐
│ 登録済みの報酬一覧:                     報酬情報の設定                  │
│ ┌──────────────────────────────────┐ ┌────────────────────────────────┐ │
│ │ 🟢 [設定済] たぬき投げ (500pt)   │ │ 報酬 ID: [2cc66b0c-8ee9...]    │ │
│ │ ⚪ [未設定] ホゲリクエスト! (100pt)│ │ 報酬名 : [昔あった怖かった話    ]│ │
│ │ 🟡 [オフライン] セーフ (200pt)    │ │ 消費ポイント: [800]            │ │
│ │                                  │ │ クールタイム: [0] 秒           │ │
│ │                                  │ │ 演出再生モード: [順番に再生▼] │ │
│ │                                  │ │ ステータス: ☑ 報酬演出の有効化 │ │
│ │                                  │ ├────────────────────────────────┤ │
│ │                                  │ │ 演出効果（エフェクト）設定     │ │
│ │                                  │ │ 編集対象の演出:                 │ │
│ │                                  │ │ [演出 1: [画像]  ▼] [+演出追加] [❌削除]│
│ │                                  │ │ 演出の種類: [画像のみ (image) ▼] │
│ │                                  │ │ 画像/動画: [path/to/file][参照]│ │
│ │                                  │ │ 効果音  : [path/to/audio][参照]│ │
│ │                                  │ │ 表示時間: [5] 秒               │ │
│ │                                  │ │ 表示サイズ: [100] %            │ │
│ │                                  │ │ 吹き出し: [例: {user}が投ゲ...]│ │
│ └──────────────────────────────────┘ └────────────────────────────────┘ │
│ ┌──────────────┐ ┌───────────────┐ ┌──────────────┐ ┌─────────────────┐ │
│ │ 新規演出登録 │ │  Twitch同期   │ │  設定を保存  │ │ この報酬を削除  │ │
│ └──────────────┘ └───────────────┘ └──────────────┘ └─────────────────┘ │
└────────────────────────────────────────────────────────────────────────┘
```
