<p align="center">
  <img src="brand/out/marketing/readme-banner.png" alt="VoiceBooth — 歌ってみた専用DAW。見て直して、一本渡す。" width="100%">
</p>

<p align="center">
  <b>日本語</b> ·
  <a href="README.en.md">English</a> ·
  <a href="README.ko.md">한국어</a> ·
  <a href="README.zh-Hans.md">简体中文</a> ·
  <a href="README.zh-Hant.md">繁體中文</a> ·
  <a href="README.es.md">Español</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-beta-F4B942?labelColor=141311" alt="ベータ">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-8CC1EE?labelColor=141311" alt="Windows / macOS">
  <img src="https://img.shields.io/badge/license-AGPL--3.0--or--later-C6EE6A?labelColor=141311" alt="AGPL-3.0-or-later">
</p>

# VoiceBooth

**歌ってみた専用の、小さなボーカル DAW です。**
オフボーカルに合わせて歌いながら、音程を画面で見て直し、直したい所だけ録り直して、ミックス担当にそのまま渡せる WAV を書き出す。そのための機能だけを入れています。万能 DAW ではありません。

![VoiceBooth のメイン画面](docs/screenshots/main-ja.png)

> 画面は開発中の見本（UI モック。見本のデータを描いたもの）です。実際の曲での表示は改良中で、見え方が違う場合があります。

