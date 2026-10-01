# VoiceBooth ブランド素材

![アイコン](out/preview/preview-icons.png)
![ブランドキット](out/preview/preview-brand.png)

## マーク

**窓（ブース）＋マイクのカプセル＋タリーランプ**。録音ブースの窓越しにマイクがあり、右上のランプが灯っている形。

- タリーは通常ライム `#C6EE6A`（待機・再生）。アプリ内では録音中だけ赤 `#FF3B30` に灯る
- 機能の参考にした 既存の練習アプリ の波形アイコンとは別系統（DESIGN 4）
- アプリ内のロゴ（`app/ui/TopBar.cpp` の `drawBoothMark`）と同じ形

## 素材一覧（`out/`）

| 用途 | ファイル | 備考 |
|---|---|---|
| macOS アプリアイコン | `icons/VoiceBooth.icns`、`icons/mac/icon_16〜1024.png` | 1024 キャンバスに 824 の角丸＋影（Big Sur 以降の形） |
| Windows アプリアイコン | `icons/VoiceBooth.ico`（16/24/32/48/64/128/256）、`icons/win/*.png` | 16/24/32/48 は線を画素に合わせた専用版 |
| ビルド用（CMake） | `icons/juce_icon_big_mac.png`、`juce_icon_big_win.png`、`juce_icon_small.png` | `CMakeLists.txt` の `ICON_BIG` / `ICON_SMALL` |
| ロゴ（横組み） | `logo/logo-horizontal-{dark,light,mono-white,mono-black}.png` | dark = 暗い背景用 |
| ロゴ（縦組み） | `logo/logo-stacked-*.png` | 肩書き「歌ってみた専用DAW」入り |
| マーク単体 | `logo/logo-mark-*.png` | タリーの周りは透過で抜いてあるので、どの背景色にも置ける |
| SNS・GitHub 共有画像 | `marketing/social-preview.png`（1280x640） | GitHub の Settings → Social preview に設定 |
| README ヘッダー | `marketing/readme-banner.png`（1280x320、日本語）、`readme-banner-{en,ko,zh-Hans,zh-Hant}.png` | 肩書きはアプリの翻訳表 `app.tagline` と同じ文。韓国語・中国語は Noto Sans CJK の各地域のフェイス |
| README の色見本 | `marketing/palette.png`（1280x200） | DESIGN 4.9 のトークン。名前は英語（全言語の README で共用） |
| Web | `marketing/favicon-32.png`、`favicon.svg`（src）、`apple-touch-icon-180.png` | |
| macOS DMG 背景 | `installer/dmg-background.png`（660x400）、`@2x` | アイコン位置: VoiceBooth.app (165, 200) / Applications (495, 200)。名前は Finder が描く |
| Windows インストーラー | `installer/installer-wizard.png`（164x314）、`installer-small.png`（55x55）、各 `@2x` | Inno Setup の WizardImageFile / WizardSmallImageFile |

ベクターの原本は `src/*.svg`（文字はアウトライン化済み。フォントが無い環境でも同じ見た目）。

## 使い方の決まり

- マークの周りには、マークの幅の 1/4 以上の余白を空ける
- 最小サイズ：マーク単体 16px、横組みロゴ 高さ 24px
- 色は DESIGN 4.9 のトークンだけを使う（地 `#141311`、文字 `#F2EDE3`、信号 `#C6EE6A`、お手本 `#8CC1EE`）
- 明るい背景では light 版（文字はグラファイト、タリーは `#7FB51F`）
- マークを回転・変形・影付け・グラデーション化しない（アプリアイコンの光沢は例外）
- 書体：IBM Plex Sans JP SemiBold（ワードマーク）／ Medium（肩書き）

## 作り直し

```bash
sudo apt-get install -y librsvg2-bin icnsutils imagemagick fonts-noto-cjk
pip install fonttools uharfbuzz
python3 brand/build_brand.py
```

- 色や形を変える時は `build_brand.py` を直して再生成する（PNG を直接いじらない）
- 文字は `text_paths.py` が IBM Plex（`resources/fonts/`）でアウトライン化する。同梱フォントに無い字（ハングル・簡体字）は Noto Sans CJK で補う

## 未対応

- Windows の .exe に埋め込まれるアイコンは、JUCE が `ICON_BIG` / `ICON_SMALL` から作る。手調整した 16/24/48 を確実に使うには、配布時にインストーラーかリソース（.rc）で `VoiceBooth.ico` を指定する
- プロジェクトファイル用のドキュメントアイコンは、拡張子（ファイル関連付け）を決めてから作る（現状はフォルダ＋ project.json）
