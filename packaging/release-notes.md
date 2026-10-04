**ベータ版です。** 歌ってみた専用の小さなボーカル DAW。オフボに合わせて歌い、音程を画面で見て直し、直したいところだけ録り直して、ミックス担当にそのまま渡せる WAV を書き出します。

English follows Japanese.

## ダウンロード

| OS | ファイル |
|---|---|
| Windows 10 / 11（64bit） | `VoiceBooth-*-win-x64-setup.exe` |
| macOS 11 以降（Apple シリコン / Intel） | `VoiceBooth-*-mac-universal.dmg` |

- スマホ・タブレット版はありません。グラフィックボードは要りません（CPU だけで動きます）
- 無料です。サブスク・課金はありません

### 初めて開くとき（コード署名をしていないため）

- **Windows**：「Windows によって PC が保護されました」→「詳細情報」→「実行」
- **Mac**：DMG の VoiceBooth をアプリケーションフォルダへ入れて一度開く →「開けません」と表示されたら、システム設定 →「プライバシーとセキュリティ」→「このまま開く」（開こうとしてから約 1 時間だけ表示されます）→ パスワード

## beta.4 で増えたもの・直したもの

- **分離中の画面が分かりやすく**：分離を始めてから残り時間が表示されるまでの間は「準備中・経過時間」と動くバーを表示します。読み込みが終わると「メイン画面へ進む」の案内を表示し、Enter でも進めます
- **リードとハモリの分離を別の行に**：起動画面で、声 / オフボの分離と、リード / ハモリの分離を分けて表示します。メイン画面に移ってから自動で始まる項目は「メイン画面で自動」と表示し、状態バーでも「リード・ハモリを分離中」と表示します
- **待っている間に遊べるゲーム**：原曲から分離している間に、［音程あて］（表示された音を声で当てる）・［リズムタップ］（クリックに合わせて Space）・［おまかせ］で遊べます。遊ぶかは自由です
- **ハモリの線がないときの理由**：ハモリのトラックを選んでもハモリの線がないとき、「作成中」「リードボーカルのモデルが必要」「ハモリの声がほとんど見つからない」などの理由を表示します（それまではメインの線を表示します）
- **状態バーのずれ**：左下の音量（dBFS）の数字で状態バー全体が左右に動いていたのを直しました

## beta.3 で直したもの

- **分離モデルをダウンロードできなかった（Windows・Mac）**：beta.2 では「分離モデルの一覧を読めませんでした」と表示されて先に進めませんでした。一覧が最後まで届いたかどうかの判定を直しました
- **小さい画面でも収まる**：ウィンドウの最小を 1180x640 にしました（1366x768・1280x720 の画面でも収まります）。右のラックは高さが足りないと縦にスクロールし、書き出しなどの画面は収まるように縮めて表示します
- **文字の切れ**：ベトナム語・トルコ語の字が大きすぎて切れていたのを直しました。フェーダーの注記と名前、テイク比較の列、設定のキーの幅も直しました
- **お手本を差し替えたとき**、ピアノロールの音符とキーの提案が古いままだったのを直しました
- **4 GB を超える納品パック**は zip を作らず、フォルダのまま渡してお知らせします（壊れた zip ができていました）
- **プロジェクトのフォルダの外を指すテイク**は読み込まず、お知らせします
- **大きい曲を開いたとき**、メモリが足りなくなりそうならお知らせします
- **再生中を軽く**：ピッチと波形のレーンは再生位置の前後だけを描き直します。分離はほかの処理より優先度を下げて動かします（録音・再生と CPU を取り合いません）
- **翻訳**：お知らせに混ざっていた英語を 12 言語に訳しました。Mac のマイクの許可の文も 12 言語にしました。分離のエラーの文字化けを直し、訳語の揺れと複数形の書き方もそろえました
- **ライセンス**：同梱しているライブラリのライセンス全文をアプリに含め、「このアプリについて」から開けるようにしました

