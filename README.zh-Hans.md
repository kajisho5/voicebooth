<p align="center">
  <img src="brand/out/marketing/readme-banner-zh-Hans.png" alt="VoiceBooth — 翻唱专用 DAW。看着修正，交出一轨。" width="100%">
</p>

<p align="center">
  <a href="README.md">日本語</a> ·
  <a href="README.en.md">English</a> ·
  <a href="README.ko.md">한국어</a> ·
  <b>简体中文</b> ·
  <a href="README.zh-Hant.md">繁體中文</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-in%20development-F4B942?labelColor=141311" alt="开发中">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-8CC1EE?labelColor=141311" alt="Windows / macOS">
  <img src="https://img.shields.io/badge/license-AGPL--3.0--or--later-C6EE6A?labelColor=141311" alt="AGPL-3.0-or-later">
</p>

# VoiceBooth

**专为翻唱录音打造的小型人声 DAW。**
跟着伴奏唱，在屏幕上看着音高修正，只重录想改的部分，然后导出混音师可以直接使用的 WAV。只做这些事，不是万能 DAW。

![VoiceBooth 主界面](docs/screenshots/main-zh-Hans.png)

> [!NOTE]
> **开发中，尚未发布。** 目前可用的只有“打开歌曲并查看波形”和“伴奏的播放、跳转、循环”。界面上的数值、歌词和音高都是示例数据。

## 功能

| | 说明 | 状态 |
|---|---|---|
| 打开歌曲 | wav / flac / aiff / ogg / mp3 / m4a，也可拖放打开。歌曲开头的位置在 Windows 和 Mac 上一致 | ✅ 可用 |
| 播放伴奏 | 播放、跳转、区间循环、音量。循环衔接处没有空隙 | ✅ 可用 |
| 输入设备与电平 | 选择麦克风，对照目标范围（-12～-6 dB）调整电平 | 🔧 开发中 |
| 参考音高 | 参考旋律以“容差带”显示，你的声音以线条叠加。唱准时为青柠色，偏离时变为琥珀色→红色 | ⬜ 计划中 |
| 练习 | 速度 50～150%，调 ±6。可以放慢练习，但交付录音始终以原速、原调录制 | ⬜ 计划中 |
| 回溯录音 | 从开始播放起就在后台录音，即使晚按 REC，开头也不会被截掉 | ⬜ 计划中 |
| 分段重录 | 选择范围进行插录（Punch-in），衔接处 8 ms 交叉淡化 | ⬜ 计划中 |
| Main / Double / Harmony | 以相同长度录制叠唱与和声 | ⬜ 计划中 |
| 人声分离 | 从混音音源生成伴奏与参考人声（首次使用时下载模型，中断后可继续下载） | ⬜ 计划中 |
| 5 种语言 | 日本語 / English / 한국어 / 简体中文 / 繁體中文 | ✅ 可用 |

### 交付文件的约定

导出的文件放进 DAW 的那一刻，就与伴奏的开头对齐（目标 ±1 ms）。

- 从歌曲开头（0 秒）到结尾的完整长度，没有录音的部分为静音
- 24bit 单声道 WAV，采样率与原曲相同（不会擅自改为 48 kHz）
- 不做标准化，不加自动淡入淡出；监听用的混响和参考人声不会混入
- 练习录音不会放进交付文件夹

### 三种模式

引擎相同，只是显示的内容不同。在任何模式下创建的项目都能在其他模式中打开。

| 简单 | 标准 | 专业 |
|---|---|---|
| 整首录 Main 后直接交付 | 叠唱、一条和声、分段重录、进入时机 | 两条和声＋左右叠唱、颤音等分析、完整交付包 |

| 简单模式 | 专业模式（和声） |
|---|---|
| ![简单模式](docs/screenshots/mode-easy.png) | ![专业模式和声](docs/screenshots/mode-pro-harmony.png) |

## 标志与设计

<p>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="brand/out/logo/logo-mark-dark.png">
    <img src="brand/out/logo/logo-mark-light.png" alt="VoiceBooth 标志" width="96" align="left">
  </picture>
  <b>标志：录音棚的窗户、麦克风振膜、指示灯。</b><br>
  隔着录音棚的窗户可以看到麦克风，右上角的指示灯亮着。指示灯平时是青柠色，在应用里只有录音时才会变红。
</p>
<br clear="left">

