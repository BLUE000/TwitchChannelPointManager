# 🦊 Twitch チャンネルポイント演出管理システム (TwitchChannelPointManager)

本システム（TwitchChannelPointManager）をご利用いただきありがとうございます。  
本システムは、視聴者が Twitch のチャンネルポイントを引き換えた際に、OBS（Open Broadcaster Software）上に画像・透過動画・効果音・テキストなどを組み合わせたリッチな演出をリアルタイムで表示し、配信を最高に盛り上げるためのデスクトップ連携アプリケーションです。

---

## 🚀 クイックスタートガイド

配信画面に最初の演出を表示するまでの手順は、わずか **3ステップ** です。

### STEP 1: Twitch 連携（初回のみ）
1. アプリを起動し、上部の「**システム設定**」タブを開きます。
2. 「**Twitch 連携認証**」ボタンをクリックします。
3. ブラウザが自動的に開き、Twitch の認可画面が表示されます。画面の指示に従ってアクセスを許可してください。
4. 「**認証成功**」と表示されれば連携は完了です！

### STEP 2: OBS スタジオへのオーバーレイ追加
1. OBS Studio を起動します。
2. 「ソース」エリアの「**＋**」ボタンをクリックし、「**ブラウザ**」を選択します。
3. 新規作成し、プロパティで以下のように設定します：
   * **URL**: `http://localhost:28081/overlay` （デフォルト設定時）
   * **幅**: `1920` (配信画面の解像度に合わせてください)
   * **高さ**: `1080` (配信画面の解像度に合わせてください)
4. これで、アプリから送られる演出を受信する準備が整いました。

#### 🏆 (オプション) 配信画面へのランキングボード追加
本システムでは、今日最も使われたチャンネルポイントのランキング（リーダーボード）を配信画面に直接表示することも可能です。
1. OBSの「ソース」エリアの「**＋**」ボタンから「**ブラウザ**」を新規追加します。
2. 以下のように設定します：
   * **URL**: `http://localhost:28081/ranking` （デフォルト設定時）
   * **幅**: `360` 
   * **高さ**: `600`
3. 配信画面上の好きな位置に配置してください。本日のポイント演出回数がリアルタイムに自動更新されて表示されます！

### STEP 3: 演出の作成と保存
1. 上部の「**報酬演出管理**」タブを開きます。
2. 下部にある紫色をした「🔄 **Twitch同期**」ボタンをクリックします。あなたの Twitch チャンネルにあるカスタム報酬の一覧が自動取得されます。
3. リストから演出を設定したい報酬（最初は `⚪ [未設定]`）をクリックします。
4. 右側のパネルで、演出したい「画像/動画ファイル」や「効果音ファイル」を選択し、表示時間を設定します。
5. 「**設定を保存**」をクリックすると、ステータスが `🟢 [設定済]` に変わり、演出の登録が完了します！

---

## 🛠️ 画面の機能と使い方

### 報酬演出管理画面（メイン編集）
左側のリストで Twitch 上の報酬を一覧し、右側でそれぞれの演出を設定します。

#### ① リストのステータス（色分け表示）
ユーザーの混乱を防ぐため、報酬の状態はわかりやすく3つに色分けされて表示されます。
* `🟢 [設定済]` (白色文字): ローカルデータベースに演出効果が設定され、稼働状態にある報酬です。
* `⚪ [未設定]` (明灰色文字): Twitch 側には存在しますが、まだ演出が紐付けられていない報酬です。クリックするだけで簡単に設定を開始できます。
* `🟡 [オフライン]` (黄色文字): ローカルには設定が存在しますが、現在 Twitch API から取得できなかった報酬です（Twitch 上で削除された、またはオフライン環境等のケース）。

#### ② 複数演出エフェクト（マルチエフェクト）の編集
1つのポイント引き換えに対して、複数の演出を組み合わせて表示させることができます。
* **追加**: 「**＋ 演出を追加**」ボタンを押すことで、2個目、3個目の演出スロットが追加されます。
* **切り替え**: 「編集対象の演出」ドロップダウンから編集したい演出スロット（演出 1、演出 2...）を切り替えてそれぞれの画像やサウンドを設定します。
* **削除**: 不要な演出スロットは「**❌ 削除**」ボタンで削除できます。
* ⚠️ **安全ガード仕様**: 設定されている演出効果が0個になるのを防ぐため、**最後の1個になった演出は削除できないよう削除ボタンが自動的に無効化**されます。

#### ③ 演出再生モード
* **全ての演出を順番に再生 (sequential)**: 登録された複数の演出が、演出 1 ➔ 演出 2 ➔ 演出 3 の順に連鎖して1つずつ再生されます。
* **演出リストからランダムで1つ再生 (random)**: 視聴者が引き換えるたびに、登録された演出の中からどれか1つが完全にランダムで選ばれて再生されます（「当たり・ハズレ」などのガチャ演出が作れます）。