## 0.2.0 で増えたもの

- **分離モデルの配布を開始**：分離（BS-RoFormer ft1）・リードボーカル（BS-RoFormer karaoke）・音程（RMVPE）のモデルを、署名を確認してからダウンロード（約 210 MB）。「原曲だけで始める」と、引き算で声を取り出せない組み合わせの分離が使えます
- **ハモリのお手本**：分離した声をリードとハモリに分け、ハモリのトラックではハモリの線と比べます。お手本のハモリの声も聴けます
- **ピアノロール**：左に鍵盤、半音ごとの段、お手本の音を段にそろえた音符の棒と音名。Ctrl / ⌘＋ホイールで横に拡大
- **キー違いのカラオケ**：分離した声の線と声をカラオケのキーに合わせて重ねます
- **時間合わせの手直し**：ピッチレーンを右クリックで ±1 / ±10 ms、「ここで時間を合わせる」。「原曲で聴く」でカラオケと聞き比べ。カット版は原曲にない部分に「お手本なし」を表示
- 範囲の端をドラッグして動かす（拍・区間の頭に吸着）、書き出し前の「未録音の箇所」の確認、トラックの音量・M / S と練習のテンポ・キーをプロジェクトに保存、「このアプリについて」（ライセンス）
- **1920x1080 で読みやすく**：全画面の文字を大きくしました（以前は文字の実寸が 7〜9 px）。設定画面はデジタル庁デザインシステムの目安（本文 16 px・補足 14 px）。1920x1080 の画面ではウィンドウを 1760x990 で開きます
- **README の一番上にダウンロードのボタン**：Windows / Mac のファイルを直接保存でき、初めての人向けの 3 ステップ（保存 → 警告が出たら → 分離モデルをダウンロード）を追加（12 言語）
- **起動時に分離モデルのダウンロードを案内**：まだダウンロードしていなければ、起動時（初回は言語とモードを選んだ後）に確認を表示します（［ダウンロード］を押したときだけダウンロード。「あとで」も選べます）
- **分離モデルをいつでもダウンロードできる**：起動画面の［分離モデルをダウンロード…］と、設定の「分離モデル」の行（ダウンロード済みかどうかと、ダウンロード中の進み具合も表示）
- 直したもの：録音中 Esc →「破棄する」でテイクが残っていた、曲を開かずに起動すると見本の画面が表示されていた、ほか

## 入っているもの

- 手元の音声ファイル（wav / flac / aiff / ogg / mp3 / m4a）を開く。サブスクの曲は直接読み込めません
- 声入りの原曲＋カラオケから、お手本の音程の線を作り、自分の音程を色で重ねる（オクターブ違いも合わせて表示）
- 練習用のテンポ 50〜150 %・キー ±6（納品は原速・原キー）
- お手本の声を聴く（原曲から取り出した声。オフボと一緒に・ソロで）
- 声域を測って、お手本の最低音・最高音が収まるキーを提案（収まらないときは何半音はみ出すかも表示）
- 通し録音・遡及録音・範囲の録り直し・遅延の測定と補正
- 曲のテンポで鳴るクリック（練習のテンポにも合う）と、録音の前のカウントイン（モニターで聞こえるだけで、録音には入りません）
- Main / Double / Harmony のトラック、入りタイミング（標準以上）、音程の割合・ビブラート（プロ）
- テイク比較（標準以上）：録ったテイクを範囲に差し替えて曲の中で聴き比べ、選んだテイクを使う
- フル尺 WAV の書き出し、納品パック（WAV・確認用ミックス・メモ・zip）
- 12 言語（日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français）、スキン 10 種

## まだのもの・注意