> [!NOTE]
> **ベータ版です。** 主な機能はそろいましたが、作者の手元の Windows / Mac での確認はこれからです。不具合や「ここが分かりにくい」は [Issues](https://github.com/kajisho5/voicebooth/issues) へどうぞ。

## 先に読んでください

| | |
|---|---|
| 対応 OS | **Windows と Mac の両方**（Mac は Apple シリコン / Intel 両対応）。**スマホ・タブレット版はありません** |
| グラフィックボード | **要りません。** CPU だけで動きます。重いのはボーカル分離だけで、4 コアの CPU で 30 秒の曲に約 2 分かかりました（画面に見込み時間が出ます） |
| 入れられる音源 | **手元の音声ファイルだけ**（wav / flac / aiff / ogg / mp3 / m4a）。Spotify・Apple Music・YouTube Music などのサブスクからは直接入れられません |
| 大きさ | インストーラーは Windows 約 13 MB、Mac 約 40 MB。分離のモデル（約 223 MB）は**使う時にボタンを押した時だけ**ダウンロードします。勝手に取りに行きません |
| 重い処理 | 分離は**押した時だけ**動きます。歌詞の表示は**既定でオフ**（設定で出せます） |
| ハモリ | ハモリのトラックを録ることはできます。**ミックスからハモリだけを取り出してお手本にする機能はありません**（分離は「声全体」と「伴奏」の 2 つ） |
| 分離した音の扱い | 分離した声・伴奏は**個人の練習用**です。VoiceBooth は元の曲の権利を変えません。配布・公開（歌ってみたのオフボとして配るのを含む）は、元の曲の権利者が許している範囲だけにしてください |
| 料金 | **無料です。** サブスクも課金もありません。支援は [GitHub Sponsors](https://github.com/sponsors/kajisho5) から（任意。機能は変わりません） |
| 言語 | 日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español |

## できること

| | 内容 |
|---|---|
| 曲を開く | 上の形式。ドラッグ＆ドロップでも開ける。曲の頭の位置は Windows と Mac で変わらない |
| 原曲＋カラオケ | 声入りの原曲をお手本に、カラオケ（オフボ）の上で歌う。前奏の長さの違いなどは自動で時間を合わせる。原曲からカラオケを引いて声を取り出し、お手本の音程の線にする |
| 原曲だけ | 原曲を分離してオフボを作り、お手本の線も出す（分離のモデルが要る） |
| 音程を色で | 自分の声の音程を線で重ね、合っていればライム、ずれるとアンバー→赤。オクターブ違いで歌っても合わせて表示できる |
| 練習 | テンポ 50〜150 %、キー ±6。遅くして練習しても、納品は原速・原キーで録る |
| お手本を聴く | 原曲から取り出したお手本の声を、オフボと一緒に・ソロで聴ける（練習のテンポ・キーも掛かる）。メインとハモリは分けられないので 1 本 |
| 声域とおすすめのキー | マイクで声域（いちばん低い声・高い声）を測ると、お手本の最低音・最高音が収まるキーを出す。押すとそのキーに。収まらない時は上下に何半音はみ出すかも出す |
| 録音 | 通し録音、遡及録音（REC を押し遅れても歌い出しが欠けない）、範囲の録り直し（つなぎ目は 8 ms のクロスフェード。プロで 0〜20 ms）、遅延の測定と補正 |
| クリック・カウントイン | 曲のテンポでクリック（1 拍目は高い音。練習のテンポにも合う）。REC の前に 1〜2 小節数える。範囲の録り直しは範囲の前を数える。耳だけで、録音・書き出しには入らない |
| Main / Double / Harmony | ダブルとハモリを同じ尺で録って一緒に鳴らす |
| 入りのタイミング | お手本と比べて、入りが何 ms 早い・遅いかを出す（標準以上）。プロでは音程の合っている割合・ビブラートも |
| 書き出し | 曲の頭からのフル尺 WAV、納品パック（トラックごとの WAV・確認用ミックス・メモ・zip） |
| 歌詞（既定オフ） | .txt / .lrc の読み込み、タップで合わせる |
| スキン | 色をまとめて着せ替え（内蔵 10 種）。`.vbskin` ファイルで人と共有できる |

### まだできないこと

ベータの今は、次のものは入っていません（画面にも出していません）。

- ハモリのお手本の線（ハモリのトラックを選んでも、メインのお手本の線を出します）
- テイクの聴き比べ、ギター・ドラムなど声以外の分離

> [!IMPORTANT]
> 分離のモデルは、**配布の準備中**です（署名した一覧の公開待ち）。それまでは、原曲＋カラオケの組でお手本の線を出す・録る・書き出す、は使えます。

### 渡すファイルの約束

ミックス担当が DAW に置いた瞬間、オフボと頭がそろうように書き出します（目標 ±1 ms）。

- 曲の頭（0 秒）から終わりまでのフル尺。録っていない所は無音
- 24bit・モノラルの WAV。サンプリングレートは元の曲のまま（勝手に 48 kHz にしない）
- ノーマライズ・自動フェードなし。モニター用のリバーブやガイドは混ぜない
- 練習で録ったものは納品フォルダに入れない

### 3 つのモード

中身は同じで、見えるものだけ変わります。どのモードで作ったプロジェクトも、ほかのモードで開けます。

| 簡単 | 標準 | プロ |
|---|---|---|
| Main を通しで録って渡すだけ | ダブル・ハモリ 1 本・区間の録り直し・入りタイミング・納品パック | ハモリ 2 本、音程の合っている割合・ビブラートの解析、つなぎ目のクロスフェードの長さ |

| 簡単モード | プロモード（ハモリ） |
|---|---|
| ![簡単モード](docs/screenshots/mode-easy.png) | ![プロモードのハモリ](docs/screenshots/mode-pro-harmony.png) |

## ロゴとデザイン

<p>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="brand/out/logo/logo-mark-dark.png">
    <img src="brand/out/logo/logo-mark-light.png" alt="VoiceBooth のマーク" width="96" align="left">
  </picture>
  <b>マーク：録音ブースの窓、マイクのカプセル、タリーランプ。</b><br>
  ブースの窓越しにマイクがあり、右上のランプが灯っている形です。タリーはふだんライムで、アプリの中では録音中だけ赤く灯ります。
</p>
<br clear="left">

**コンセプトは「夜の録音ブース」。** 暖かみのある暗いグラファイトに、機材の LED とタリーランプが灯ります。状態は枠の色ではなく LED の点灯で示し、録音中以外は画面を赤くしません。

![色見本](brand/out/marketing/palette.png)

| 色 | 使う所 |
|---|---|
| Signal（ライム） | 自分のピッチが合っている・再生ヘッド・点灯した LED |
| Reference（アイスブルー） | お手本の許容の帯 |
| Amber / Coral | 少しずれている / 大きくずれている・警告 |
| Tally（赤） | 録音中だけ |

- **書体**：IBM Plex Sans JP。時間・dB などの数値は IBM Plex Mono（等幅なので桁が揺れない）
- **部品**：キーキャップ＋LED のボタン、LED リングのツマミ、縦のコンソールフェーダー、セグメント LED のメーター
- **動き**：ばねで押し込まれるキー、0 dB で手応えのあるフェーダー、録音中だけ赤く灯るタリー。音を最優先にし、OS の「動きを減らす」設定にも従います

**スキン。** 色だけをまとめて着せ替えられます（書体・配置・動きは同じ）。内蔵は 10 種類で、明るい部屋向けの Studio Day / Sweet、見やすさ優先の High Contrast、色の見分けにくさに配慮した Color Safe もあります。設定の「スキン」→「新しく作る」でテンプレートを選び、色ごと・グループごと（ほかのテンプレートから借りることも）に変えて保存できます。見づらい組み合わせは編集中に知らせます（保存は止めません）。`.vbskin` ファイルで人と共有できます。

![内蔵スキン 10 種](docs/screenshots/skins/all.png)

| アプリアイコン | ブランドキット |
|---|---|
| ![アプリアイコン](brand/out/preview/preview-icons.png) | ![ブランドキット](brand/out/preview/preview-brand.png) |

ロゴの使い方（余白・最小サイズ・明るい背景用）と全素材は [`brand/`](brand/README.md) にあります。

## 動作環境（暫定）

| | 最低 | 推奨 |
|---|---|---|
| Windows | Windows 10 64bit（バージョン 1607 以降） | Windows 11 |
| Mac | macOS 11 Big Sur 以降（Apple シリコン / Intel 両対応のユニバーサル版） | 最新の macOS、Apple シリコン |
| CPU | 64bit・4 コア | 6 コア以上（Apple M1 以降、ここ数年の Intel Core i5 / AMD Ryzen 5 クラス以上） |
| メモリ | 8 GB | 16 GB |
| 空き容量 | 2 GB | 10 GB 以上（SSD） |
| 画面 | 1280×800 | 1440×900 以上 |
| 音声 | 内蔵の入出力でも動く | オーディオインターフェース＋有線ヘッドホン（Windows は ASIO 対応だと遅延が少ない） |
| ネット | 初回の分離モデルのダウンロードと、新しいバージョンの確認（1 日 1 回まで GitHub のリリースを見るだけで、ほかは何も送りません。設定で切れます）。つながらなくても分離以外は使える | — |

- 重いのはボーカル分離だけです。古い CPU や Intel Mac では分離に時間がかかります（実測して確定します）
- Bluetooth のイヤホン・ヘッドホンは遅延が大きく、録音には向きません
- Arm 版 Windows は未確認です
- 数値は開発中の目安です。根拠は [`docs/DESIGN.md`](docs/DESIGN.md) 11.6.1

## ダウンロード

[Releases](https://github.com/kajisho5/voicebooth/releases) から、Windows は `VoiceBooth-<版>-win-x64-setup.exe`、Mac は `VoiceBooth-<版>-mac-universal.dmg` を取ってください。

ベータ版はまだ**コード署名をしていません**。初めて開く時だけ、OS の警告が出ます。

- **Windows**：「Windows によって PC が保護されました」と出たら「詳細情報」→「実行」
- **Mac**：DMG の VoiceBooth をアプリケーションフォルダへ入れて一度開く → 「開けません」と出たら、システム設定 →「プライバシーとセキュリティ」→ セキュリティの「このまま開く」（開こうとしてから約 1 時間だけ出ます）→ パスワード（[Apple の説明](https://support.apple.com/ja-jp/guide/mac-help/mh40616/mac)）

## 開発の支援

VoiceBooth は無料です。気に入ったら [GitHub Sponsors](https://github.com/sponsors/kajisho5) で支援していただけると開発が続けられます。支援の有無で使える機能は変わりません。

## ライセンス

ソースコードは **GNU Affero General Public License v3.0 以降（AGPL-3.0-or-later）** です（[`LICENSE`](LICENSE)）。JUCE 8 を AGPLv3 で使っているため、アプリ全体を AGPL にしています。

- 使う・改造する・配る・売るのは自由です。配る時（改造版をネット越しに使わせる時も）は、ソースも同じライセンスで公開してください
- **名前とロゴ**：「VoiceBooth」の名前とロゴは、改造版を別の製品として配る時には使わないでください（別の名前・ロゴにする）。元のままの再配布や、紹介・レビューでの使用は構いません
- 録音・書き出した音声ファイルはあなたのものです。AGPL は作った作品には及びません

| 同梱・利用しているもの | ライセンス |
|---|---|
| JUCE 8 | AGPLv3 / 商用のデュアル（ここでは AGPLv3） |
| minimp3（`third_party/minimp3`） | CC0 |
| IBM Plex Sans JP / IBM Plex Mono（`resources/fonts`） | SIL Open Font License 1.1 |
| ONNX Runtime 1.22.0（ボーカル分離の別プロセスだけ。ビルド時に公式のビルド済みを取得） | MIT |
| Monocypher 4.0.3（分離モデルの一覧の Ed25519 署名を確かめる。ビルド時に取得） | CC0 / BSD-2-Clause のデュアル |
| Rubber Band Library 4（練習用のテンポ / キー。ビルド時に取得） | GPL v2 以降 / 商用のデュアル（ここでは GPL） |
| Steinberg ASIO SDK 2.3.4（Windows のビルドだけ。ビルド時に公式の配布物を取得） | GPLv3 / 商用のデュアル（ここでは GPLv3）。SDK 自体はリポジトリに入れない。ASIO は Steinberg Media Technologies GmbH の商標 |
| モデル（アプリとは別に、押した時だけダウンロード）：分離 Mel-Band RoFormer（Kimberley Jensen） | MIT。重みのライセンスを確かめたものだけ配る |

## 開発に参加する

ビルド・テスト・構成は [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)、仕様は [`docs/DESIGN.md`](docs/DESIGN.md)（どちらも日本語）。
