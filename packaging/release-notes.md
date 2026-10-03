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

## 0.2.0 で増えたもの

- **分離のモデルを配布開始**：「分離モデルを入れる」を押すと、分離（BS-RoFormer ft1）・リードボーカル（BS-RoFormer karaoke）・音程（RMVPE）を署名を確かめてダウンロード（約 210 MB。押した時だけ）。原曲だけで始める・引けない組の分離が使えます
- **ハモリのお手本**：分離した声をリードとハモリに分け、ハモリのトラックではハモリの線と比べる。お手本のハモリの声も聴ける
- **ピアノロール**：左に鍵盤、半音ごとの段、お手本の音を段に揃えた音符の棒と音名。Ctrl / ⌘＋ホイールで横に拡大
- **キー違いのカラオケ**：分離した声の線と声をカラオケのキーにずらして重ねる
- **時間合わせの手直し**：ピッチレーンを右クリックで ±1 / ±10 ms、「ここで合わせる」。「原曲で聴く」でカラオケと聞き比べ。カット版は原曲に無い所に「お手本なし」
- 範囲の端をつまんで動かす（拍・区間の頭に吸い付く）、書き出し前の「録っていない所」の確認、トラックの音量・M / S と練習のテンポ・キーをプロジェクトに保存、「このアプリについて」（ライセンス）
- **1920x1080 で読みやすく**：全画面の文字を大きくした（以前は字の実寸が 7〜9 px）。設定画面はデジタル庁デザインシステムの目安（本文 16 px・補足 14 px）。1920x1080 の画面では窓を 1760x990 で開く
- **分離モデルをいつでも入れられる**：起動画面の［分離モデルを入れる…］と、設定の「分離モデル」の行（入っているか・受け取り中の進み具合も出る）
- 直したもの：録音中 Esc →「破棄する」でテイクが残っていた、曲を開かずに起動すると見本の画面が出ていた、ほか

## 入っているもの

- 手元の音声ファイル（wav / flac / aiff / ogg / mp3 / m4a）を開く。サブスクの曲は直接入れられません
- 声入りの原曲＋カラオケから、お手本の音程の線を作り、自分の音程を色で重ねる（オクターブ違いも合わせて表示）
- 練習用のテンポ 50〜150 %・キー ±6（納品は原速・原キー）
- お手本の声を聴く（原曲から取り出した声。オフボと一緒に・ソロで）
- 声域を測って、お手本の最低音・最高音が収まるキーを提案（収まらない時は何半音はみ出すかも）
- 通し録音・遡及録音・範囲の録り直し・遅延の測定と補正
- 曲のテンポで鳴るクリック（練習のテンポにも合う）と、録音の前のカウントイン（耳だけで、録音には入りません）
- Main / Double / Harmony のトラック、入りタイミング（標準以上）、音程の割合・ビブラート（プロ）
- テイク比較（標準以上）：録ったテイクを範囲に差し替えて曲の中で聴き比べ、選んだテイクを使う
- フル尺 WAV の書き出し、納品パック（WAV・確認用ミックス・メモ・zip）
- 12 言語（日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français）、スキン 10 種

## まだのもの・注意

- 作者の手元の実機（Windows / Mac）での確認はこれからです。おかしな所は [Issues](https://github.com/kajisho5/voicebooth/issues) へどうぞ
- 歌詞の自動合わせ（音声認識）は入れていません（.txt / .lrc の読み込みとタップで合わせるは使えます）
- 速さの違う版の原曲は、お手本の線を出しません（同じ速さの版を使ってください）
- 分離した声・伴奏は個人の練習用です。配布・公開（歌ってみたのオフボとして配るのを含む）は、元の曲の権利者が許している範囲だけにしてください

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

## New in 0.2.0

- **Separation models are now available**: "Install separation model" downloads separation (BS-RoFormer ft1), lead vocal (BS-RoFormer karaoke) and pitch (RMVPE) after checking their signature (about 210 MB, only when you press it). "Start from the original only" and separation for pairs that can't be subtracted now work
- **Harmony guide**: the separated vocal is split into lead and harmony; harmony tracks are compared with the harmony line, and you can hear the harmony guide
- **Piano roll**: a keyboard on the left, semitone rows, guide notes as bars on the rows with note names. Ctrl / ⌘ + wheel zooms horizontally
- **Karaoke in a different key**: the separated guide line and vocal are shifted to the karaoke key
- **Fixing the alignment**: right-click the pitch lane for ±1 / ±10 ms and "Align here"; "Hear original" to A/B against the karaoke; cut versions show "No guide" where the original has no match
- Drag range edges (snaps to beats and section starts), a check for unrecorded parts before export, track volume / M / S and practice tempo / key saved in the project, an About / license screen
- **Readable at 1920x1080**: text is larger on every screen (it used to be 7–9 px tall). Settings follows the Japanese Digital Agency design system (16 px body, 14 px notes). On a 1920x1080 screen the window opens at 1760x990
- **Install the separation models any time**: "Get the separation model…" on the start screen and a "Separation models" row in Settings (shows whether they are installed and the download progress)
- Fixes: Esc → "Discard" during recording kept the take; launching without a song showed the demo screen; and more

## What's in it

- Opens audio files on your computer (wav / flac / aiff / ogg / mp3 / m4a). Songs from streaming services can't be added directly
- Builds a reference pitch line from the original song plus its karaoke track and colours your own pitch against it (octave-shifted singing is aligned too)
- Practice tempo 50–150 % and key ±6 (deliveries are recorded at the original tempo and key)
- Hear the guide vocal extracted from the original, with the backing or solo
- Measure your vocal range and get a key that fits the guide's lowest and highest notes (or how many semitones stick out if none fits)
- Full-length recording, retroactive recording, range re-recording, latency measurement and compensation
- A click on the song's beat (it follows the practice tempo) and a count-in before recording (headphones only, never recorded)
- Main / Double / Harmony tracks, entry timing (Standard and up), pitch accuracy and vibrato (Pro)
- Take comparison (Standard and up): hear each take in place for a range and use the one you pick
- Full-length WAV export and a delivery pack (WAVs, reference mix, notes, zip)
- Twelve languages (Japanese, English, Korean, Simplified and Traditional Chinese, Spanish, Portuguese (Brazil), Indonesian, Vietnamese, Turkish, German, French) and ten built-in skins

## Not yet / please note

- Not yet tested on the author's own Windows / Mac machines. Please report problems in [Issues](https://github.com/kajisho5/voicebooth/issues)
- Automatic lyric alignment (speech recognition) is not included (loading .txt / .lrc and tap-to-sync work)
- An original at a different speed gets no guide line (please use versions at the same speed)
- Separated vocals and backing tracks are for personal practice. Distribute or publish them (including as an off-vocal track for covers) only as far as the rights holders of the original song allow