- 作者の手元の実機（Windows / Mac）での確認はこれからです。おかしなところは [Issues](https://github.com/kajisho5/voicebooth/issues) へどうぞ
- 歌詞の自動合わせ（音声認識）は入れていません（.txt / .lrc の読み込みと、タップでの時刻合わせは使えます）
- 速さの違うバージョンの原曲は、お手本の線を表示しません（同じ速さのバージョンを使ってください）
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

## New and fixed in beta 4

- **Clearer separation screen**: between starting separation and the first time estimate, it now shows "Preparing" with the elapsed time and a moving bar. When loading is done it shows how to continue to the main screen, and Enter also continues
- **Lead / harmony separation in its own row**: the start screen shows voice / backing separation and lead / harmony separation separately. Items that start automatically on the main screen are marked as such, and the status bar says when lead / harmony separation is running
- **Games while you wait**: while separating from the original song you can play Pitch match (sing the note shown), Rhythm tap (press Space with the click) or Random. Playing is optional
- **Why there is no harmony line**: when a harmony track is selected but there is no harmony line, the pitch lane says why (still building, lead vocal model needed, almost no harmony found). The main line is shown until then
- **Status bar shift**: the level (dBFS) number at the bottom left no longer shifts the whole status bar

## Fixed in beta 3

- **Separation models could not be downloaded (Windows, Mac)**: beta 2 stopped with "Couldn't read the model list". The check for whether the list arrived in full is fixed
- **Fits on small screens**: the minimum window size is now 1180x640 (fits 1366x768 and 1280x720 screens). The right rack scrolls when it is too short, and screens such as Export shrink to fit
- **Cut-off text**: Vietnamese and Turkish text was drawn too large and got cut off. Fader notes and names, the take comparison columns and the key widths in Settings are fixed too
- **Replacing the guide**: the piano-roll notes and the key suggestion now follow the new guide
- **Delivery packs over 4 GB** stay as a folder without a zip, with a notice (a broken zip used to be created)
- **Takes pointing outside the project folder** are not loaded, with a notice
- **Large songs**: a notice appears when opening one may run short of memory
- **Lighter playback**: the pitch and waveform lanes only redraw around the playhead, and separation runs at a lower priority so it doesn't compete with recording and playback
- **Translations**: English fragments in notices are translated into all 12 languages, the Mac microphone permission text is in 12 languages, garbled separation errors are fixed, and terms and plurals are consistent
- **Licenses**: the full license texts of the bundled libraries are included and can be opened from About

## New in 0.2.0

- **Separation models are now available**: "Install separation model" downloads separation (BS-RoFormer ft1), lead vocal (BS-RoFormer karaoke) and pitch (RMVPE) after checking their signature (about 210 MB). "Start from the original only" and separation for pairs that can't be subtracted now work
- **Harmony guide**: the separated vocal is split into lead and harmony; harmony tracks are compared with the harmony line, and you can hear the harmony guide
- **Piano roll**: a keyboard on the left, semitone rows, guide notes as bars on the rows with note names. Ctrl / ⌘ + wheel zooms horizontally
- **Karaoke in a different key**: the separated guide line and vocal are shifted to the karaoke key
- **Fixing the alignment**: right-click the pitch lane for ±1 / ±10 ms and "Align here"; "Hear original" to A/B against the karaoke; cut versions show "No guide" where the original has no match
- Drag range edges (snaps to beats and section starts), a check for unrecorded parts before export, track volume / M / S and practice tempo / key saved in the project, an About / license screen
- **Readable at 1920x1080**: text is larger on every screen (it used to be 7–9 px tall). Settings follows the Japanese Digital Agency design system (16 px body, 14 px notes). On a 1920x1080 screen the window opens at 1760x990
- **Download buttons at the top of the README**: save the Windows / Mac file directly, with three steps for first-timers (save → if you see a warning → install the separation models), in twelve languages
- **Separation models are offered at startup**: if they are missing, a download prompt appears at startup (the first time, after choosing the language and mode). They are downloaded only when you press Download; you can also choose Later
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
