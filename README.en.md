<p align="center">
  <img src="brand/out/marketing/readme-banner-en.png" alt="VoiceBooth — a vocal DAW for song covers. See it, fix it, hand over one take." width="100%">
</p>

<p align="center">
  <a href="README.md">日本語</a> ·
  <b>English</b> ·
  <a href="README.ko.md">한국어</a> ·
  <a href="README.zh-Hans.md">简体中文</a> ·
  <a href="README.zh-Hant.md">繁體中文</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-in%20development-F4B942?labelColor=141311" alt="In development">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-8CC1EE?labelColor=141311" alt="Windows / macOS">
  <img src="https://img.shields.io/badge/license-AGPL--3.0--or--later-C6EE6A?labelColor=141311" alt="AGPL-3.0-or-later">
</p>

# VoiceBooth

**A small vocal DAW made only for recording song covers.**
Sing along to an instrumental, see your pitch on screen, re-record just the parts you want to fix, and export a WAV your mix engineer can drop straight into their DAW. That is all it does. It is not a general-purpose DAW.

![VoiceBooth main screen](docs/screenshots/main-en.png)

> [!NOTE]
> **In development. Not released yet.** What works today: opening a song and seeing its waveform, and playing, seeking and looping the instrumental. The numbers, lyrics and pitch on screen are sample data.

## What it does

| | Details | Status |
|---|---|---|
| Open a song | wav / flac / aiff / ogg / mp3 / m4a, also by drag and drop. The start of the song lines up the same on Windows and Mac | ✅ Works |
| Play the instrumental | Play, seek, loop a range, volume. Loops wrap with no gap | ✅ Works |
| Input device and level | Pick your mic and set the level against a target band (-12 to -6 dB) | 🔧 In progress |
| Reference pitch | The reference melody is drawn as a tolerance band, your voice as a line on top. Lime when you are on pitch, amber then red as you drift | ⬜ Planned |
| Original + karaoke | Use the original song (with vocals) as the reference and sing over the karaoke / instrumental. Different intro lengths, slightly different speeds, cut versions and transposed karaoke are aligned automatically, with manual fine-tuning. Delivered files always start at the karaoke's first sample | 🔧 Alignment engine done (screens to come) |
| Practice | Tempo 50–150%, key ±6. Practice slowly, but the delivery take is always recorded at the original tempo and key | ⬜ Planned |
| Retroactive recording | Recording starts quietly when playback starts, so pressing REC late never cuts off the first word | ⬜ Planned |
| Punch-in | Select a range and re-record it, with an 8 ms crossfade at each edge | ⬜ Planned |
| Main / Double / Harmony | Doubles and harmonies recorded to the same length | ⬜ Planned |
| Vocal separation | Make an instrumental and a reference vocal from a full mix (the model is downloaded on first use and resumes if interrupted) | ⬜ Planned |
| 5 languages | 日本語 / English / 한국어 / 简体中文 / 繁體中文 | ✅ Works |
| Skins | Recolour the whole app (10 built-in). Make your own from a template and share it as a `.vbskin` file | ✅ Works |

### What your mix engineer gets

Files are written so they line up with the instrumental the moment they are placed in a DAW (target ±1 ms).

- Full length from the very start of the song (0 s) to the end. Parts you did not record are silence
- 24-bit mono WAV at the song's own sample rate (never silently converted to 48 kHz)
- No normalizing and no automatic fades. Monitor reverb and guide tracks are never mixed in
- Practice takes are kept out of the delivery folder

### Three modes

The engine is the same; only what you see changes. A project made in one mode opens in any other.

| Easy | Standard | Pro |
|---|---|---|
| Record Main in one pass and hand it over | Doubles, one harmony, punch-in, entry timing | Two harmonies plus left/right doubles, vibrato and other analysis, full delivery pack |

| Easy mode | Pro mode (harmony) |
|---|---|
| ![Easy mode](docs/screenshots/mode-easy.png) | ![Pro mode harmony](docs/screenshots/mode-pro-harmony.png) |

## Logo and design

<p>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="brand/out/logo/logo-mark-dark.png">
    <img src="brand/out/logo/logo-mark-light.png" alt="VoiceBooth mark" width="96" align="left">
  </picture>
  <b>The mark: a booth window, a microphone capsule and a tally lamp.</b><br>
  A mic seen through the window of a recording booth, with the lamp lit in the corner. The tally is lime by default; inside the app it turns red only while recording.
</p>
<br clear="left">

