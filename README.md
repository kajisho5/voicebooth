<p align="center">
  <img src="brand/out/marketing/readme-banner.png" alt="VoiceBooth — 歌ってみた専用DAW。見て直して、一本渡す。" width="100%">
</p>

<p align="center">
  <b>日本語</b> ·
  <a href="README.en.md">English</a> ·
  <a href="README.ko.md">한국어</a> ·
  <a href="README.zh-Hans.md">简体中文</a> ·
  <a href="README.zh-Hant.md">繁體中文</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-in%20development-F4B942?labelColor=141311" alt="開発中">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-8CC1EE?labelColor=141311" alt="Windows / macOS">
  <img src="https://img.shields.io/badge/license-AGPL--3.0--or--later-C6EE6A?labelColor=141311" alt="AGPL-3.0-or-later">
</p>

# VoiceBooth

**歌ってみた専用の、小さなボーカル DAW です。**
オフボーカルに合わせて歌いながら、音程を画面で見て直し、直したい所だけ録り直して、ミックス担当にそのまま渡せる WAV を書き出す。そのための機能だけを入れています。万能 DAW ではありません。

![VoiceBooth のメイン画面](docs/screenshots/main-ja.png)

> [!NOTE]
> **開発中です。まだ配布していません。** いま動くのは「曲を開いて波形を見る」「オフボを再生・シーク・ループする」までです。画面の数値・歌詞・ピッチは見本です。

## できること

| | 内容 | 状態 |
|---|---|---|
| 曲を開く | wav / flac / aiff / ogg / mp3 / m4a。ドラッグ＆ドロップでも開ける。曲の頭の位置は Windows と Mac で変わらない | ✅ 動く |
| オフボの再生 | 再生・シーク・範囲ループ・音量。ループのつなぎ目にすき間なし | ✅ 動く |
| 入力デバイスとレベル | マイクを選び、目標の帯（-12〜-6 dB）を見ながらレベルを合わせる | 🔧 開発中 |
| お手本ピッチ | お手本の音程を「許容の帯」、自分の声を線で重ねる。合っていればライム、ずれるとアンバー→赤 | ⬜ 予定 |
| 原曲＋カラオケ | 声入りの原曲をお手本に、カラオケ（オフボ）の上で歌う。前奏の長さ・速さの違い・カット版・キー違いの版も自動で時間を合わせ、手で微調整もできる。渡すファイルは常にカラオケの頭から | 🔧 合わせる部品はできた（画面はこれから） |
| 練習 | テンポ 50〜150%、キー ±6。遅くして練習しても、納品は原速・原キーで録る | ⬜ 予定 |
| 遡及録音 | 再生を始めた時から裏で録っていて、REC を押し遅れても歌い出しが欠けない | ⬜ 予定 |
| 区間の録り直し | 範囲を選んでパンチイン。つなぎ目は 8 ms のクロスフェード | ⬜ 予定 |
| Main / Double / Harmony | ダブルとハモリを同じ尺で録る | ⬜ 予定 |
| ボーカル分離 | ミックス音源からオフボとお手本を作る（初回にモデルをダウンロード。途中で切れても続きから） | ⬜ 予定 |
| 5 言語 | 日本語 / English / 한국어 / 简体中文 / 繁體中文 | ✅ 動く |
| スキン | 色をまとめて着せ替え（内蔵 10 種）。テンプレートから自分で作り、`.vbskin` ファイルで人と共有できる | ✅ 動く |

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
| Main を通しで録って渡すだけ | ダブル・ハモリ 1 本・区間の録り直し・入りタイミング | ハモリ 2 本＋左右のダブル、ビブラートなどの解析、納品パック |

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
- **動き**：ばねで押し込まれるキー、0 dB で手応えのあるフェーダー、拍に合わせて灯るカウントイン。音を最優先にし、OS の「動きを減らす」設定にも従います

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
| ネット | 初回の分離モデルのダウンロードだけ（つながらなくても分離以外は使える） | — |

- 重いのはボーカル分離だけです。古い CPU や Intel Mac では分離に時間がかかります（実測して確定します）
- Bluetooth のイヤホン・ヘッドホンは遅延が大きく、録音には向きません
- Arm 版 Windows は未確認です
- 数値は開発中の目安です。根拠は [`docs/DESIGN.md`](docs/DESIGN.md) 11.6.1

## ダウンロード

まだ配布していません。公開の準備ができたら、このページの Releases に置きます。

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
| Rubber Band Library 4（練習用のテンポ / キー。ビルド時に取得） | GPL v2 以降 / 商用のデュアル（ここでは GPL） |
| Steinberg ASIO SDK 2.3.4（Windows のビルドだけ。ビルド時に公式の配布物を取得） | GPLv3 / 商用のデュアル（ここでは GPLv3）。SDK 自体はリポジトリに入れない。ASIO は Steinberg Media Technologies GmbH の商標 |
| 予定：分離・ピッチ・歌詞のモデル | 重みのライセンスを確かめたものだけ。アプリとは別に配る |

## 開発に参加する

ビルド・テスト・構成は [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)、仕様は [`docs/DESIGN.md`](docs/DESIGN.md)（どちらも日本語）。