---

## ⚡ 高度な機能

### 非常停止（パニックボタン）
万が一、不適切な画像・予期せぬ長い音声・荒らし行為などのトラブルが発生した場合、ダッシュボードにある赤い「**緊急停止 (PANIC BUTTON)**」をクリックすると、**OBS上のすべての演出を一瞬で消去し、待機中の演出キューもすべて強制全クリア**します。

### 統計ランキング画面
「**統計**」タブでは、今日最も使われたチャンネルポイントや、よく引き換えてくれる視聴者のランキングをリアルタイムで閲覧できます。

#### 📺 OBSへのランキング出力（オーバーレイ表示）
アプリ上で確認するだけでなく、OBSのブラウザソースとして配信画面に直接オシャレなランキングボード（リーダーボード）をオーバーレイ表示することが可能です。
詳細な登録手順は「クイックスタートガイドのSTEP 2」を、表示カスタマイズに関しては下部の「カスタマイズ機能」セクションをご覧ください。

#### カスタマイズ機能
各統計タブ（ランキング、ユーザ別）には、アプリ画面内での**「背景画像」** と **「文字色」** を変更できる設定ボタンがあります。
*   **背景画像**: デフォルトの表示領域サイズに合わせた画像を設定することをお勧めしますが、サイズが合わない場合でも自動的に領域に合わせて拡縮して表示されます。また、ウィンドウを最大化したり、手動でサイズ変更した場合でもレスポンシブに追従して拡縮されます。
*   **文字色**: 設定した背景画像に合わせて、見やすい文字色をカラーパレットから自由に選択できます。
*   **期間切り替えとCSV出力**: 「今日」「今週」「今月」「全期間」のプルダウンで絞り込みができ、表示中のデータをCSVファイルとして出力することも可能です。

---

## 🎨 カスタマイズ & 外部連携機能（開発者・パワーユーザー向け）

本システムは、HTML/CSSによるデザイン変更や、Perl / PHPなどの外部スクリプト言語を用いた無限の拡張性を提供しています。

### 1. OBS 演出・ランキング画面の完全カスタマイズ

アプリ起動後、OBSが以下のURLにアクセスした際、実行ファイル（`.exe`）と同階層にある外部HTMLファイルを読み込んで配信します。これらのファイルを直接エディタで編集することで、デザインやアニメーションを完全に自作できます。（※ファイルが存在しない場合は、アプリが初期テンプレートを自動生成します）

*   **演出画面 (`overlay.html`)**: `http://localhost:28081/overlay`
    *   画面上の画像・動画・テキストのレイアウトや、CSSアニメーションを編集できます。
    *   `{{WS_PORT}}` という記述をしておくことで、アプリ側で設定された WebSocket ポートに自動置換されます。
*   **ランキング表示 (`ranking.html`)**: `http://localhost:28081/ranking`
    *   最新の使用数ランキングを一覧表示する画面です。デフォルトでは5秒ごとに最新情報を自動取得します。
    *   `{{HTTP_PORT}}` は、アプリ側で設定されたアセット配信用HTTPポートに自動置換されます。
*   **ランキングデータ取得用API (`/api/ranking`)**: `http://localhost:28081/api/ranking`
    *   現在のランキング情報をJSON形式で取得できます。
    *   クエリパラメータ `?period=0`（今日）、`?period=1`（今週）、`?period=2`（今月）、`?period=3`（全期間）で絞り込めます。

---

### 2. 演出テキストで使えるプレースホルダー（埋め込み変数）

演出設定の「吹き出し表示文字列」に以下のキーワードを含めることで、OBSの表示時に対象のデータへ自動的に置換されて描画されます。

| プレースホルダー | 置換後の効果 | 表示・置換の例 |
| :--- | :--- | :--- |
| `{user}` | チャンネルポイントを引き換えた視聴者の名前（Twitch表示名）に置換されます。 | `twitch_user` |
| `{reward_id}` | 引き換えられた Twitch カスタム報酬の一意なID文字列に置換されます。 | `rzb7kp8tg7no8ghnugx631bqqauvy3` |
| `{time}` | ポイントが引き換えられたローカル時刻に置換されます（`yyyy-MM-dd HH:mm:ss` 形式）。 | `2026-05-23 10:39:07` |

---

### 3. Perl / PHP 外部スクリプト連携

演出の種類を「**外部スクリプト実行 (script)**」に設定し、実行したいスクリプトのパス（`.pl`、`.cgi`、`.php`）を指定すると、ポイント引き換え時にアプリの裏側で非同期にスクリプトが自動実行されます。
この際、スクリプトには以下のコマンドライン引数が順番に渡されます。

