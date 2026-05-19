1. システム構成
1.1 全体アーキテクチャ
┌─────────────────────────────────────────────────┐
│                  Twitch                         │
│  (OAuth認証 / EventSub / チャンネルポイント)      │
└─────────────────┬───────────────────────────────┘
                  │ HTTPS
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
                  │ WebSocket (localhost)
                  │
┌─────────────────▼───────────────────────────────┐
│              OBS ブラウザソース                   │
│         (HTML/CSS/JavaScript)                   │
│      http://localhost:PORT/overlay              │
└─────────────────────────────────────────────────┘

2. モジュール構成
2.1 主要モジュール構成図
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

2.2 ディレクトリ構成
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
5. 通信プロトコル
5.1 WebSocket通信 (アプリ ⇔ OBSブラウザソース)
演出表示メッセージ

{
  "type": "show_effect",
  "data": {
    "queueId": "queue_12345",
    "effect": {
      "type": "image",
      "filePath": "file:///path/to/image.png",
      "duration": 5,
      "position": {...},
      "animation": "fade",
      "volume": 80
    }
  }
}

演出完了レスポンス
{
  "type": "effect_completed",
  "data": {
    "queueId": "queue_12345"
  }
}

パニックボタン
{
  "type": "stop_all"
}

キュー全クリア
{
  "type": "clear_queue"
}

6. 処理フロー
6.1 チャンネルポイント使用時
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

7. 画面構成
7.1 メインウィンドウ
┌────────────────────────────────────────────┐
│  [ダッシュボード] [報酬管理] [統計] [設定]  │
├────────────────────────────────────────────┤
│                                            │
│            (各タブの内容)                   │
│                                            │
│                                            │
│                                            │
└────────────────────────────────────────────┘
7.2 ダッシュボード
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

7.3 報酬編集画面
┌─────────────────────────────────────┐
│ 報酬名: [たぬき投げ          ]       │
│ ポイント: [500]                     │
│ クールタイム: [30] 秒               │
│ ロール: ☑全員 ☐サブスク ☐VIP       │
│                                     │
│ 演出モード: ◉順番再生 ○ランダム     │
│                                     │
│ ┌─ 演出1 ─────────────────────┐   │
│ │ ファイル: [参照...]            │   │
│ │ 位置: [中央▼] X:+10 Y:-20     │   │
│ │ 音量: [80]%                   │   │
│ │ [プレビュー]                  │   │
│ └──────────────────────────────┘   │
│                                     │
│ [+ 演出を追加]                      │
│                                     │
│ [保存] [キャンセル]                 │
└─────────────────────────────────────┘
