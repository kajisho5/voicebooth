**ベータ版です。** 歌ってみた専用の小さなボーカル DAW。オフボに合わせて歌い、音程を画面で見て直し、直したい所だけ録り直して、ミックス担当にそのまま渡せる WAV を書き出します。

English follows Japanese.

## ダウンロード

| OS | ファイル |
|---|---|
| Windows 10 / 11（64bit） | `VoiceBooth-*-win-x64-setup.exe` |
| macOS 11 以降（Apple シリコン / Intel） | `VoiceBooth-*-mac-universal.dmg` |

- スマホ・タブレット版はありません。グラフィックボードは要りません（CPU だけで動きます）
- 無料です。サブスク・課金はありません

### 初めて開く時（コード署名をしていないため）

- **Windows**：「Windows によって PC が保護されました」→「詳細情報」→「実行」
- **Mac**：DMG の VoiceBooth をアプリケーションフォルダへ入れて一度開く →「開けません」と出たら、システム設定 →「プライバシーとセキュリティ」→「このまま開く」（開こうとしてから約 1 時間だけ出ます）→ パスワード

## 入っているもの

- 手元の音声ファイル（wav / flac / aiff / ogg / mp3 / m4a）を開く。サブスクの曲は直接入れられません
- 声入りの原曲＋カラオケから、お手本の音程の線を作り、自分の音程を色で重ねる（オクターブ違いも合わせて表示）
- 練習用のテンポ 50〜150 %・キー ±6（納品は原速・原キー）
- 通し録音・遡及録音・範囲の録り直し・遅延の測定と補正
- Main / Double / Harmony のトラック、入りタイミング（標準以上）、音程の割合・ビブラート（プロ）
- フル尺 WAV の書き出し、納品パック（WAV・確認用ミックス・メモ・zip）
- 5 言語（日本語 / English / 한국어 / 简体中文 / 繁體中文）、スキン 10 種

## まだのもの・注意

- **分離と歌詞の自動合わせのモデルは配布の準備中です。** それまでは「原曲だけで始める」と歌詞の自動合わせは使えません（原曲＋カラオケの組は使えます）
- お手本の声だけをソロで聴く、ハモリのお手本の線、自分の声域に合うキーの提案、メトロノームはまだありません
- 分離した声・伴奏は個人の練習用です。配布・公開（歌ってみたのオフボとして配るのを含む）は、元の曲の権利者が許している範囲だけにしてください
- 作者の手元の実機での確認はこれからです。おかしな所は [Issues](https://github.com/kajisho5/voicebooth/issues) へどうぞ

---

**This is a beta.** A small vocal DAW for cover singers: sing over the off-vocal track, see your pitch on screen, re-record only the parts you want, and export WAV files your mix engineer can drop straight in.

## Download

| OS | File |
|---|---|
| Windows 10 / 11 (64-bit) | `VoiceBooth-*-win-x64-setup.exe` |
| macOS 11 or later (Apple silicon / Intel) | `VoiceBooth-*-mac-universal.dmg` |

- No phone or tablet version. No graphics card needed (runs on the CPU).
- Free. No subscription, no in-app purchases.

### Opening it the first time (the builds are not code-signed yet)

- **Windows**: "Windows protected your PC" → "More info" → "Run anyway"
- **Mac**: move VoiceBooth from the DMG to Applications and open it once → if it says it can't be opened, go to System Settings → Privacy & Security → "Open Anyway" (shown for about an hour after the attempt) → enter your password

## What's in it

- Opens audio files on your computer (wav / flac / aiff / ogg / mp3 / m4a). Songs from streaming services can't be added directly
- Builds a reference pitch line from the original song plus its karaoke track and colours your own pitch against it (octave-shifted singing is aligned too)
- Practice tempo 50–150 % and key ±6 (deliveries are recorded at the original tempo and key)
- Full-length recording, retroactive recording, range re-recording, latency measurement and compensation
- Main / Double / Harmony tracks, entry timing (Standard and up), pitch accuracy and vibrato (Pro)
- Full-length WAV export and a delivery pack (WAVs, reference mix, notes, zip)
- Five languages and ten built-in skins

## Not yet / please note

- **The separation and lyrics-alignment models are still being prepared for download.** Until then, "start from the original only" and automatic lyrics alignment are unavailable (the original + karaoke pair works).
- Soloing the reference vocal, a harmony reference line, a key suggestion for your vocal range and a metronome are not in this version.
- Separated vocals and backing are for personal practice. Only share or publish them (including as an off-vocal for a cover) where the rights holder of the original song allows it.
- Real-hardware testing is still under way. Please report problems in [Issues](https://github.com/kajisho5/voicebooth/issues).
