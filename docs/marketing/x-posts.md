# X（旧 Twitter）の紹介ポスト

v0.2.0-beta.2 の公開に合わせて作った紹介文。言語ごとに本文 1 本と、返信でつなげるツリー 9 本。

- 作った日：2026-10-03（v0.2.0-beta.2）
- コピー用のページ：https://claude.ai/artifact/UurHfP86cjTQMnjYPsW64Y （持ち主だけが開ける）
- X Premium の長文投稿が前提（無料アカウントは 280 字、日本語などは 140 字まで）
- 本文には、ピッチの線が見える画面（`docs/screenshots/main-ja.png` など）を付ける
- 設定の項目名とモード名は、アプリの翻訳ファイル（`resources/i18n/`）に合わせている。アプリの言葉を変えたら、ここも直す
- 機能・数字（テンポ 50〜150%、キー ±6、モデル約 210 MB、12 言語、スキン 10 種など）を変えたら、ここも直す

- [日本語](#ja)
- [English](#en)
- [한국어](#ko)
- [简体中文](#zh-hans)
- [繁體中文](#zh-hant)
- [Español](#es)
- [Português (Brasil)](#pt-br)
- [Bahasa Indonesia](#id)
- [Tiếng Việt](#vi)
- [Türkçe](#tr)
- [Deutsch](#de)
- [Français](#fr)

<a id="ja"></a>

## 日本語（ja）

リンク先：https://github.com/kajisho5/voicebooth

### 本文：本文

```text
【無料】歌ってみた用の練習＆録音アプリ「VoiceBooth」を作りました🎤

原曲とオフボを読み込むと、お手本のメロディ（ピッチ）が画面に表示され、歌うと自分の声の音程が線で重なります。
合っていればライム、ズレるとオレンジ→赤。どこで外れているかを目で見ながら練習できます。

・原曲からボーカルの音程を自動で取り出して、ピアノロールに表示
・原曲しかなくても、AI でボーカルとオフボに分けられる
・テンポ 50〜150%、キー ±6 で練習
・声域を測って、おすすめのキーを提案
・録ったら、MIX 師さんにそのまま渡せる WAV で書き出し
・12 言語に対応

Windows / Mac 対応、無料（ベータ版）
👇ダウンロードはこちら
https://github.com/kajisho5/voicebooth

#歌ってみた #歌い手 #ボイトレ
```

### ツリー 1：お手本の音程

```text
🎼 お手本の音程の表示

声入りの原曲とオフボを読み込むと、原曲から歌声だけを取り出して、ピアノロールに音符の棒と音名で表示します。
前奏の長さが違っても、時間は自動で合わせます。
キー違いのカラオケでも、カラオケのキーに合わせて表示します。
オクターブ違いで歌っても、ちゃんと合わせて判定します。
```

### ツリー 2：原曲しかないとき

```text
🎧 原曲しかないとき

AI のボーカル分離で、歌とオフボに分けられます（モデルは初回だけ約 210 MB をダウンロード）。
メインとハモリも分けるので、ハモリのお手本の線も表示できます。

※分離した音は個人の練習用です。配布や公開は、元の曲の権利者が許している範囲だけにしてください
```

### ツリー 3：練習

```text
🎤 練習のための機能

・テンポ 50〜150%、キー ±6（本番の録音は原速・原キーに戻して録る）
・範囲を決めてループ
・お手本の声をソロでもオフボと一緒にでも聴ける
・マイクで声域を測ると、お手本が収まるキーを提案。収まらないときは何半音はみ出すかも表示
```

### ツリー 4：録音

```text
⏺ 録音

・遡及録音：REC を押し遅れても、歌い出しが欠けない
・範囲の録り直し（パンチイン）。つなぎ目はクロスフェード
・遅延の測定と自動補正
・クリックとカウントイン（モニターのみで、録音には入らない）
・Main / Double / Harmony のトラック
・テイク比較：録ったテイクを曲の中で聴き比べて、良いものを選ぶ
・入りのタイミングが何 ms 早い・遅いかを表示
```

### ツリー 5：MIX 師さんへ

```text
📦 MIX 師さんへの渡し方

・曲の頭からのフル尺 WAV（24bit、元のサンプリングレートのまま、ノーマライズなし）
・納品パック：トラックごとの WAV、確認用のミックス、メモ、zip

DAW に置けばオフボと頭がそろうように書き出すので、そのまま渡せます。
```

### ツリー 6：モード

```text
🎛 3 つのモード

・簡単：Main を通しで録って渡すだけ
・標準：ダブル、ハモリ 1 本、区間の録り直し、テイク比較、納品パック
・プロ：ハモリ 2 本、音程の正確さやビブラートの解析

スキンは 10 種類。好みの見た目に着せ替えできます。
```

### ツリー 7：言語

```text
🌏 12 言語に対応

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・初めて起動したときは、パソコンの表示言語に合わせた言語で開きます（対応していない言語なら英語）
・最初の画面で言語を選び直せます
・あとから設定の「表示言語」でいつでも切り替えられます。再起動は不要で、すぐに切り替わります
・ダウンロードページの説明（README）も 12 言語あります

海外の歌い手さん・MIX 師さんにもそのまま紹介できます。
```

### ツリー 8：動作環境

```text
💻 動作環境と注意

・Windows 10 / 11、macOS 11 以降（Apple シリコン / Intel）
・グラボは不要、CPU だけで動きます
・スマホ版はありません
・読み込めるのは手元の音声ファイル（wav / mp3 / m4a など）。サブスクの曲は直接読み込めません
・ベータ版でまだ署名していないため、初めて開くときに OS の警告が表示されます。開き方は README に書いています

無料・オープンソースです。不具合や要望は GitHub の Issues へどうぞ🙏
```

### ツリー 9：支援

```text
💚 開発の支援

VoiceBooth は無料で、サブスクや課金はありません。
気に入ったら GitHub Sponsors で開発を支援していただけるとうれしいです。支援の有無で使える機能は変わりません。
https://github.com/sponsors/kajisho5

アプリの設定の「開発を支援する」と、GitHub のページの Sponsor ボタンからも開けます。
```

<a id="en"></a>

## English（en）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.en.md

### 本文：Main

```text
[Free] I made VoiceBooth, a practice & recording app for song covers 🎤

Load the original song and its off-vocal (karaoke) track, and the guide melody (pitch) appears on screen. When you sing, your own pitch is drawn on top as a line.
Lime when you're on, orange → red when you drift. You can see exactly where you go off while you practice.

・Pulls the vocal pitch out of the original automatically and shows it on a piano roll
・Only have the original? AI splits it into vocals and backing
・Practice at 50–150% tempo and ±6 keys
・Measures your vocal range and suggests a key
・Exports WAV files your mix engineer can use as-is
・Available in 12 languages

Windows / Mac, free (beta)
👇 Download
https://github.com/kajisho5/voicebooth/blob/main/README.en.md

#SongCover #Singing #Vocals
```

### ツリー 1：Guide pitch

```text
🎼 The guide pitch

Load the original (with vocals) and the off-vocal track. VoiceBooth pulls out just the singing voice and shows it on a piano roll as note bars with note names.
Different intro lengths are lined up automatically.
If the karaoke is in a different key, the guide follows the karaoke's key.
Sing an octave up or down and it is still judged correctly.
```

### ツリー 2：Original only

```text
🎧 When you only have the original

AI vocal separation splits it into vocals and backing (the models, about 210 MB, download once on first use).
It also separates lead and harmony, so you get a guide line for the harmony too.

※ Separated audio is for personal practice. Share or publish it only as far as the rights holders of the original song allow.
```

### ツリー 3：Practice

```text
🎤 Practice tools

・Tempo 50–150%, key ±6 (final takes are recorded at the original tempo and key)
・Loop a range
・Hear the guide vocal solo or with the backing
・Measure your range with your mic and get a key that fits the guide. If none fits, it shows how many semitones stick out
```

### ツリー 4：Recording

```text
⏺ Recording

・Retroactive recording: hit REC late and the start of the phrase is still there
・Punch-in re-recording of a range, with crossfades at the joins
・Latency measurement and automatic compensation
・Click and count-in (monitor only, never recorded)
・Main / Double / Harmony tracks
・Take comparison: hear your takes in the song and pick the best one
・Shows how many ms early or late your entries are
```

### ツリー 5：Hand-off

```text
📦 Handing off to your mix engineer

・Full-length WAV from the top of the song (24-bit, original sample rate, no normalization)
・Delivery pack: a WAV per track, a reference mix, notes, all zipped

Drop the files into a DAW and they line up with the off-vocal, so you can send them as they are.
```

### ツリー 6：Modes

```text
🎛 Three modes

・Easy: record Main straight through and hand it off
・Standard: double, one harmony, section re-records, take comparison, delivery pack
・Pro: two harmonies, pitch accuracy and vibrato analysis

10 skins to change the look.
```

### ツリー 7：Languages

```text
🌏 12 languages

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・On first launch it opens in your computer's display language (English if that language isn't supported)
・You can pick another language on the first screen
・Switch any time in Settings → Language. No restart needed, it changes right away
・The download page (README) is available in all 12 languages too
```

### ツリー 8：Requirements

```text
💻 Requirements & notes

・Windows 10 / 11, macOS 11 or later (Apple silicon / Intel)
・No graphics card needed, runs on the CPU
・No phone or tablet version
・Opens audio files on your computer (wav / mp3 / m4a, etc.). Songs from streaming services can't be loaded directly
・It's a beta and not code-signed yet, so your OS shows a warning the first time you open it. The README explains how to open it

Free and open source. Bugs and ideas are welcome in GitHub Issues 🙏
```

### ツリー 9：Support

```text
💚 Support development

VoiceBooth is free, with no subscription and no in-app purchases.
If you like it, you can support development on GitHub Sponsors. Every feature is the same whether you support or not.
https://github.com/sponsors/kajisho5

You can also get there from Settings → Support development in the app, or the Sponsor button on the GitHub page.
```

<a id="ko"></a>

## 한국어（ko）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.ko.md

### 本文：본문

```text
[무료] 커버곡 연습 & 녹음 앱 「VoiceBooth」를 만들었습니다 🎤

원곡과 MR(반주)을 불러오면 가이드 멜로디(음정)가 화면에 표시되고, 노래하면 내 목소리의 음정이 선으로 겹쳐집니다.
맞으면 라임색, 어긋나면 주황 → 빨강. 어디서 음이 벗어나는지 눈으로 보면서 연습할 수 있습니다.

・원곡에서 보컬 음정을 자동으로 추출해 피아노 롤에 표시
・원곡만 있어도 AI로 보컬과 MR을 분리
・템포 50~150%, 키 ±6으로 연습
・음역을 측정해 추천 키를 제안
・녹음한 뒤 믹스 엔지니어에게 그대로 넘길 수 있는 WAV로 내보내기
・12개 언어 지원

Windows / Mac 지원, 무료(베타)
👇 다운로드
https://github.com/kajisho5/voicebooth/blob/main/README.ko.md

#커버곡 #노래연습 #보컬
```

### ツリー 1：가이드 음정

```text
🎼 가이드 음정 표시

보컬이 들어간 원곡과 MR을 불러오면 원곡에서 노랫소리만 추출해 피아노 롤에 음표 막대와 음이름으로 표시합니다.
전주 길이가 달라도 시간은 자동으로 맞춥니다.
키가 다른 MR이어도 MR의 키에 맞춰 표시합니다.
옥타브를 바꿔 불러도 제대로 맞춰서 판정합니다.
```

### ツリー 2：원곡만 있을 때

```text
🎧 원곡만 있을 때

AI 보컬 분리로 노래와 MR을 나눌 수 있습니다(모델은 처음 한 번만 약 210MB 다운로드).
메인과 화음도 나누기 때문에 화음 가이드 선도 표시할 수 있습니다.

※ 분리한 음원은 개인 연습용입니다. 배포·공개는 원곡 권리자가 허락한 범위 안에서만 해 주세요.
```

### ツリー 3：연습

```text
🎤 연습 기능

・템포 50~150%, 키 ±6(본 녹음은 원래 템포·원키로 돌려서 녹음)
・구간을 정해서 반복
・가이드 보컬을 솔로로도, MR과 함께도 들을 수 있음
・마이크로 음역을 측정하면 가이드가 들어가는 키를 제안. 맞는 키가 없으면 몇 반음 벗어나는지도 표시
```

### ツリー 4：녹음

```text
⏺ 녹음

・소급 녹음: REC를 늦게 눌러도 첫 소절이 잘리지 않음
・구간 재녹음(펀치 인). 이음매는 크로스페이드
・지연 측정과 자동 보정
・클릭과 카운트인(모니터 전용, 녹음에는 안 들어감)
・Main / Double / Harmony 트랙
・테이크 비교: 녹음한 테이크를 곡 안에서 비교해 듣고 좋은 것을 선택
・들어가는 타이밍이 몇 ms 빠르거나 늦은지 표시
```

### ツリー 5：믹스 전달

```text
📦 믹스 엔지니어에게 넘기기

・곡 처음부터의 풀 길이 WAV(24bit, 원래 샘플레이트 그대로, 노멀라이즈 없음)
・납품 팩: 트랙별 WAV, 확인용 믹스, 메모, zip

DAW에 놓으면 MR과 시작이 맞도록 내보내므로 그대로 넘길 수 있습니다.
```

### ツリー 6：모드

```text
🎛 3가지 모드

・간단: Main을 처음부터 끝까지 녹음해서 넘기기만
・표준: 더블, 화음 1개, 구간 재녹음, 테이크 비교, 납품 팩
・프로: 화음 2개, 음정 정확도와 비브라토 분석

스킨은 10종류. 원하는 모습으로 바꿀 수 있습니다.
```

### ツリー 7：언어

```text
🌏 12개 언어 지원

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・처음 실행하면 컴퓨터의 표시 언어로 열립니다(지원하지 않는 언어라면 영어)
・첫 화면에서 언어를 다시 고를 수 있습니다
・나중에 설정의 「언어」에서 언제든 바꿀 수 있습니다. 재시작 없이 바로 바뀝니다
・다운로드 페이지 설명(README)도 12개 언어로 있습니다
```

### ツリー 8：사용 환경

```text
💻 사용 환경과 주의

・Windows 10 / 11, macOS 11 이상(Apple 실리콘 / Intel)
・그래픽카드 불필요, CPU만으로 동작
・스마트폰 버전은 없습니다
・불러올 수 있는 것은 내 컴퓨터의 오디오 파일(wav / mp3 / m4a 등). 스트리밍 서비스의 곡은 직접 불러올 수 없습니다
・베타 버전이고 아직 서명하지 않아서 처음 열 때 OS 경고가 표시됩니다. 여는 방법은 README에 있습니다

무료·오픈소스입니다. 버그나 요청은 GitHub Issues로 부탁드립니다 🙏
```

### ツリー 9：후원

```text
💚 개발 후원

VoiceBooth는 무료이며 구독이나 결제가 없습니다.
마음에 드셨다면 GitHub Sponsors로 개발을 후원해 주시면 기쁘겠습니다. 후원 여부와 관계없이 기능은 같습니다.
https://github.com/sponsors/kajisho5

앱 설정의 「개발 후원」과 GitHub 페이지의 Sponsor 버튼에서도 열 수 있습니다.
```

<a id="zh-hans"></a>

## 简体中文（zh-Hans）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.zh-Hans.md

### 本文：正文

```text
【免费】我做了一款翻唱用的练习 & 录音软件「VoiceBooth」🎤

导入原曲和伴奏后，示范旋律（音高）会显示在画面上，你唱的时候，自己的音高会以线条叠加在上面。
唱准是青柠色，跑调就变橙色 → 红色。可以边看边练，清楚知道哪里唱偏了。

・自动从原曲提取人声音高，显示在钢琴卷帘上
・只有原曲也没关系，AI 可以分离出人声和伴奏
・速度 50～150%、调 ±6 练习
・测量你的音域，推荐合适的调
・录完后导出 WAV，可以直接交给混音师
・支持 12 种语言

支持 Windows / Mac，免费（测试版）
👇 下载
https://github.com/kajisho5/voicebooth/blob/main/README.zh-Hans.md

#翻唱 #唱歌 #声乐练习
```

### ツリー 1：示范音高

```text
🎼 示范音高的显示

导入带人声的原曲和伴奏，软件会从原曲中只提取歌声，在钢琴卷帘上用音符条和音名显示。
前奏长度不同也会自动对齐时间。
伴奏的调和原曲不同时，也会按伴奏的调显示。
高八度或低八度唱，也能正确对齐判定。
```

### ツリー 2：只有原曲时

```text
🎧 只有原曲时

用 AI 人声分离把原曲分成人声和伴奏（模型只在第一次下载，约 210 MB）。
还会分开主唱和和声，所以也能显示和声的示范线。

※ 分离出的音频仅供个人练习。传播或公开请只在原曲权利人允许的范围内进行。
```

### ツリー 3：练习

```text
🎤 练习功能

・速度 50～150%、调 ±6（正式录音会恢复原速、原调）
・指定范围循环
・示范人声可以单独听，也可以和伴奏一起听
・用麦克风测量音域，推荐能容纳示范的调。没有合适的调时，还会显示超出几个半音
```

### ツリー 4：录音

```text
⏺ 录音

・追溯录音：REC 按晚了，开头也不会缺
・范围重录（Punch in），接缝处交叉淡化
・延迟测量和自动补偿
・节拍器和预备拍（仅监听，不会录进去）
・Main / Double / Harmony 音轨
・Take 比较：在歌曲中对比试听录好的 Take，选出最好的
・显示进拍提前或延后了多少 ms
```

### ツリー 5：交给混音师

```text
📦 交给混音师

・从歌曲开头开始的完整长度 WAV（24bit、保持原采样率、不做标准化）
・交付包：每条音轨的 WAV、确认用混音、备注、zip

导出时已对齐，放进 DAW 就和伴奏的开头一致，可以直接交出去。
```

### ツリー 6：模式

```text
🎛 3 种模式

・简单：只把 Main 从头录到尾再交出去
・标准：叠录、1 条和声、分段重录、Take 比较、交付包
・专业：2 条和声、音准和颤音分析

10 种皮肤，可以换成喜欢的外观。
```

### ツリー 7：语言

```text
🌏 支持 12 种语言

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・第一次启动时，会按电脑的显示语言打开（不支持的语言则用英语）
・在第一个画面可以重新选择语言
・之后随时可以在设置的「语言」中切换，无需重启，立即生效
・下载页面的说明（README）也有 12 种语言
```

### ツリー 8：运行环境

```text
💻 运行环境和注意事项

・Windows 10 / 11、macOS 11 及以上（Apple 芯片 / Intel）
・不需要显卡，只用 CPU 运行
・没有手机版
・可以导入电脑里的音频文件（wav / mp3 / m4a 等）。流媒体平台的歌曲不能直接导入
・这是测试版，还没有代码签名，第一次打开时系统会显示警告。打开方法写在 README 里

免费、开源。欢迎在 GitHub Issues 反馈问题和建议 🙏
```

### ツリー 9：支持

```text
💚 支持开发

VoiceBooth 是免费的，没有订阅，也没有内购。
如果喜欢，欢迎通过 GitHub Sponsors 支持开发。是否支持，功能都完全一样。
https://github.com/sponsors/kajisho5

也可以从软件设置里的「支持开发」和 GitHub 页面上的 Sponsor 按钮打开。
```

<a id="zh-hant"></a>

## 繁體中文（zh-Hant）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.zh-Hant.md

### 本文：正文

```text
【免費】我做了一款翻唱用的練習 & 錄音軟體「VoiceBooth」🎤

匯入原曲和伴奏後，示範旋律（音高）會顯示在畫面上，你唱的時候，自己的音高會以線條疊在上面。
唱準是萊姆綠，走音就變橘色 → 紅色。可以邊看邊練，清楚知道哪裡唱偏了。

・自動從原曲擷取人聲音高，顯示在鋼琴捲簾上
・只有原曲也沒關係，AI 可以分離出人聲和伴奏
・速度 50～150%、調 ±6 練習
・測量你的音域，推薦適合的調
・錄完後匯出 WAV，可以直接交給混音師
・支援 12 種語言

支援 Windows / Mac，免費（測試版）
👇 下載
https://github.com/kajisho5/voicebooth/blob/main/README.zh-Hant.md

#翻唱 #唱歌 #歌唱練習
```

### ツリー 1：示範音高

```text
🎼 示範音高的顯示

匯入有人聲的原曲和伴奏，軟體會從原曲中只擷取歌聲，在鋼琴捲簾上用音符條和音名顯示。
前奏長度不同也會自動對齊時間。
伴奏的調和原曲不同時，也會依伴奏的調顯示。
高八度或低八度唱，也能正確對齊判定。
```

### ツリー 2：只有原曲時

```text
🎧 只有原曲時

用 AI 人聲分離把原曲分成人聲和伴奏（模型只在第一次下載，約 210 MB）。
還會分開主唱和和聲，所以也能顯示和聲的示範線。

※ 分離出的音訊僅供個人練習。散布或公開請只在原曲權利人允許的範圍內進行。
```

### ツリー 3：練習

```text
🎤 練習功能

・速度 50～150%、調 ±6（正式錄音會恢復原速、原調）
・指定範圍循環
・示範人聲可以單獨聽，也可以和伴奏一起聽
・用麥克風測量音域，推薦能容納示範的調。沒有適合的調時，還會顯示超出幾個半音
```

### ツリー 4：錄音

```text
⏺ 錄音

・追溯錄音：REC 按晚了，開頭也不會缺
・範圍重錄（Punch in），接縫處交叉淡化
・延遲測量和自動補償
・節拍器和預備拍（僅監聽，不會錄進去）
・Main / Double / Harmony 音軌
・Take 比較：在歌曲中比較試聽錄好的 Take，選出最好的
・顯示進拍提早或延後了多少 ms
```

### ツリー 5：交給混音師

```text
📦 交給混音師

・從歌曲開頭開始的完整長度 WAV（24bit、維持原取樣率、不做標準化）
・交件包：每條音軌的 WAV、確認用混音、備註、zip

匯出時已對齊，放進 DAW 就和伴奏的開頭一致，可以直接交出去。
```

### ツリー 6：模式

```text
🎛 3 種模式

・簡單：只把 Main 從頭錄到尾再交出去
・標準：疊錄、1 條和聲、分段重錄、Take 比較、交件包
・專業：2 條和聲、音準和顫音分析

10 種外觀主題，可以換成喜歡的樣子。
```

### ツリー 7：語言

```text
🌏 支援 12 種語言

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・第一次啟動時，會依電腦的顯示語言開啟（不支援的語言則用英文）
・在第一個畫面可以重新選擇語言
・之後隨時可以在設定的「語言」中切換，不必重新啟動，立即生效
・下載頁面的說明（README）也有 12 種語言
```

### ツリー 8：執行環境

```text
💻 執行環境和注意事項

・Windows 10 / 11、macOS 11 以上（Apple 晶片 / Intel）
・不需要顯示卡，只用 CPU 執行
・沒有手機版
・可以匯入電腦裡的音訊檔（wav / mp3 / m4a 等）。串流平台的歌曲不能直接匯入
・這是測試版，還沒有程式碼簽章，第一次開啟時系統會顯示警告。開啟方法寫在 README 裡

免費、開源。歡迎在 GitHub Issues 回報問題和建議 🙏
```

### ツリー 9：支持

```text
💚 支持開發

VoiceBooth 是免費的，沒有訂閱，也沒有內購。
如果喜歡，歡迎透過 GitHub Sponsors 支持開發。不論是否支持，功能都完全一樣。
https://github.com/sponsors/kajisho5

也可以從軟體設定裡的「支持開發」和 GitHub 頁面上的 Sponsor 按鈕開啟。
```

<a id="es"></a>

## Español（es）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.es.md

### 本文：Principal

```text
[Gratis] Hice VoiceBooth, una app para practicar y grabar covers 🎤

Carga la canción original y su pista instrumental (karaoke), y la melodía guía (el tono) aparece en pantalla. Cuando cantas, tu propio tono se dibuja encima como una línea.
Verde lima si vas afinado, naranja → rojo si te desvías. Ves exactamente dónde te sales mientras practicas.

・Extrae el tono de la voz del original automáticamente y lo muestra en un piano roll
・¿Solo tienes el original? La IA lo separa en voz e instrumental
・Practica al 50–150 % de tempo y ±6 semitonos
・Mide tu rango vocal y te sugiere una tonalidad
・Exporta WAV que tu ingeniero de mezcla puede usar tal cual
・Disponible en 12 idiomas

Windows / Mac, gratis (beta)
👇 Descarga
https://github.com/kajisho5/voicebooth/blob/main/README.es.md

#cover #canto #cantantes
```

### ツリー 1：Tono guía

```text
🎼 El tono guía

Carga el original (con voz) y la pista instrumental. VoiceBooth extrae solo la voz cantada y la muestra en un piano roll con barras de notas y sus nombres.
Si las intros duran distinto, se alinean automáticamente.
Si el karaoke está en otra tonalidad, la guía sigue la tonalidad del karaoke.
Si cantas una octava arriba o abajo, también lo evalúa correctamente.
```

### ツリー 2：Solo el original

```text
🎧 Cuando solo tienes el original

La separación de voz con IA lo divide en voz e instrumental (los modelos, unos 210 MB, se descargan una sola vez).
También separa voz principal y armonías, así que tienes una línea guía para la armonía.

※ El audio separado es para práctica personal. Compártelo o publícalo solo en la medida en que lo permitan los titulares de derechos de la canción.
```

### ツリー 3：Práctica

```text
🎤 Para practicar

・Tempo 50–150 %, tonalidad ±6 (las tomas finales se graban en el tempo y la tonalidad originales)
・Repite un fragmento en bucle
・Escucha la voz guía sola o con el instrumental
・Mide tu rango con el micrófono y te propone una tonalidad donde cabe la guía. Si ninguna cabe, te dice cuántos semitonos sobran
```

### ツリー 4：Grabación

```text
⏺ Grabación

・Grabación retroactiva: si pulsas REC tarde, el inicio de la frase sigue ahí
・Regrabación de un fragmento (punch-in) con fundidos cruzados en las uniones
・Medición y compensación automática de la latencia
・Clic y cuenta atrás (solo en la escucha, nunca se graban)
・Pistas Main / Double / Harmony
・Comparación de tomas: escucha tus tomas dentro de la canción y elige la mejor
・Muestra cuántos ms entras antes o después
```

### ツリー 5：Para el mezclador

```text
📦 Para tu ingeniero de mezcla

・WAV de duración completa desde el inicio de la canción (24 bits, frecuencia de muestreo original, sin normalizar)
・Paquete de entrega: un WAV por pista, una mezcla de referencia, notas, todo en zip

Al ponerlos en un DAW quedan alineados con el instrumental, así que puedes enviarlos tal cual.
```

### ツリー 6：Modos

```text
🎛 Tres modos

・Fácil: graba Main de principio a fin y entrégalo
・Estándar: doblaje, una armonía, regrabar secciones, comparar tomas, paquete de entrega
・Pro: dos armonías, análisis de afinación y vibrato

10 skins para cambiar el aspecto.
```

### ツリー 7：Idiomas

```text
🌏 12 idiomas

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・La primera vez se abre en el idioma de tu computadora (en inglés si ese idioma no está disponible)
・Puedes elegir otro idioma en la primera pantalla
・Cámbialo cuando quieras en Ajustes → Idioma. Sin reiniciar, el cambio es inmediato
・La página de descarga (README) también está en los 12 idiomas
```

### ツリー 8：Requisitos

```text
💻 Requisitos y avisos

・Windows 10 / 11, macOS 11 o posterior (Apple silicon / Intel)
・No necesita tarjeta gráfica, funciona con la CPU
・No hay versión para móvil ni tableta
・Abre archivos de audio de tu computadora (wav / mp3 / m4a, etc.). Las canciones de servicios de streaming no se pueden cargar directamente
・Es una beta y aún no está firmada, así que tu sistema mostrará un aviso la primera vez. El README explica cómo abrirla

Gratis y de código abierto. Reporta errores e ideas en GitHub Issues 🙏
```

### ツリー 9：Apoyo

```text
💚 Apoya el desarrollo

VoiceBooth es gratis, sin suscripción ni compras dentro de la app.
Si te gusta, puedes apoyar el desarrollo en GitHub Sponsors. Las funciones son las mismas apoyes o no.
https://github.com/sponsors/kajisho5

También puedes llegar desde Ajustes → Apoyar el desarrollo en la app, o con el botón Sponsor de la página de GitHub.
```

<a id="pt-br"></a>

## Português (Brasil)（pt-BR）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.pt-BR.md

### 本文：Principal

```text
[Grátis] Fiz o VoiceBooth, um app para praticar e gravar covers 🎤

Carregue a música original e o instrumental (playback), e a melodia guia (o tom) aparece na tela. Quando você canta, o seu tom é desenhado por cima como uma linha.
Verde-limão quando está afinado, laranja → vermelho quando desafina. Você vê exatamente onde sai do tom enquanto pratica.

・Extrai o tom da voz do original automaticamente e mostra num piano roll
・Só tem o original? A IA separa em voz e instrumental
・Pratique com tempo de 50–150% e tom ±6
・Mede sua extensão vocal e sugere um tom
・Exporta WAV que o seu engenheiro de mixagem pode usar direto
・Disponível em 12 idiomas

Windows / Mac, grátis (beta)
👇 Download
https://github.com/kajisho5/voicebooth/blob/main/README.pt-BR.md

#cover #canto #cantores
```

### ツリー 1：Tom guia

```text
🎼 O tom guia

Carregue o original (com voz) e o instrumental. O VoiceBooth extrai só a voz cantada e mostra num piano roll com barras de notas e os nomes das notas.
Se as introduções tiverem durações diferentes, ele alinha automaticamente.
Se o playback estiver em outro tom, a guia acompanha o tom do playback.
Se você cantar uma oitava acima ou abaixo, ele avalia corretamente também.
```

### ツリー 2：Só o original

```text
🎧 Quando você só tem o original

A separação de voz com IA divide em voz e instrumental (os modelos, cerca de 210 MB, são baixados uma única vez).
Também separa voz principal e harmonia, então você tem uma linha guia para a harmonia.

※ O áudio separado é para prática pessoal. Compartilhe ou publique apenas até onde os detentores dos direitos da música permitirem.
```

### ツリー 3：Prática

```text
🎤 Para praticar

・Tempo 50–150%, tom ±6 (as gravações finais são feitas no tempo e tom originais)
・Repita um trecho em loop
・Ouça a voz guia sozinha ou com o instrumental
・Meça sua extensão com o microfone e receba um tom em que a guia cabe. Se nenhum couber, mostra quantos semitons ficam de fora
```

### ツリー 4：Gravação

```text
⏺ Gravação

・Gravação retroativa: apertou REC atrasado e o começo da frase continua lá
・Regravação de um trecho (punch-in) com crossfade nas emendas
・Medição e compensação automática de latência
・Clique e contagem (só no retorno, nunca são gravados)
・Faixas Main / Double / Harmony
・Comparação de takes: ouça os takes dentro da música e escolha o melhor
・Mostra quantos ms você entrou adiantado ou atrasado
```

### ツリー 5：Para o mixador

```text
📦 Para o seu engenheiro de mixagem

・WAV com a duração completa desde o começo da música (24 bits, taxa de amostragem original, sem normalização)
・Pacote de entrega: um WAV por faixa, uma mix de referência, notas, tudo em zip

Colocados numa DAW, ficam alinhados com o instrumental, então é só enviar.
```

### ツリー 6：Modos

```text
🎛 Três modos

・Fácil: grave Main do começo ao fim e entregue
・Padrão: dobra, uma harmonia, regravar trechos, comparar takes, pacote de entrega
・Pro: duas harmonias, análise de afinação e vibrato

10 skins para mudar o visual.
```

### ツリー 7：Idiomas

```text
🌏 12 idiomas

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・Na primeira vez, abre no idioma do seu computador (em inglês se esse idioma não estiver disponível)
・Dá para escolher outro idioma na primeira tela
・Troque quando quiser em Ajustes → Idioma. Sem reiniciar, muda na hora
・A página de download (README) também está nos 12 idiomas
```

### ツリー 8：Requisitos

```text
💻 Requisitos e avisos

・Windows 10 / 11, macOS 11 ou posterior (Apple silicon / Intel)
・Não precisa de placa de vídeo, roda na CPU
・Não tem versão para celular ou tablet
・Abre arquivos de áudio do seu computador (wav / mp3 / m4a etc.). Músicas de serviços de streaming não podem ser carregadas diretamente
・É uma beta e ainda não tem assinatura de código, então o sistema mostra um aviso na primeira vez. O README explica como abrir

Grátis e de código aberto. Bugs e ideias são bem-vindos no GitHub Issues 🙏
```

### ツリー 9：Apoio

```text
💚 Apoie o desenvolvimento

O VoiceBooth é grátis, sem assinatura e sem compras no app.
Se gostar, você pode apoiar o desenvolvimento no GitHub Sponsors. Os recursos são os mesmos apoiando ou não.
https://github.com/sponsors/kajisho5

Também dá para abrir em Ajustes → Apoiar o desenvolvimento no app, ou pelo botão Sponsor na página do GitHub.
```

<a id="id"></a>

## Bahasa Indonesia（id）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.id.md

### 本文：Utama

```text
[Gratis] Saya membuat VoiceBooth, aplikasi latihan & rekaman untuk lagu cover 🎤

Muat lagu asli dan instrumental (karaoke)-nya, lalu melodi panduan (nada) muncul di layar. Saat kamu bernyanyi, nada suaramu digambar di atasnya sebagai garis.
Hijau limau kalau pas, oranye → merah kalau meleset. Kamu bisa lihat persis di mana suaramu meleset sambil berlatih.

・Mengambil nada vokal dari lagu asli secara otomatis dan menampilkannya di piano roll
・Cuma punya lagu asli? AI memisahkannya jadi vokal dan instrumental
・Latihan dengan tempo 50–150% dan kunci ±6
・Mengukur jangkauan vokalmu dan menyarankan kunci
・Ekspor WAV yang bisa langsung dipakai mixing engineer
・Tersedia dalam 12 bahasa

Windows / Mac, gratis (beta)
👇 Unduh
https://github.com/kajisho5/voicebooth/blob/main/README.id.md

#cover #nyanyi #vokal
```

### ツリー 1：Nada panduan

```text
🎼 Nada panduan

Muat lagu asli (dengan vokal) dan instrumentalnya. VoiceBooth mengambil suara nyanyiannya saja dan menampilkannya di piano roll sebagai balok not beserta nama notnya.
Panjang intro yang berbeda disejajarkan otomatis.
Kalau karaoke-nya beda kunci, panduannya mengikuti kunci karaoke.
Bernyanyi satu oktaf lebih tinggi atau rendah pun tetap dinilai dengan benar.
```

### ツリー 2：Hanya lagu asli

```text
🎧 Kalau hanya punya lagu asli

Pemisahan vokal dengan AI membaginya jadi vokal dan instrumental (model sekitar 210 MB, diunduh sekali saja).
Vokal utama dan harmoni juga dipisah, jadi ada garis panduan untuk harmoni.

※ Audio hasil pemisahan hanya untuk latihan pribadi. Bagikan atau publikasikan hanya sejauh yang diizinkan pemegang hak lagu aslinya.
```

### ツリー 3：Latihan

```text
🎤 Fitur latihan

・Tempo 50–150%, kunci ±6 (rekaman final dibuat di tempo dan kunci asli)
・Ulangi bagian tertentu (loop)
・Dengarkan vokal panduan sendiri atau bersama instrumental
・Ukur jangkauan suara dengan mikrofon dan dapatkan kunci yang pas untuk panduan. Kalau tidak ada yang pas, ditampilkan berapa semitone yang lewat
```

### ツリー 4：Rekaman

```text
⏺ Rekaman

・Rekaman retroaktif: telat menekan REC pun awal frasa tetap ada
・Rekam ulang bagian tertentu (punch-in) dengan crossfade di sambungannya
・Pengukuran dan kompensasi latensi otomatis
・Klik dan hitungan awal (hanya di monitor, tidak ikut terekam)
・Trek Main / Double / Harmony
・Perbandingan take: dengarkan take di dalam lagu dan pilih yang terbaik
・Menampilkan berapa ms kamu masuk lebih cepat atau lambat
```

### ツリー 5：Untuk mixer

```text
📦 Untuk mixing engineer

・WAV durasi penuh dari awal lagu (24-bit, sample rate asli, tanpa normalisasi)
・Paket kiriman: WAV per trek, mix referensi, catatan, dalam zip

Saat ditaruh di DAW, file sudah sejajar dengan instrumental, jadi bisa langsung dikirim.
```

### ツリー 6：Mode

```text
🎛 Tiga mode

・Mudah: rekam Main dari awal sampai akhir lalu kirim
・Standar: double, satu harmoni, rekam ulang bagian, perbandingan take, paket kiriman
・Pro: dua harmoni, analisis ketepatan nada dan vibrato

10 skin untuk mengganti tampilan.
```

### ツリー 7：Bahasa

```text
🌏 12 bahasa

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・Saat pertama dibuka, aplikasi memakai bahasa tampilan komputermu (bahasa Inggris kalau bahasanya belum didukung)
・Kamu bisa memilih bahasa lain di layar pertama
・Ganti kapan saja di Pengaturan → Bahasa. Tanpa restart, langsung berubah
・Halaman unduhan (README) juga tersedia dalam 12 bahasa
```

### ツリー 8：Persyaratan

```text
💻 Persyaratan & catatan

・Windows 10 / 11, macOS 11 atau lebih baru (Apple silicon / Intel)
・Tidak perlu kartu grafis, berjalan di CPU
・Tidak ada versi ponsel atau tablet
・Membuka file audio di komputermu (wav / mp3 / m4a, dll.). Lagu dari layanan streaming tidak bisa dimuat langsung
・Ini versi beta dan belum ditandatangani, jadi sistem akan menampilkan peringatan saat pertama kali dibuka. Cara membukanya ada di README

Gratis dan open source. Laporkan bug dan ide di GitHub Issues 🙏
```

### ツリー 9：Dukungan

```text
💚 Dukung pengembangan

VoiceBooth gratis, tanpa langganan dan tanpa pembelian dalam aplikasi.
Kalau kamu suka, kamu bisa mendukung pengembangan lewat GitHub Sponsors. Fiturnya sama saja, mendukung atau tidak.
https://github.com/sponsors/kajisho5

Bisa juga dibuka dari Pengaturan → Dukung pengembangan di aplikasi, atau tombol Sponsor di halaman GitHub.
```

<a id="vi"></a>

## Tiếng Việt（vi）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.vi.md

### 本文：Bài chính

```text
[Miễn phí] Mình đã làm VoiceBooth, ứng dụng luyện tập & thu âm để hát cover 🎤

Mở bài hát gốc và bản beat (karaoke), giai điệu mẫu (cao độ) sẽ hiện trên màn hình. Khi bạn hát, cao độ giọng của bạn được vẽ chồng lên thành một đường.
Đúng thì màu xanh chanh, lệch thì cam → đỏ. Bạn vừa luyện vừa thấy rõ chỗ nào bị phô.

・Tự động lấy cao độ giọng hát từ bản gốc và hiển thị trên piano roll
・Chỉ có bản gốc? AI tách thành giọng hát và beat
・Luyện với tempo 50–150% và tông ±6
・Đo quãng giọng và gợi ý tông phù hợp
・Xuất WAV để gửi thẳng cho người mix
・Hỗ trợ 12 ngôn ngữ

Windows / Mac, miễn phí (beta)
👇 Tải về
https://github.com/kajisho5/voicebooth/blob/main/README.vi.md

#cover #hátcover #luyệnthanh
```

### ツリー 1：Cao độ mẫu

```text
🎼 Hiển thị cao độ mẫu

Mở bản gốc (có giọng hát) và bản beat. VoiceBooth chỉ lấy riêng giọng hát và hiển thị trên piano roll bằng các thanh nốt kèm tên nốt.
Đoạn dạo đầu dài ngắn khác nhau cũng được tự động căn khớp.
Beat khác tông thì phần mẫu cũng theo tông của beat.
Hát cao hay thấp một quãng tám vẫn được chấm đúng.
```

### ツリー 2：Chỉ có bản gốc

```text
🎧 Khi chỉ có bản gốc

Tách giọng bằng AI chia bài hát thành giọng hát và beat (mô hình khoảng 210 MB, chỉ tải một lần).
Giọng chính và bè cũng được tách riêng, nên có cả đường mẫu cho phần bè.

※ Âm thanh đã tách chỉ dùng để luyện tập cá nhân. Chỉ chia sẻ hoặc đăng tải trong phạm vi chủ sở hữu quyền của bài hát cho phép.
```

### ツリー 3：Luyện tập

```text
🎤 Tính năng luyện tập

・Tempo 50–150%, tông ±6 (bản thu chính thức được thu ở tempo và tông gốc)
・Lặp lại một đoạn
・Nghe giọng mẫu riêng hoặc cùng với beat
・Đo quãng giọng bằng micro và nhận gợi ý tông vừa với phần mẫu. Nếu không có tông nào vừa, sẽ hiện lệch bao nhiêu nửa cung
```

### ツリー 4：Thu âm

```text
⏺ Thu âm

・Thu hồi tố: bấm REC muộn vẫn không mất đầu câu
・Thu lại một đoạn (punch-in), chỗ nối có crossfade
・Đo và tự động bù độ trễ
・Click và đếm nhịp vào (chỉ nghe ở monitor, không bị thu vào)
・Các track Main / Double / Harmony
・So sánh take: nghe các take ngay trong bài và chọn take tốt nhất
・Hiện bạn vào sớm hay muộn bao nhiêu ms
```

### ツリー 5：Gửi người mix

```text
📦 Gửi cho người mix

・WAV đủ độ dài từ đầu bài (24-bit, giữ nguyên tần số lấy mẫu, không chuẩn hóa)
・Gói bàn giao: WAV từng track, bản mix tham khảo, ghi chú, nén zip

Đặt vào DAW là khớp đầu với beat, nên có thể gửi luôn.
```

### ツリー 6：Chế độ

```text
🎛 Ba chế độ

・Dễ: thu Main một mạch từ đầu đến cuối rồi gửi
・Chuẩn: double, một bè, thu lại từng đoạn, so sánh take, gói bàn giao
・Pro: hai bè, phân tích độ chuẩn cao độ và vibrato

10 skin để đổi giao diện.
```

### ツリー 7：Ngôn ngữ

```text
🌏 12 ngôn ngữ

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・Lần đầu mở, ứng dụng dùng ngôn ngữ hiển thị của máy tính (tiếng Anh nếu ngôn ngữ đó chưa được hỗ trợ)
・Có thể chọn lại ngôn ngữ ở màn hình đầu tiên
・Đổi bất cứ lúc nào trong Cài đặt → Ngôn ngữ. Không cần khởi động lại, đổi ngay lập tức
・Trang tải về (README) cũng có đủ 12 ngôn ngữ
```

### ツリー 8：Cấu hình

```text
💻 Cấu hình & lưu ý

・Windows 10 / 11, macOS 11 trở lên (Apple silicon / Intel)
・Không cần card đồ họa, chạy bằng CPU
・Không có bản cho điện thoại hay máy tính bảng
・Mở được file âm thanh trên máy (wav / mp3 / m4a…). Không mở trực tiếp được bài hát từ dịch vụ nghe nhạc trực tuyến
・Đây là bản beta và chưa được ký số, nên lần đầu mở hệ điều hành sẽ hiện cảnh báo. Cách mở có trong README

Miễn phí và mã nguồn mở. Báo lỗi và góp ý tại GitHub Issues 🙏
```

### ツリー 9：Ủng hộ

```text
💚 Ủng hộ phát triển

VoiceBooth miễn phí, không có gói thuê bao hay mua trong ứng dụng.
Nếu bạn thích, có thể ủng hộ phát triển qua GitHub Sponsors. Ủng hộ hay không thì tính năng vẫn như nhau.
https://github.com/sponsors/kajisho5

Cũng có thể mở từ Cài đặt → Ủng hộ phát triển trong ứng dụng, hoặc nút Sponsor trên trang GitHub.
```

<a id="tr"></a>

## Türkçe（tr）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.tr.md

### 本文：Ana gönderi

```text
[Ücretsiz] Cover şarkılar için pratik ve kayıt uygulaması VoiceBooth'u yaptım 🎤

Orijinal şarkıyı ve altyapısını (karaoke) yükleyin; rehber melodi (perde) ekranda görünür. Siz söylerken kendi sesinizin perdesi üzerine çizgi olarak çizilir.
Doğruysa limon yeşili, kayınca turuncu → kırmızı. Pratik yaparken nerede detone olduğunuzu açıkça görürsünüz.

・Orijinalden vokal perdesini otomatik çıkarır ve piyano rulosunda gösterir
・Elinizde sadece orijinal mi var? Yapay zekâ onu vokal ve altyapı olarak ayırır
・%50–150 tempo ve ±6 ton ile pratik
・Ses aralığınızı ölçer ve ton önerir
・Miks mühendisinize olduğu gibi verebileceğiniz WAV dosyaları dışa aktarır
・12 dil desteği

Windows / Mac, ücretsiz (beta)
👇 İndir
https://github.com/kajisho5/voicebooth/blob/main/README.tr.md

#cover #şarkı #vokal
```

### ツリー 1：Rehber perde

```text
🎼 Rehber perde

Vokalli orijinali ve altyapıyı yükleyin. VoiceBooth yalnızca şarkı söyleyen sesi çıkarır ve piyano rulosunda nota çubukları ve nota adlarıyla gösterir.
Giriş bölümlerinin uzunluğu farklı olsa da zaman otomatik hizalanır.
Karaoke farklı tondaysa rehber, karaokenin tonunu takip eder.
Bir oktav yukarı ya da aşağı söyleseniz de doğru değerlendirilir.
```

### ツリー 2：Sadece orijinal

```text
🎧 Sadece orijinal varsa

Yapay zekâ ile vokal ayırma, şarkıyı vokal ve altyapı olarak böler (modeller yaklaşık 210 MB, yalnızca bir kez indirilir).
Ana vokal ve armoni de ayrılır; böylece armoni için de rehber çizgisi olur.

※ Ayrılan ses kişisel pratik içindir. Yalnızca orijinal şarkının hak sahiplerinin izin verdiği ölçüde paylaşın veya yayımlayın.
```

### ツリー 3：Pratik

```text
🎤 Pratik araçları

・Tempo %50–150, ton ±6 (asıl kayıtlar orijinal tempo ve tonda yapılır)
・Bir bölümü döngüye alın
・Rehber vokali tek başına ya da altyapıyla birlikte dinleyin
・Mikrofonla ses aralığınızı ölçün, rehberin sığdığı tonu önersin. Hiçbiri sığmazsa kaç yarım ses taştığını gösterir
```

### ツリー 4：Kayıt

```text
⏺ Kayıt

・Geriye dönük kayıt: REC'e geç bassanız da cümlenin başı kaybolmaz
・Bir bölümü yeniden kaydetme (punch-in), birleşim yerlerinde crossfade
・Gecikme ölçümü ve otomatik telafi
・Klik ve sayım (yalnızca monitörde, kayda girmez)
・Main / Double / Harmony kanalları
・Take karşılaştırma: take'leri şarkının içinde dinleyip en iyisini seçin
・Girişlerinizin kaç ms erken ya da geç olduğunu gösterir
```

### ツリー 5：Miks için

```text
📦 Miks mühendisinize teslim

・Şarkının başından itibaren tam uzunlukta WAV (24-bit, orijinal örnekleme hızı, normalizasyon yok)
・Teslim paketi: kanal başına bir WAV, referans miks, notlar, hepsi zip içinde

Bir DAW'a koyduğunuzda altyapıyla hizalı olur, olduğu gibi gönderebilirsiniz.
```

### ツリー 6：Modlar

```text
🎛 Üç mod

・Kolay: Main'i baştan sona kaydedip teslim edin
・Standart: double, bir armoni, bölüm yeniden kayıt, take karşılaştırma, teslim paketi
・Pro: iki armoni, perde doğruluğu ve vibrato analizi

Görünümü değiştirmek için 10 tema.
```

### ツリー 7：Diller

```text
🌏 12 dil

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・İlk açılışta bilgisayarınızın görüntüleme dilinde açılır (o dil desteklenmiyorsa İngilizce)
・İlk ekranda başka bir dil seçebilirsiniz
・İstediğiniz zaman Ayarlar → Dil'den değiştirin. Yeniden başlatmaya gerek yok, hemen değişir
・İndirme sayfası (README) de 12 dilde mevcut
```

### ツリー 8：Gereksinimler

```text
💻 Gereksinimler ve notlar

・Windows 10 / 11, macOS 11 veya sonrası (Apple silicon / Intel)
・Ekran kartı gerekmez, CPU ile çalışır
・Telefon veya tablet sürümü yok
・Bilgisayarınızdaki ses dosyalarını açar (wav / mp3 / m4a vb.). Müzik akış servislerindeki şarkılar doğrudan yüklenemez
・Beta sürümdür ve henüz imzalı değildir; ilk açılışta işletim sistemi uyarı gösterir. Nasıl açılacağı README'de anlatılıyor

Ücretsiz ve açık kaynak. Hata ve önerilerinizi GitHub Issues'a bekliyorum 🙏
```

### ツリー 9：Destek

```text
💚 Geliştirmeyi destekleyin

VoiceBooth ücretsizdir; abonelik ya da uygulama içi satın alma yoktur.
Beğendiyseniz GitHub Sponsors üzerinden geliştirmeyi destekleyebilirsiniz. Destekleseniz de desteklemeseniz de özellikler aynıdır.
https://github.com/sponsors/kajisho5

Uygulamada Ayarlar → Geliştirmeyi destekle'den ya da GitHub sayfasındaki Sponsor düğmesinden de açabilirsiniz.
```

<a id="de"></a>

## Deutsch（de）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.de.md

### 本文：Haupt-Post

```text
[Kostenlos] Ich habe VoiceBooth gemacht, eine App zum Üben und Aufnehmen von Coversongs 🎤

Lade den Originalsong und das Instrumental (Karaoke), und die Leitmelodie (Tonhöhe) erscheint auf dem Bildschirm. Wenn du singst, wird deine eigene Tonhöhe als Linie darübergelegt.
Limettengrün, wenn du triffst, Orange → Rot, wenn du danebenliegst. Du siehst beim Üben genau, wo du abweichst.

・Holt die Gesangstonhöhe automatisch aus dem Original und zeigt sie in einer Piano-Roll
・Nur das Original da? Die KI trennt es in Gesang und Instrumental
・Üben mit 50–150 % Tempo und ±6 Halbtönen
・Misst deinen Stimmumfang und schlägt eine Tonart vor
・Exportiert WAV-Dateien, die dein Mix-Engineer direkt verwenden kann
・In 12 Sprachen verfügbar

Windows / Mac, kostenlos (Beta)
👇 Download
https://github.com/kajisho5/voicebooth/blob/main/README.de.md

#Cover #Gesang #Singen
```

### ツリー 1：Leit-Tonhöhe

```text
🎼 Die Leit-Tonhöhe

Lade das Original (mit Gesang) und das Instrumental. VoiceBooth holt nur die Singstimme heraus und zeigt sie in einer Piano-Roll als Notenbalken mit Notennamen.
Unterschiedlich lange Intros werden automatisch ausgerichtet.
Ist das Karaoke in einer anderen Tonart, folgt die Leitlinie der Tonart des Karaoke.
Singst du eine Oktave höher oder tiefer, wird trotzdem richtig bewertet.
```

### ツリー 2：Nur das Original

```text
🎧 Wenn du nur das Original hast

Die KI-Gesangstrennung teilt es in Gesang und Instrumental (die Modelle, etwa 210 MB, werden nur einmal heruntergeladen).
Lead und Harmonie werden ebenfalls getrennt, so gibt es auch eine Leitlinie für die Harmonie.

※ Getrenntes Audio ist für das private Üben gedacht. Teile oder veröffentliche es nur so weit, wie es die Rechteinhaber des Originalsongs erlauben.
```

### ツリー 3：Üben

```text
🎤 Zum Üben

・Tempo 50–150 %, Tonart ±6 (die finalen Takes werden in Originaltempo und -tonart aufgenommen)
・Einen Abschnitt in Schleife üben
・Den Leitgesang solo oder mit dem Instrumental hören
・Stimmumfang mit dem Mikrofon messen und eine Tonart vorschlagen lassen, in die die Leitmelodie passt. Passt keine, zeigt es, um wie viele Halbtöne sie herausragt
```

### ツリー 4：Aufnahme

```text
⏺ Aufnahme

・Rückwirkende Aufnahme: REC zu spät gedrückt, und der Phrasenanfang ist trotzdem da
・Abschnitt neu aufnehmen (Punch-in), mit Crossfades an den Übergängen
・Latenzmessung und automatischer Ausgleich
・Klick und Einzähler (nur im Monitor, nie auf der Aufnahme)
・Spuren Main / Double / Harmony
・Take-Vergleich: Takes im Song anhören und den besten wählen
・Zeigt, wie viele ms du zu früh oder zu spät einsetzt
```

### ツリー 5：Für den Mixer

```text
📦 Übergabe an deinen Mix-Engineer

・WAV in voller Länge ab Songanfang (24 Bit, Original-Samplerate, ohne Normalisierung)
・Lieferpaket: ein WAV pro Spur, ein Referenzmix, Notizen, alles als ZIP

In eine DAW gelegt, liegen die Dateien bündig zum Instrumental, du kannst sie also direkt weitergeben.
```

### ツリー 6：Modi

```text
🎛 Drei Modi

・Einfach: Main am Stück aufnehmen und abgeben
・Standard: Doppelung, eine Harmonie, Abschnitte neu aufnehmen, Take-Vergleich, Lieferpaket
・Pro: zwei Harmonien, Analyse von Intonation und Vibrato

10 Skins für ein anderes Aussehen.
```

### ツリー 7：Sprachen

```text
🌏 12 Sprachen

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・Beim ersten Start öffnet sie sich in der Anzeigesprache deines Computers (auf Englisch, wenn diese Sprache nicht unterstützt wird)
・Auf dem ersten Bildschirm kannst du eine andere Sprache wählen
・Jederzeit wechseln unter Einstellungen → Sprache. Kein Neustart nötig, es wechselt sofort
・Die Download-Seite (README) gibt es ebenfalls in allen 12 Sprachen
```

### ツリー 8：Voraussetzungen

```text
💻 Voraussetzungen und Hinweise

・Windows 10 / 11, macOS 11 oder neuer (Apple Silicon / Intel)
・Keine Grafikkarte nötig, läuft auf der CPU
・Keine Version für Smartphone oder Tablet
・Öffnet Audiodateien auf deinem Computer (wav / mp3 / m4a usw.). Songs von Streamingdiensten lassen sich nicht direkt laden
・Es ist eine Beta und noch nicht signiert, daher zeigt dein Betriebssystem beim ersten Öffnen eine Warnung. Wie du sie öffnest, steht in der README

Kostenlos und Open Source. Fehler und Ideen gern in den GitHub Issues 🙏
```

### ツリー 9：Unterstützen

```text
💚 Entwicklung unterstützen

VoiceBooth ist kostenlos, ohne Abo und ohne In-App-Käufe.
Wenn sie dir gefällt, kannst du die Entwicklung über GitHub Sponsors unterstützen. Alle Funktionen sind gleich, ob du unterstützt oder nicht.
https://github.com/sponsors/kajisho5

Du kommst auch über Einstellungen → Entwicklung unterstützen in der App oder den Sponsor-Button auf der GitHub-Seite dorthin.
```

<a id="fr"></a>

## Français（fr）

リンク先：https://github.com/kajisho5/voicebooth/blob/main/README.fr.md

### 本文：Principal

```text
[Gratuit] J'ai créé VoiceBooth, une app pour s'entraîner et enregistrer des reprises 🎤

Chargez la chanson originale et son instrumental (karaoké) : la mélodie guide (hauteur) s'affiche à l'écran. Quand vous chantez, votre propre hauteur se superpose sous forme de ligne.
Vert citron quand c'est juste, orange → rouge quand ça dérape. Vous voyez exactement où vous sortez de la note en vous entraînant.

・Extrait automatiquement la hauteur de la voix de l'original et l'affiche dans un piano roll
・Vous n'avez que l'original ? L'IA le sépare en voix et instrumental
・Entraînement à 50–150 % du tempo et ±6 demi-tons
・Mesure votre tessiture et propose une tonalité
・Exporte des WAV que votre ingénieur de mixage peut utiliser tels quels
・Disponible en 12 langues

Windows / Mac, gratuit (bêta)
👇 Téléchargement
https://github.com/kajisho5/voicebooth/blob/main/README.fr.md

#reprise #chant #cover
```

### ツリー 1：Hauteur guide

```text
🎼 La hauteur guide

Chargez l'original (avec la voix) et l'instrumental. VoiceBooth extrait uniquement la voix chantée et l'affiche dans un piano roll avec des barres de notes et leurs noms.
Les intros de longueurs différentes sont alignées automatiquement.
Si le karaoké est dans une autre tonalité, le guide suit celle du karaoké.
Chantez une octave au-dessus ou en dessous, l'évaluation reste correcte.
```

### ツリー 2：Seulement l'original

```text
🎧 Quand vous n'avez que l'original

La séparation vocale par IA le divise en voix et instrumental (les modèles, environ 210 Mo, se téléchargent une seule fois).
La voix principale et les harmonies sont aussi séparées : vous avez donc une ligne guide pour l'harmonie.

※ L'audio séparé est destiné à l'entraînement personnel. Ne le partagez ou ne le publiez que dans la mesure autorisée par les ayants droit de la chanson.
```

### ツリー 3：Entraînement

```text
🎤 Pour s'entraîner

・Tempo 50–150 %, tonalité ±6 (les prises finales sont enregistrées au tempo et à la tonalité d'origine)
・Bouclez un passage
・Écoutez la voix guide seule ou avec l'instrumental
・Mesurez votre tessiture au micro et obtenez une tonalité où le guide tient. Si aucune ne convient, l'app indique de combien de demi-tons ça dépasse
```

### ツリー 4：Enregistrement

```text
⏺ Enregistrement

・Enregistrement rétroactif : même si vous appuyez sur REC en retard, le début de la phrase est conservé
・Réenregistrement d'un passage (punch-in) avec fondus enchaînés aux raccords
・Mesure et compensation automatique de la latence
・Clic et décompte (uniquement dans le retour, jamais enregistrés)
・Pistes Main / Double / Harmony
・Comparaison des prises : écoutez vos prises dans la chanson et gardez la meilleure
・Indique de combien de ms vos entrées sont en avance ou en retard
```

### ツリー 5：Pour le mixeur

```text
📦 Pour votre ingénieur de mixage

・WAV pleine durée depuis le début de la chanson (24 bits, fréquence d'échantillonnage d'origine, sans normalisation)
・Pack de livraison : un WAV par piste, un mix de référence, des notes, le tout en zip

Placés dans une DAW, les fichiers sont calés sur l'instrumental : vous pouvez les envoyer tels quels.
```

### ツリー 6：Modes

```text
🎛 Trois modes

・Facile : enregistrez Main d'une traite et livrez
・Standard : doublage, une harmonie, réenregistrement de passages, comparaison des prises, pack de livraison
・Pro : deux harmonies, analyse de la justesse et du vibrato

10 skins pour changer l'apparence.
```

### ツリー 7：Langues

```text
🌏 12 langues

日本語 / English / 한국어 / 简体中文 / 繁體中文 / Español / Português (Brasil) / Bahasa Indonesia / Tiếng Việt / Türkçe / Deutsch / Français

・Au premier lancement, l'app s'ouvre dans la langue d'affichage de votre ordinateur (en anglais si cette langue n'est pas prise en charge)
・Vous pouvez choisir une autre langue dès le premier écran
・Changez quand vous voulez dans Réglages → Langue. Pas besoin de redémarrer, le changement est immédiat
・La page de téléchargement (README) existe aussi dans les 12 langues
```

### ツリー 8：Configuration

```text
💻 Configuration et remarques

・Windows 10 / 11, macOS 11 ou ultérieur (Apple silicon / Intel)
・Pas besoin de carte graphique, fonctionne sur le processeur
・Pas de version pour téléphone ou tablette
・Ouvre les fichiers audio de votre ordinateur (wav / mp3 / m4a, etc.). Les morceaux des services de streaming ne peuvent pas être chargés directement
・C'est une bêta, pas encore signée : votre système affichera un avertissement à la première ouverture. Le README explique comment l'ouvrir

Gratuit et open source. Bugs et idées bienvenus dans les GitHub Issues 🙏
```

### ツリー 9：Soutien

```text
💚 Soutenir le développement

VoiceBooth est gratuit, sans abonnement ni achat intégré.
Si vous l'aimez, vous pouvez soutenir le développement sur GitHub Sponsors. Toutes les fonctions restent les mêmes, que vous souteniez ou non.
https://github.com/sponsors/kajisho5

Vous pouvez aussi y accéder depuis Réglages → Soutenir le développement dans l'app, ou avec le bouton Sponsor de la page GitHub.
```