#### 渡される引数の仕様

| 引数の位置 | 内容・データ型 | データの例 |
| :---: | :--- | :--- |
| **第1引数** | チャンネルポイントを引き換えた視聴者のユーザー名（文字列） | `twitch_user` |
| **第2引数** | 引き換えられた Twitch カスタム報酬 ID（文字列） | `rzb7kp8tg7no8ghnugx631bqqauvy3` |
| **第3引数** | 引き換えが行われた日時（ISO 8601形式の文字列） | `2026-05-23T10:39:07` |

#### スクリプトでの受け取り例

*   **Perlの場合 (`.pl` / `.cgi`)**
    ```perl
    # 引数配列 @ARGV から順番に取得
    my $username  = $ARGV[0]; # twitch_user
    my $reward_id = $ARGV[1]; # rzb7kp8tg7no8ghnugx631bqqauvy3
    my $timestamp = $ARGV[2]; # 2026-05-23T10:39:07
    ```
*   **PHPの場合 (`.php`)**
    ```php
    <?php
    // 引数配列 $argv から順番に取得 (※ 0番目はスクリプトファイル名が入るため1番目から)
    $username  = $argv[1]; // twitch_user
    $reward_id = $argv[2]; // rzb7kp8tg7no8ghnugx631bqqauvy3
    $timestamp = $argv[3]; // 2026-05-23T10:39:07
    ```

#### 💡 応用的な裏技アイデア (アイデア次第で無限に拡張可能)
このスクリプト起動機能を利用すると、例えば以下のような配信を盛り上げる強力な裏技が作れます。

1. **OBSテキストソースと連動したリアルタイムテロップ表示**
   * PerlやPHPのスクリプト側で、現在の日時やユーザー名・報酬名を「`1位: たぬき投げ (15回) / 2位: 音響 (10回)`」のような1行のテキストに整形します。
   * そのテキストを `now_ranking.txt` などのローカルテキストファイルに上書き保存し続けます。
   * **OBS側の設定**: OBSに「**テキスト（GDI+）**」ソースを追加し、**「ファイルからの読み込み」** にチェックを入れて上記テキストファイルを指定します。
   * これだけで、視聴者のアクションやランキングの推移が、リアルタイムに配信画面の端でカタカタ流れる電光掲示板テロップのように表示されます！

2. **Discord Webhookへのイベント自動投稿**
   * Perlの `LWP::UserAgent` や PHP の `curl` を使って、ポイントが引き換えられた瞬間に Discord の Webhook URL に向けて自動でJSONデータをPOST送信します。
   * 配信を見ていない時でも、「🎉 **引換通知**: `twitch_user` さんが `たぬき投げ` を使用しました！」といったアクティビティ履歴が自分のDiscordサーバーにリアルタイムで綺麗にログとして記録されます。

> [!WARNING]
> #### ⚠️ 外部スクリプトの利用に関する重要な免責事項（重要）
> 本システムは、ユーザーが指定した任意の外部プログラム・スクリプトを実行する機能を提供します。本機能をご利用になる場合は、以下の免責事項に完全に同意されたものとみなします。
> * **自己責任の原則**: 実行するスクリプト（自作スクリプト、および第三者から入手したスクリプト）の設計・安全性・動作テストに関しては、すべて**ユーザー自身の完全な責任**となります。
> * **損害に対する一切の免責**: 当システムおよびその開発者（BLUE000 / TwitchChannelPointManager）は、実行された外部スクリプトが引き起こした**あらゆる損害**（PC内の重要データの破損・消失、セキュリティインシデント、個人情報の漏洩、システムエラー、スマート家電等の誤動作など）に関して、**一切の責任および賠償義務を負いません。**
> * **安全性の確保**: 出所の分からない第三者が作成したスクリプトや、中身（コード）を確認できない暗号化されたスクリプトは、マルウェアや悪意ある動作をするリスクがあるため、**絶対に本システムに設定して実行させないでください。** 必ずコードを一行ずつ確認し、安全であることを確認した上でご使用ください。

---

### 4. 具体的なファイル作成例

#### ① カスタム演出画面例 (`overlay.html`)
以下は、WebSocketイベントを直接受信し、独自のCSS装飾をしたゴールドのバナー演出を表示した後に、C++アプリに再生完了通知（`effect_completed`）を送るシンプルな記述例です。

