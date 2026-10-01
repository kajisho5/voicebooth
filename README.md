![VoiceBooth](brand/out/marketing/readme-banner.png)

# VoiceBooth

歌ってみた専用の超小型ボーカルDAW。万能DAWではない。

> 見て直して、一本渡す。

仕様は [`docs/DESIGN.md`](docs/DESIGN.md) が唯一の正。

## 現在の状態：Phase B1（曲を開いて実波形）— 手動確認待ち

- Phase A（A1–A7）：メイン画面・操作の見た目（再生ヘッド・REC・範囲・ショートカット）・モード出し分け・起動 / 入力セットアップ / 書き出し / 設定
- **B1**：曲ファイル（wav / flac / aiff / ogg / mp3 / m4a。mp3・m4a は Win・Mac の OS 標準デコーダ）を開くと、形式・長さ・SR・ch を読み、実波形を描く。起動画面のクリック / ドロップ、メイン画面へのドロップ、`--open=<path>`
- **日本語 / English / 한국어 / 简体中文 / 繁體中文**。初回起動で言語を選び、以後は記憶（設定から変更可。DESIGN 10.1）
- **音声デバイスは開かない**（`VOICEBOOTH_UI_MOCK=ON`、`juce_audio_devices` をリンクしない構成）。再生・録音はまだ見た目だけで音は出ない（B2 以降）

| 曲を読み込んだところ | 開いた曲の画面（実波形） |
|---|---|
| ![](docs/screenshots/b1-loaded.png) | ![](docs/screenshots/b1-main.png) |

![メイン画面](docs/screenshots/main-ja.png)

| 録音中 | 書き出し | 設定（繁體中文） |
|---|---|---|
| ![](docs/screenshots/recording.png) | ![](docs/screenshots/export.png) | ![](docs/screenshots/settings-zh-Hant.png) |

| English | 한국어 | 简体中文 |
|---|---|---|
| ![](docs/screenshots/main-en.png) | ![](docs/screenshots/main-ko.png) | ![](docs/screenshots/main-zh-Hans.png) |

見た目は v2 "Booth"（夜の録音ブース：暖色グラファイト＋LED＋タリー）。機能の参考にした 既存の練習アプリ とは配色・書体・部品・配置を変えている（DESIGN 4 / 4.9）。

起動オプション（`--lang=en` `--mode=pro` `--screen=export` など）は [`docs/UI_STATES.md`](docs/UI_STATES.md)。

部品ギャラリー（全部品を状態ごとに表示。開発用）: `VoiceBooth --gallery`

![部品ギャラリー](docs/screenshots/parts-gallery.png)

画面の状態一覧は [`docs/UI_STATES.md`](docs/UI_STATES.md)。

## ビルド

必要なもの: CMake 3.22+、C++17 コンパイラ、Git（JUCE 8.0.15 を初回 configure 時に自動取得）

### Windows（Visual Studio 2022）

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\VoiceBooth_artefacts\Release\VoiceBooth.exe
```

### macOS（Xcode / Command Line Tools）

```bash
cmake -S . -B build -G Xcode
cmake --build build --config Release
open build/VoiceBooth_artefacts/Release/VoiceBooth.app
```

### Linux（開発確認用。配布対象外）

```bash
sudo apt-get install -y libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
                        libfreetype-dev libfontconfig1-dev ninja-build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/VoiceBooth_artefacts/Release/VoiceBooth
```

JUCE を手元のチェックアウトから使う場合: `-DVOICEBOOTH_JUCE_DIR=/path/to/JUCE`

### テスト

```bash
ctest --test-dir build -C Release --output-on-failure
```

波形の概形（ピーク・RMS、ビン境界・全チャンネル・粗い段と総当たりの一致）と、曲の読み込み（WAV / FLAC の長さがサンプル単位で一致、日本語と空白を含むパス、壊れた / 空のファイル、中止、非同期の通知、mp3 / m4a を OS の読み手で開けるか・長さと頭のずれ）を確かめる。CI（Win / Mac）でも実行。テスト用音声は `tests/data/make_fixtures.sh` で作る自作の合成音。

## 構成

```text
docs/DESIGN.md        仕様（唯一の正）
docs/UI_STATES.md     画面状態一覧
app/Main.cpp          アプリ / ウィンドウ（既定 1440x900、最小 1280x800）/ 起動オプション / 設定保存
app/i18n/             多言語対応 tr("key")（5 言語）
app/ui/UiSession.*    画面の状態と変更通知（Phase B で音声エンジンにつなぐ）
app/audio/            曲の読み込み（SongLoader）と波形の概形（WaveformOverview）
tests/                VoiceBoothTests（ctest）
app/ui/screens/       起動画面 / 入力セットアップ / 書き出し / 設定
app/ui/Theme.*        色トークン・書体・描画の基本（キーキャップ / 表示窓 / LED）
app/ui/parts/         部品：KeyButton / SegmentedKeys / Encoder / ConsoleFader / LedMeter / Readout / TallyLamp / Icons
app/ui/Gallery.*      部品ギャラリー
app/ui/*.cpp          画面：TopBar / TransportBar / PitchLane / LyricsLane / WaveLane / TrackTabs / Rack / StatusBar
app/ui/DummySession.* 固定ダミー（DESIGN 20）。結線時に差し替える
app/audio/            音声エンジンのインターフェースのみ（中身は Phase B）
app/project/          データモデル（形のみ）+ project.example.json
app/export/           ExportService スタブ
resources/fonts/      IBM Plex Sans JP / IBM Plex Mono（SIL OFL 1.1）
resources/i18n/       翻訳表 ja / en / ko / zh-Hans / zh-Hant
tools/check_i18n.py   翻訳表と直書きの検査（CI でも実行）
```

## 多言語対応

画面の文字は必ず `tr("key")` で引き、`resources/i18n/` の全言語（ja / en / ko / zh-Hans / zh-Hant）にキーを足す。

```bash
python3 tools/check_i18n.py
```

言語を足す時は `app/i18n/I18n.cpp` の `available()` に 1 行、JSON を 1 枚、`CMakeLists.txt` の埋め込みに 1 行（DESIGN 10.1）。
韓国語・中国語は OS の標準フォントで表示する（Linux で確認する場合は `fonts-noto-cjk` を入れる）。

## ブランド素材

アプリアイコン・ロゴ・SNS 画像・インストーラー画像は [`brand/`](brand/README.md)（`python3 brand/build_brand.py` で再生成）。

## ライセンス上の注意（未決事項に関係）

- **JUCE 8** は AGPLv3 と商用 JUCE ライセンスのデュアルライセンス。配布形態（有料/無料、ソース公開の有無）に応じてどちらで使うか決める必要がある（DESIGN 19 未決）
- **IBM Plex Sans JP / IBM Plex Mono** は SIL Open Font License 1.1（`resources/fonts/OFL-*.txt`）。アプリへの同梱・再配布可（フォント名 "Plex" は予約名なので、改変した場合は別名にする）
- 既存の録音ソフト（AGPL）のソースはコピーしない（DESIGN 0）
- ASIO SDK はリポジトリに入れない
