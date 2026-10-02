<p align="center">
  <img src="brand/out/marketing/readme-banner-ko.png" alt="VoiceBooth — 커버곡 녹음 전용 DAW. 보면서 고치고, 한 트랙으로 넘긴다." width="100%">
</p>

<p align="center">
  <a href="README.md">日本語</a> ·
  <a href="README.en.md">English</a> ·
  <b>한국어</b> ·
  <a href="README.zh-Hans.md">简体中文</a> ·
  <a href="README.zh-Hant.md">繁體中文</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-in%20development-F4B942?labelColor=141311" alt="개발 중">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-8CC1EE?labelColor=141311" alt="Windows / macOS">
  <img src="https://img.shields.io/badge/license-AGPL--3.0--or--later-C6EE6A?labelColor=141311" alt="AGPL-3.0-or-later">
</p>

# VoiceBooth

**커버곡 녹음만을 위한 작은 보컬 DAW입니다.**
MR에 맞춰 노래하면서 음정을 화면으로 보고 고치고, 고치고 싶은 부분만 다시 녹음해서, 믹스 엔지니어에게 그대로 넘길 수 있는 WAV를 내보냅니다. 그 일에 필요한 기능만 넣었습니다. 만능 DAW가 아닙니다.

![VoiceBooth 메인 화면](docs/screenshots/main-ko.png)

> [!NOTE]
> **개발 중입니다. 아직 배포하지 않았습니다.** 지금 동작하는 것은 "곡을 열어 파형 보기"와 "MR 재생·탐색·루프"까지입니다. 화면의 숫자·가사·음정은 샘플입니다.

## 할 수 있는 것

| | 내용 | 상태 |
|---|---|---|
| 곡 열기 | wav / flac / aiff / ogg / mp3 / m4a. 드래그 앤 드롭으로도 열 수 있음. 곡의 시작 위치가 Windows와 Mac에서 같음 | ✅ 동작 |
| MR 재생 | 재생·탐색·구간 루프·볼륨. 루프 이음새에 빈틈 없음 | ✅ 동작 |
| 입력 장치와 레벨 | 마이크를 고르고, 목표 범위(-12~-6 dB)를 보면서 레벨을 맞춤 | 🔧 개발 중 |
| 가이드 음정 | 가이드 멜로디를 "허용 범위의 띠"로, 내 목소리를 선으로 겹쳐 표시. 맞으면 라임색, 벗어나면 주황→빨강 | ⬜ 예정 |
| 원곡＋MR | 보컬이 있는 원곡을 가이드로, MR(반주) 위에서 노래. 전주 길이·속도 차이·컷 버전·키가 다른 MR도 자동으로 시간을 맞추고 손으로 미세 조정 가능. 넘기는 파일은 항상 MR의 처음부터 | 🔧 맞추는 부품 완료(화면은 앞으로) |
| 연습 | 템포 50~150%, 키 ±6. 느리게 연습해도 납품 녹음은 원래 템포·키로 | ⬜ 예정 |
| 소급 녹음 | 재생을 시작할 때부터 뒤에서 녹음하고 있어서, REC를 늦게 눌러도 첫 소절이 잘리지 않음 | ⬜ 예정 |
| 구간 재녹음 | 범위를 골라 펀치 인. 이음새는 8 ms 크로스페이드 | ⬜ 예정 |
| Main / Double / Harmony | 더블과 하모니를 같은 길이로 녹음 | ⬜ 예정 |
| 보컬 분리 | 믹스 음원에서 MR과 가이드 보컬을 만듦(처음 사용할 때 모델을 다운로드. 중간에 끊겨도 이어서 받음) | ⬜ 예정 |
| 5개 언어 | 日本語 / English / 한국어 / 简体中文 / 繁體中文 | ✅ 동작 |
| 스킨 | 색을 한꺼번에 바꾸기(내장 10종). 템플릿으로 직접 만들고 `.vbskin` 파일로 공유 | ✅ 동작 |

### 넘기는 파일의 약속

믹스 엔지니어가 DAW에 놓는 순간 MR과 시작이 맞도록 내보냅니다(목표 ±1 ms).