**概念是“夜晚的录音棚”。** 带暖意的深色石墨底色上，亮着设备的 LED 和指示灯。状态用 LED 的亮灭而不是边框颜色来表示；除录音时以外，画面不会变红。

![配色](brand/out/marketing/palette.png)

| 颜色 | 用途 |
|---|---|
| Signal（青柠） | 唱准时的音高线、播放头、点亮的 LED |
| Reference（冰蓝） | 参考的容差带 |
| Amber / Coral | 稍有偏离 / 偏离较大、警告 |
| Tally（红） | 仅限录音时 |

- **字体**：IBM Plex Sans JP；时间、dB 等数字使用 IBM Plex Mono（等宽，数字不会跳动）。中文使用系统字体显示
- **控件**：带 LED 的键帽按钮、LED 环旋钮、竖向调音台推子、分段 LED 电平表
- **动效**：像弹簧一样按下的按键、在 0 dB 有手感的推子、随节拍点亮的预备拍。始终以声音为先，并遵循系统的“减少动态效果”设置

| 应用图标 | 品牌套件 |
|---|---|
| ![应用图标](brand/out/preview/preview-icons.png) | ![品牌套件](brand/out/preview/preview-brand.png) |

标志的使用规范（留白、最小尺寸、浅色背景版本）和全部素材见 [`brand/`](brand/README.md)（日语）。

## 系统要求（暂定）

| | 最低 | 推荐 |
|---|---|---|
| Windows | Windows 10 64 位（1607 版本及以上） | Windows 11 |
| Mac | macOS 11 Big Sur 及以上（同时支持 Apple 芯片与 Intel 的通用版） | 最新 macOS、Apple 芯片 |
| CPU | 64 位、4 核 | 6 核及以上（Apple M1 及以上、近几年的 Intel Core i5 / AMD Ryzen 5 级别及以上） |
| 内存 | 8 GB | 16 GB |
| 可用空间 | 2 GB | 10 GB 以上（SSD） |
| 屏幕 | 1280×800 | 1440×900 及以上 |
| 音频 | 内置输入输出也可运行 | 声卡（音频接口）＋有线耳机（Windows 支持 ASIO 时延迟更低） |
| 网络 | 仅首次下载分离模型时需要（无网络时除分离外都能使用） | — |

- 只有人声分离比较吃资源。在较旧的 CPU 或 Intel Mac 上分离需要更多时间（将实测后确定）
- 蓝牙耳机延迟较大，不适合录音
- 尚未在 Arm 版 Windows 上测试
- 数值为开发阶段的参考。依据见 [`docs/DESIGN.md`](docs/DESIGN.md) 11.6.1（日语）

## 下载

尚未发布。准备好后会发布在本页面的 Releases 中。

## 支持开发

VoiceBooth 是免费的。如果你喜欢，可以通过 [GitHub Sponsors](https://github.com/sponsors/kajisho5) 支持开发。是否赞助不会影响可用的功能。

## 许可证

源代码采用 **GNU Affero General Public License v3.0 或更高版本（AGPL-3.0-or-later）**（[`LICENSE`](LICENSE)）。由于以 AGPLv3 使用 JUCE 8，整个应用采用 AGPL。

- 可以自由使用、修改、分发和出售。分发时（包括通过网络让他人使用修改版），请以相同许可证公开源代码
- **名称与标志**：以修改版作为另一款产品分发时，请不要使用“VoiceBooth”的名称和标志（请换成别的名称和标志）。原样再分发，或在介绍、评测中使用都没有问题
- 你录制和导出的音频文件归你所有，AGPL 不适用于你的作品

| 包含 / 使用的组件 | 许可证 |
|---|---|
| JUCE 8 | AGPLv3 / 商业双许可（此处使用 AGPLv3） |
| minimp3（`third_party/minimp3`） | CC0 |
| IBM Plex Sans JP / IBM Plex Mono（`resources/fonts`） | SIL Open Font License 1.1 |
| 计划：Rubber Band（速度 / 调） | GPL v2 及以上 / 商业双许可（此处使用 GPL） |
| 计划：ASIO SDK（Windows） | GPLv3 / 商业双许可。SDK 本身不放进本仓库 |
| 计划：分离、音高、歌词模型 | 仅使用已确认权重许可证的模型，与应用分开分发 |

## 参与开发

构建、测试和项目结构见 [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)，规格见 [`docs/DESIGN.md`](docs/DESIGN.md)（均为日语）。