```html
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="utf-8">
    <title>Custom Overlay</title>
    <style>
        body { margin: 0; background-color: transparent; overflow: hidden; }
        .simple-banner {
            position: absolute; top: 15%; left: 50%; transform: translateX(-50%);
            background: rgba(0, 0, 0, 0.85); color: #FFD700; padding: 15px 30px;
            font-size: 26px; font-weight: bold; border-radius: 8px;
            border: 2px solid #FFD700; font-family: 'Arial', sans-serif;
            text-shadow: 2px 2px 4px #000; box-shadow: 0 4px 15px rgba(0,0,0,0.5);
        }
    </style>
</head>
<body>
    <div id="wrapper"></div>
    <script>
        // ポート番号はアプリ起動時に自動置換されます
        const ws = new WebSocket("ws://localhost:{{WS_PORT}}/overlay");
        
        ws.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            if (msg.type === "show_effect") {
                const data = msg.data;
                const wrapper = document.getElementById("wrapper");
                
                // {user} 置換済みの吹き出し用文字列を表示
                wrapper.innerHTML = `<div class="simple-banner">${data.effect.text}</div>`;
                
                // 5秒表示したあとにバナーを消し、C++側へ完了通知を送信して次の演出キューへ進める
                setTimeout(() => {
                    wrapper.innerHTML = "";
                    ws.send(JSON.stringify({
                        type: "effect_completed",
                        data: { queueId: data.queueId }
                    }));
                }, 5000);
            }
        };
    </script>
</body>
</html>
```

#### ② Perlによる履歴書き出し例 (`logger.pl` / `logger.cgi`)
Twitchでポイントが使われるたびに、ローカルディレクトリ内のテキストファイル `point_history.txt` へ実行履歴を自動で追記していくスクリプト例です。

```perl
#!/usr/bin/perl
use strict;
use warnings;
use utf8;
use open ':std', ':encoding(UTF-8)';

# コマンドライン引数の受け取り（無い場合のデフォルト値付き）
my $username  = $ARGV[0] // "Anonymous";
my $reward_id = $ARGV[1] // "Unknown";
my $timestamp = $ARGV[2] // "N/A";

# ログファイル名
my $log_file = "point_history.txt";

# ファイルへ追記 (Append) モードでオープン
if (open(my $fh, ">>:encoding(UTF-8)", $log_file)) {
    print $fh "[$timestamp] 視聴者 $username が報酬 (ID: $reward_id) を使用しました。\n";
    close($fh);
    print "Perl Log write success.\n";
} else {
    warn "Cannot open $log_file: $!";
}
```

#### ③ PHPによる履歴書き出し例 (`logger.php`)
上記と同様に、ポイント引き換えログを自動的にローカルのファイルに追記するPHPによるスクリプト例です。

```php
<?php
// 文字エンコーディングの設定
header('Content-Type: text/plain; charset=UTF-8');

// コマンドライン引数の受け取り (argv[0]にはスクリプト名が入るため1番目から取得)
$username  = isset($argv[1]) ? $argv[1] : "Anonymous";
$reward_id = isset($argv[2]) ? $argv[2] : "Unknown";
$timestamp = isset($argv[3]) ? $argv[3] : "N/A";

// ログファイル名
$log_file = "point_history.txt";

// 追記するメッセージ行を生成
$log_message = sprintf("[%s] 視聴者 %s が報酬 (ID: %s) を使用しました。\n", $timestamp, $username, $reward_id);

// 排他ロックをかけつつ追記保存
if (file_put_contents($log_file, $log_message, FILE_APPEND | LOCK_EX) !== false) {
    echo "PHP Log write success.\n";
} else {
    echo "Log write failed.\n";
}
```

---

## ❓ トラブルシューティング

* **Q. OBSに演出が表示されない・音が聞こえない**
  * **A1.** アプリのシステム設定で「WebSocketサーバー」および「アセットサーバー」が稼働中になっているかご確認ください。
  * **A2.** OBSのブラウザソースのURLが正確に `http://localhost:28081/overlay` に設定されているか確認してください。アセットサーバーのポート番号（28081）はシステム設定で変更可能です。
* **Q. アセット参照で大容量ファイルを設定すると重くなる？**
  * **A. 当アプリは超軽量アセットサーバーを内蔵しています。** アセットは必要な時にのみOBSへURL変換されて配信されるため、PCゲームやOBSのエンコード性能に影響を与えない超低負荷動作が保証されています。

---

## 📄 ライセンス・権利表記 (Licenses & Credits)

本システムはオープンソースで公開されています。

### 本システム本体のライセンス
本システム（TwitchChannelPointManager）は、**MIT License** のもとで配布されています。

```text
The MIT License (MIT)

Copyright (c) 2026 BLUE000

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

### サードパーティ製ライブラリの権利表記
本システムで使用している難読化ライブラリ（TransCipher）の権利表記は以下の通りです。

```text
This software uses TransCipher library.
Copyright (c) 2026 BLUE000.
```