- 곡의 처음(0초)부터 끝까지 전체 길이. 녹음하지 않은 부분은 무음
- 24bit 모노 WAV. 샘플레이트는 원곡 그대로(멋대로 48 kHz로 바꾸지 않음)
- 노멀라이즈·자동 페이드 없음. 모니터용 리버브나 가이드는 섞지 않음
- 연습으로 녹음한 것은 납품 폴더에 넣지 않음

### 세 가지 모드

엔진은 같고, 보이는 것만 바뀝니다. 어느 모드에서 만든 프로젝트든 다른 모드에서 열 수 있습니다.

| 간단 | 표준 | 프로 |
|---|---|---|
| Main을 통째로 녹음해서 넘기기만 | 더블·하모니 1개·구간 재녹음·들어가는 타이밍 | 하모니 2개＋좌우 더블, 비브라토 등 분석, 납품 팩 |

| 간단 모드 | 프로 모드(하모니) |
|---|---|
| ![간단 모드](docs/screenshots/mode-easy.png) | ![프로 모드 하모니](docs/screenshots/mode-pro-harmony.png) |

## 로고와 디자인

<p>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="brand/out/logo/logo-mark-dark.png">
    <img src="brand/out/logo/logo-mark-light.png" alt="VoiceBooth 마크" width="96" align="left">
  </picture>
  <b>마크: 녹음 부스의 창, 마이크 캡슐, 탈리 램프.</b><br>
  부스 창 너머에 마이크가 있고, 오른쪽 위 램프가 켜져 있는 모습입니다. 탈리는 평소 라임색이고, 앱 안에서는 녹음 중에만 빨갛게 켜집니다.
</p>
<br clear="left">

**콘셉트는 "밤의 녹음 부스".** 따뜻한 느낌의 어두운 그래파이트에 장비의 LED와 탈리 램프가 켜집니다. 상태는 테두리 색이 아니라 LED 점등으로 보여 주고, 녹음 중이 아니면 화면을 빨갛게 하지 않습니다.

![색상 견본](brand/out/marketing/palette.png)

| 색 | 쓰는 곳 |
|---|---|
| Signal(라임) | 내 음정이 맞을 때·재생 헤드·켜진 LED |
| Reference(아이스 블루) | 가이드의 허용 범위 띠 |
| Amber / Coral | 조금 벗어남 / 많이 벗어남·경고 |
| Tally(빨강) | 녹음 중에만 |

- **서체**: IBM Plex Sans JP. 시간·dB 같은 숫자는 IBM Plex Mono(고정폭이라 자릿수가 흔들리지 않음). 한글은 OS 기본 글꼴로 표시
- **부품**: 키캡＋LED 버튼, LED 링 노브, 세로 콘솔 페이더, 세그먼트 LED 미터
- **움직임**: 스프링처럼 눌리는 키, 0 dB에서 손맛이 있는 페이더, 박자에 맞춰 켜지는 카운트인. 소리를 최우선으로 하고, OS의 "동작 줄이기" 설정도 따릅니다

**스킨.** 색만 한꺼번에 바꿀 수 있습니다(글꼴·배치·움직임은 그대로). 내장 스킨은 10종으로, 밝은 방에 맞춘 Studio Day / Sweet, 가독성을 우선한 High Contrast, 색 구별이 어려운 분을 배려한 Color Safe도 있습니다. 설정의 '스킨' → '새로 만들기'에서 템플릿을 고르고, 색마다·그룹마다(다른 템플릿에서 가져오기도 가능) 바꿔 저장할 수 있습니다. 보기 어려운 조합은 편집하는 동안 알려 줍니다(저장은 막지 않습니다). `.vbskin` 파일로 다른 사람과 공유할 수 있습니다.

![내장 스킨 10종](docs/screenshots/skins/all.png)

| 앱 아이콘 | 브랜드 키트 |
|---|---|
| ![앱 아이콘](brand/out/preview/preview-icons.png) | ![브랜드 키트](brand/out/preview/preview-brand.png) |

