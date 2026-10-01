<p align="center">
  <img src="brand/out/marketing/readme-banner-zh-Hant.png" alt="VoiceBooth — 翻唱專用 DAW。看著修正，交出一軌。" width="100%">
</p>

<p align="center">
  <a href="README.md">日本語</a> ·
  <a href="README.en.md">English</a> ·
  <a href="README.ko.md">한국어</a> ·
  <a href="README.zh-Hans.md">简体中文</a> ·
  <b>繁體中文</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-in%20development-F4B942?labelColor=141311" alt="開發中">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-8CC1EE?labelColor=141311" alt="Windows / macOS">
  <img src="https://img.shields.io/badge/license-AGPL--3.0--or--later-C6EE6A?labelColor=141311" alt="AGPL-3.0-or-later">
</p>

# VoiceBooth

**專為翻唱錄音打造的小型人聲 DAW。**
跟著伴奏唱，在螢幕上看著音高修正，只重錄想改的部分，再匯出混音師可以直接使用的 WAV。只做這些事，不是萬能 DAW。

![VoiceBooth 主畫面](docs/screenshots/main-zh-Hant.png)

> [!NOTE]
> **開發中，尚未發布。** 目前可用的只有「開啟歌曲並查看波形」和「伴奏的播放、跳轉、循環」。畫面上的數值、歌詞和音高都是範例資料。

## 功能

| | 說明 | 狀態 |
|---|---|---|
| 開啟歌曲 | wav / flac / aiff / ogg / mp3 / m4a，也可拖放開啟。歌曲開頭的位置在 Windows 和 Mac 上一致 | ✅ 可用 |
| 播放伴奏 | 播放、跳轉、區段循環、音量。循環銜接處沒有空隙 | ✅ 可用 |
| 輸入裝置與音量 | 選擇麥克風，對照目標範圍（-12～-6 dB）調整輸入音量 | 🔧 開發中 |
| 參考音高 | 參考旋律以「容許帶」顯示，你的聲音以線條疊加。唱準時為萊姆綠，偏離時變為琥珀色→紅色 | ⬜ 規劃中 |
| 練習 | 速度 50～150%，調 ±6。可以放慢練習，但交付錄音一律以原速、原調錄製 | ⬜ 規劃中 |
| 回溯錄音 | 從開始播放起就在背景錄音，即使晚按 REC，開頭也不會被切掉 | ⬜ 規劃中 |
| 分段重錄 | 選擇範圍進行插錄（Punch-in），銜接處 8 ms 交叉淡化 | ⬜ 規劃中 |
| Main / Double / Harmony | 以相同長度錄製疊唱與和聲 | ⬜ 規劃中 |
| 人聲分離 | 從混音音源產生伴奏與參考人聲（第一次使用時下載模型，中斷後可接續下載） | ⬜ 規劃中 |
| 5 種語言 | 日本語 / English / 한국어 / 简体中文 / 繁體中文 | ✅ 可用 |

### 交付檔案的約定

匯出的檔案放進 DAW 的那一刻，就與伴奏的開頭對齊（目標 ±1 ms）。

- 從歌曲開頭（0 秒）到結尾的完整長度，沒有錄音的部分為靜音
- 24bit 單聲道 WAV，取樣率與原曲相同（不會擅自改為 48 kHz）
- 不做正規化，不加自動淡入淡出；監聽用的殘響和參考人聲不會混入
- 練習錄音不會放進交付資料夾

### 三種模式

引擎相同，只是顯示的內容不同。在任何模式下建立的專案都能在其他模式中開啟。

| 簡單 | 標準 | 專業 |
|---|---|---|
| 整首錄 Main 後直接交付 | 疊唱、一條和聲、分段重錄、進入時機 | 兩條和聲＋左右疊唱、顫音等分析、完整交付包 |

| 簡單模式 | 專業模式（和聲） |
|---|---|
| ![簡單模式](docs/screenshots/mode-easy.png) | ![專業模式和聲](docs/screenshots/mode-pro-harmony.png) |

## 標誌與設計

<p>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="brand/out/logo/logo-mark-dark.png">
    <img src="brand/out/logo/logo-mark-light.png" alt="VoiceBooth 標誌" width="96" align="left">
  </picture>
  <b>標誌：錄音室的窗戶、麥克風振膜、指示燈。</b><br>
  隔著錄音室的窗戶可以看到麥克風，右上角的指示燈亮著。指示燈平時是萊姆綠，在應用程式裡只有錄音時才會變紅。
</p>
<br clear="left">