**The concept is a recording booth at night.** Warm, dark graphite, lit by equipment LEDs and a tally lamp. States are shown by LEDs lighting up rather than by colored borders, and the screen never turns red except while recording.

![Color palette](brand/out/marketing/palette.png)

| Color | Used for |
|---|---|
| Signal (lime) | Your pitch when it is on, the playhead, lit LEDs |
| Reference (ice blue) | The reference tolerance band |
| Amber / Coral | Slightly off / far off, warnings |
| Tally (red) | Recording only |

- **Type**: IBM Plex Sans JP, with IBM Plex Mono for times, dB and other numbers (fixed width, so digits never jump)
- **Controls**: keycap buttons with LEDs, LED-ring knobs, vertical console faders, segmented LED meters
- **Motion**: keys that spring when pressed, faders with a detent at 0 dB, count-in lights in time with the beat. Audio always comes first, and the OS "reduce motion" setting is respected

**Skins.** Swap all the colours at once (fonts, layout and motion stay the same). There are 10 built-in skins, including Studio Day / Sweet for bright rooms, High Contrast for legibility and Color Safe for colour-vision differences. In Settings → Skin → New, pick a template and change colours one by one or group by group (or borrow a group from another template), then save. Hard-to-read combinations are flagged as you edit (saving is never blocked). Share skins as `.vbskin` files.

![The 10 built-in skins](docs/screenshots/skins/all.png)

| App icon | Brand kit |
|---|---|
| ![App icon](brand/out/preview/preview-icons.png) | ![Brand kit](brand/out/preview/preview-brand.png) |

Logo usage rules (clear space, minimum size, light-background versions) and every asset are in [`brand/`](brand/README.md) (Japanese).

## System requirements (provisional)

| | Minimum | Recommended |
|---|---|---|
| Windows | Windows 10 64-bit (version 1607 or later) | Windows 11 |
| Mac | macOS 11 Big Sur or later (universal build for Apple silicon and Intel) | Latest macOS on Apple silicon |
| CPU | 64-bit, 4 cores | 6 cores or more (Apple M1 or later, a recent Intel Core i5 / AMD Ryzen 5 class or better) |
| Memory | 8 GB | 16 GB |
| Free disk space | 2 GB | 10 GB or more (SSD) |
| Display | 1280×800 | 1440×900 or larger |
| Audio | Built-in input/output works | An audio interface and wired headphones (ASIO on Windows gives lower latency) |
| Internet | Only for the first download of the separation model (everything except separation works offline) | — |

- Only vocal separation is heavy. It takes longer on older CPUs and Intel Macs (to be measured and confirmed)
- Bluetooth earphones and headphones have too much latency for recording
- Windows on Arm has not been tested
- These figures are working estimates during development. The reasoning is in [`docs/DESIGN.md`](docs/DESIGN.md) section 11.6.1 (Japanese)

## Download

Not released yet. When it is ready, builds will be published under Releases on this page.

## Support development

VoiceBooth is free. If you like it, you can support development through [GitHub Sponsors](https://github.com/sponsors/kajisho5). Sponsoring does not unlock any features; everyone gets the same app.

## License

The source code is licensed under the **GNU Affero General Public License v3.0 or later (AGPL-3.0-or-later)** ([`LICENSE`](LICENSE)). VoiceBooth uses JUCE 8 under AGPLv3, so the whole app is AGPL.

- You are free to use, modify, share and sell it. When you distribute it (including letting people use a modified version over a network), publish the source under the same license
- **Name and logo**: please do not use the "VoiceBooth" name or logo for a modified version distributed as a separate product (use your own name and logo). Redistributing it unchanged, and using them in introductions or reviews, is fine
- Audio you record and export is yours. The AGPL does not apply to your work

| Included / used | License |
|---|---|
| JUCE 8 | Dual AGPLv3 / commercial (used here under AGPLv3) |
| minimp3 (`third_party/minimp3`) | CC0 |
| IBM Plex Sans JP / IBM Plex Mono (`resources/fonts`) | SIL Open Font License 1.1 |
| Planned: Rubber Band (tempo / key) | Dual GPL v2 or later / commercial (used here under GPL) |
| Planned: ASIO SDK (Windows) | Dual GPLv3 / commercial. The SDK itself is not kept in this repository |
| Planned: separation, pitch and lyrics models | Only models whose weight licenses have been checked. Distributed separately from the app |

## Contributing

Build, test and project layout are in [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md), and the specification is [`docs/DESIGN.md`](docs/DESIGN.md) (both in Japanese).