로고 사용 규칙(여백·최소 크기·밝은 배경용)과 모든 소재는 [`brand/`](brand/README.md)에 있습니다(일본어).

## 동작 환경(잠정)

| | 최소 | 권장 |
|---|---|---|
| Windows | Windows 10 64bit(버전 1607 이상) | Windows 11 |
| Mac | macOS 11 Big Sur 이상(Apple 실리콘 / Intel 모두 지원하는 유니버설 버전) | 최신 macOS, Apple 실리콘 |
| CPU | 64bit·4코어 | 6코어 이상(Apple M1 이상, 최근 몇 년의 Intel Core i5 / AMD Ryzen 5 급 이상) |
| 메모리 | 8 GB | 16 GB |
| 여유 공간 | 2 GB | 10 GB 이상(SSD) |
| 화면 | 1280×800 | 1440×900 이상 |
| 오디오 | 내장 입출력으로도 동작 | 오디오 인터페이스＋유선 헤드폰(Windows는 ASIO 지원이면 지연이 적음) |
| 인터넷 | 처음 분리 모델을 다운로드할 때만(연결이 없어도 분리 외에는 사용 가능) | — |

- 무거운 것은 보컬 분리뿐입니다. 오래된 CPU나 Intel Mac에서는 분리에 시간이 걸립니다(실측해서 확정 예정)
- 블루투스 이어폰·헤드폰은 지연이 커서 녹음에는 맞지 않습니다
- Arm용 Windows는 확인하지 않았습니다
- 수치는 개발 중의 기준입니다. 근거는 [`docs/DESIGN.md`](docs/DESIGN.md) 11.6.1(일본어)

## 다운로드

아직 배포하지 않았습니다. 준비가 되면 이 페이지의 Releases에 올립니다.

## 개발 후원

VoiceBooth는 무료입니다. 마음에 드셨다면 [GitHub Sponsors](https://github.com/sponsors/kajisho5)로 후원해 주시면 개발을 이어갈 수 있습니다. 후원 여부에 따라 쓸 수 있는 기능은 달라지지 않습니다.

## 라이선스

소스 코드는 **GNU Affero General Public License v3.0 이상(AGPL-3.0-or-later)** 입니다([`LICENSE`](LICENSE)). JUCE 8을 AGPLv3로 사용하기 때문에 앱 전체를 AGPL로 했습니다.

- 사용·수정·배포·판매는 자유입니다. 배포할 때(수정판을 네트워크로 쓰게 할 때도)는 소스도 같은 라이선스로 공개해 주세요
- **이름과 로고**: 수정판을 별도 제품으로 배포할 때는 "VoiceBooth"라는 이름과 로고를 쓰지 말아 주세요(다른 이름·로고로). 그대로 재배포하거나 소개·리뷰에 쓰는 것은 괜찮습니다
- 녹음하고 내보낸 오디오 파일은 여러분의 것입니다. AGPL은 만든 작품에는 적용되지 않습니다

| 포함·사용하는 것 | 라이선스 |
|---|---|
| JUCE 8 | AGPLv3 / 상용 듀얼(여기서는 AGPLv3) |
| minimp3(`third_party/minimp3`) | CC0 |
| IBM Plex Sans JP / IBM Plex Mono(`resources/fonts`) | SIL Open Font License 1.1 |
| Rubber Band Library 4(연습용 템포 / 키. 빌드할 때 가져옴) | GPL v2 이상 / 상용 듀얼(여기서는 GPL) |
| Steinberg ASIO SDK 2.3.4(Windows 빌드만. 빌드할 때 공식 배포본을 가져옴) | GPLv3 / 상용 듀얼(여기서는 GPLv3). SDK 자체는 저장소에 넣지 않음. ASIO는 Steinberg Media Technologies GmbH의 상표 |
| 예정: 분리·음정·가사 모델 | 가중치 라이선스를 확인한 것만. 앱과 별도로 배포 |

## 개발 참여

빌드·테스트·구성은 [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md), 사양은 [`docs/DESIGN.md`](docs/DESIGN.md)(둘 다 일본어).