**概念是「夜晚的錄音室」。** 帶暖意的深色石墨底色上，亮著設備的 LED 和指示燈。狀態用 LED 的亮滅而不是邊框顏色來表示；除了錄音時以外，畫面不會變紅。

![配色](brand/out/marketing/palette.png)

| 顏色 | 用途 |
|---|---|
| Signal（萊姆綠） | 唱準時的音高線、播放頭、點亮的 LED |
| Reference（冰藍） | 參考的容許帶 |
| Amber / Coral | 稍有偏離 / 偏離較大、警告 |
| Tally（紅） | 僅限錄音時 |

- **字型**：IBM Plex Sans JP；時間、dB 等數字使用 IBM Plex Mono（等寬，數字不會跳動）。中文使用系統字型顯示
- **元件**：附 LED 的鍵帽按鈕、LED 環旋鈕、直立式混音台推桿、分段 LED 音量表
- **動態**：像彈簧一樣按下的按鍵、在 0 dB 有手感的推桿、隨節拍亮起的預備拍。始終以聲音為優先，並遵循系統的「減少動態效果」設定

| 應用程式圖示 | 品牌套件 |
|---|---|
| ![應用程式圖示](brand/out/preview/preview-icons.png) | ![品牌套件](brand/out/preview/preview-brand.png) |

標誌的使用規範（留白、最小尺寸、淺色背景版本）和全部素材見 [`brand/`](brand/README.md)（日文）。

## 系統需求（暫定）

| | 最低 | 建議 |
|---|---|---|
| Windows | Windows 10 64 位元（1607 版以上） | Windows 11 |
| Mac | macOS 11 Big Sur 以上（同時支援 Apple 晶片與 Intel 的通用版） | 最新 macOS、Apple 晶片 |
| CPU | 64 位元、4 核心 | 6 核心以上（Apple M1 以上、近幾年的 Intel Core i5 / AMD Ryzen 5 等級以上） |
| 記憶體 | 8 GB | 16 GB |
| 可用空間 | 2 GB | 10 GB 以上（SSD） |
| 螢幕 | 1280×800 | 1440×900 以上 |
| 音訊 | 內建輸入輸出也能運作 | 錄音介面＋有線耳機（Windows 支援 ASIO 時延遲較低） |
| 網路 | 只有第一次下載分離模型時需要（沒有網路時，除了分離以外都能使用） | — |

- 只有人聲分離比較吃資源。在較舊的 CPU 或 Intel Mac 上分離需要更多時間（將實測後確定）
- 藍牙耳機延遲較大，不適合錄音
- 尚未在 Arm 版 Windows 上測試
- 數值為開發階段的參考。依據見 [`docs/DESIGN.md`](docs/DESIGN.md) 11.6.1（日文）

## 下載

尚未發布。準備好後會發布在本頁面的 Releases 中。

## 支持開發

VoiceBooth 是免費的。如果你喜歡，可以透過 [GitHub Sponsors](https://github.com/sponsors/kajisho5) 支持開發。是否贊助不會影響可用的功能。

## 授權

原始碼採用 **GNU Affero General Public License v3.0 或更新版本（AGPL-3.0-or-later）**（[`LICENSE`](LICENSE)）。由於以 AGPLv3 使用 JUCE 8，整個應用程式採用 AGPL。

- 可以自由使用、修改、散布和販售。散布時（包括透過網路讓他人使用修改版），請以相同授權公開原始碼
- **名稱與標誌**：以修改版作為另一款產品散布時，請不要使用「VoiceBooth」的名稱和標誌（請換成別的名稱和標誌）。原樣再散布，或在介紹、評測中使用都沒有問題
- 你錄製和匯出的音訊檔案屬於你，AGPL 不適用於你的作品

| 包含 / 使用的元件 | 授權 |
|---|---|
| JUCE 8 | AGPLv3 / 商業雙授權（此處使用 AGPLv3） |
| minimp3（`third_party/minimp3`） | CC0 |
| IBM Plex Sans JP / IBM Plex Mono（`resources/fonts`） | SIL Open Font License 1.1 |
| 規劃中：Rubber Band（速度 / 調） | GPL v2 以上 / 商業雙授權（此處使用 GPL） |
| 規劃中：ASIO SDK（Windows） | GPLv3 / 商業雙授權。SDK 本身不放進本儲存庫 |
| 規劃中：分離、音高、歌詞模型 | 只使用已確認權重授權的模型，與應用程式分開散布 |

## 參與開發

建置、測試和專案結構見 [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)，規格見 [`docs/DESIGN.md`](docs/DESIGN.md)（皆為日文）。
