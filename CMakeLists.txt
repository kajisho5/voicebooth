cmake_minimum_required(VERSION 3.22)

# macOS：Apple シリコンと Intel の両方で動くユニバーサル版（2026-10-01 決定。DESIGN 11.6）。
# 最低は macOS 11 Big Sur。project() より前に置くこと（Mac 以外では使われない）。
# 片方だけ作る時は -DCMAKE_OSX_ARCHITECTURES=arm64 のように上書きする
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

project(VoiceBooth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# 既定：出力デバイスを開いてオフボを鳴らす（B2）
# -DVOICEBOOTH_UI_MOCK=ON：音声デバイスを一切開かないビルド（DESIGN 11.1 / 14「UI_MOCK で開く」）
# ---------------------------------------------------------------------------
option(VOICEBOOTH_UI_MOCK "Never opens audio devices (screenshots / UI checks)" OFF)
option(VOICEBOOTH_TESTS "Build VoiceBoothTests (ctest)" ON)

# ---------------------------------------------------------------------------
# JUCE 8
#   既定: FetchContent でタグ固定取得
#   ローカルにある場合: -DVOICEBOOTH_JUCE_DIR=/path/to/JUCE
# ---------------------------------------------------------------------------
set(VOICEBOOTH_JUCE_DIR "" CACHE PATH "Local JUCE checkout (empty = fetch from GitHub)")
set(VOICEBOOTH_JUCE_TAG "8.0.15" CACHE STRING "JUCE tag used by FetchContent")

if(VOICEBOOTH_JUCE_DIR)
    add_subdirectory("${VOICEBOOTH_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${VOICEBOOTH_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# 埋め込みフォント（IBM Plex Sans JP / IBM Plex Mono、SIL OFL 1.1）
# ---------------------------------------------------------------------------
juce_add_binary_data(VoiceBoothFonts
    HEADER_NAME VoiceBoothFonts.h
    NAMESPACE   VoiceBoothFonts
    SOURCES
        resources/fonts/IBMPlexSansJP-Regular.ttf
        resources/fonts/IBMPlexSansJP-Medium.ttf
        resources/fonts/IBMPlexSansJP-SemiBold.ttf
        resources/fonts/IBMPlexMono-Regular.ttf
        resources/fonts/IBMPlexMono-Medium.ttf
        resources/fonts/IBMPlexMono-SemiBold.ttf)

# 翻訳表（DESIGN 10。tools/check_i18n.py で検査）
juce_add_binary_data(VoiceBoothI18n
    HEADER_NAME VoiceBoothI18n.h
    NAMESPACE   VoiceBoothI18n
    SOURCES
        resources/i18n/ja.json
        resources/i18n/en.json
        resources/i18n/ko.json
        resources/i18n/zh-Hans.json
        resources/i18n/zh-Hant.json)

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------
# アプリアイコン（brand/build_brand.py で生成。Mac は余白＋影つき、Win はフルブリード）
if(APPLE)
    set(VOICEBOOTH_ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/brand/out/icons/juce_icon_big_mac.png")
else()
    set(VOICEBOOTH_ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/brand/out/icons/juce_icon_big_win.png")
endif()

juce_add_gui_app(VoiceBooth
    ICON_BIG     "${VOICEBOOTH_ICON_BIG}"
    ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/brand/out/icons/juce_icon_small.png"
    PRODUCT_NAME "VoiceBooth"
    COMPANY_NAME "SEVENTHWELL"
    BUNDLE_ID    "com.seventhwell.voicebooth"   # TODO: 署名主体の決定後に確定（DESIGN 19）
    VERSION      ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "VoiceBooth はボーカル録音のためにマイクを使用します。")

# 音声ファイルの読み込み（UI に依存しない。テストからも使う）
set(VOICEBOOTH_AUDIO_SOURCES
    app/audio/WaveformOverview.cpp
    app/audio/SongLoader.cpp
    app/audio/MediaFoundationFormat.cpp
    app/audio/Mp3Format.cpp
    app/audio/Mp4Gapless.cpp
    app/audio/PlaybackCore.cpp
    app/audio/InputMeter.cpp
    app/audio/DeviceRules.cpp)

# 曲の情報（DESIGN 7.5：歌詞の読み込み・テンポの手入力）。UI に依存しない。テストからも使う
set(VOICEBOOTH_SONG_SOURCES
    app/song/LyricsImport.cpp
    app/song/SongInfo.cpp
    app/system/SystemCheck.cpp
    app/analysis/Align.cpp)

# スキン（DESIGN 4.11：色・.vbskin の読み書き・見やすさの点検）。UI に依存しない。テストからも使う
set(VOICEBOOTH_SKIN_SOURCES
    app/skin/Skin.cpp)

target_sources(VoiceBooth PRIVATE
    app/Main.cpp
    app/i18n/I18n.cpp
    ${VOICEBOOTH_AUDIO_SOURCES}
    ${VOICEBOOTH_SONG_SOURCES}
    ${VOICEBOOTH_SKIN_SOURCES}
    # テーマ・動き（DESIGN 4.10：共通の時計は 1 つ）
    app/ui/Theme.cpp
    app/ui/VoiceBoothLookAndFeel.cpp
    app/ui/Animator.cpp
    # 部品
    app/ui/parts/Icons.cpp
    app/ui/parts/KeyButton.cpp
    app/ui/parts/Encoder.cpp
    app/ui/parts/ConsoleFader.cpp
    app/ui/parts/LedMeter.cpp
    app/ui/parts/Readout.cpp
    app/ui/parts/Dropdown.cpp
    app/ui/parts/ColourPad.cpp
    app/ui/Gallery.cpp
    # 状態
    app/ui/DummySession.cpp
    app/ui/UiSession.cpp
    app/ui/UiSessionSong.cpp
    # 画面
    app/ui/LaneCommon.cpp
    app/ui/SongMarks.cpp
    app/ui/Overlay.cpp
    app/ui/TopBar.cpp
    app/ui/TransportBar.cpp
    app/ui/PitchLane.cpp
    app/ui/LyricsLane.cpp
    app/ui/WaveLane.cpp
    app/ui/TrackTabs.cpp
    app/ui/Rack.cpp
    app/ui/StatusBar.cpp
    app/ui/MainComponent.cpp
    # ダイアログ / 起動画面
    app/ui/screens/StartScreen.cpp
    app/ui/screens/SetupWizard.cpp
    app/ui/screens/ExportDialog.cpp
    app/ui/screens/SettingsDialog.cpp
    app/ui/screens/WelcomeScreen.cpp
    app/ui/screens/UpdateDialog.cpp
    app/ui/screens/SkinEditor.cpp
    app/ui/screens/SkinTemplates.cpp
    app/ui/screens/SongInfoDialog.cpp
    app/ui/screens/LyricsDialog.cpp)

target_include_directories(VoiceBooth PRIVATE app)

target_compile_definitions(VoiceBooth PRIVATE
    VOICEBOOTH_UI_MOCK=$<BOOL:${VOICEBOOTH_UI_MOCK}>
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:VoiceBooth,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:VoiceBooth,JUCE_VERSION>")

# 日本語リテラルを UTF-8 として扱う
if(MSVC)
    target_compile_options(VoiceBooth PRIVATE /utf-8)
endif()

#   juce_audio_formats はファイルを読むだけ。mp3 は同梱の minimp3（CC0、third_party/minimp3）で読み、
#   JUCE 内蔵の MP3 デコーダ（JUCE_USE_MP3AUDIOFORMAT）は有効にしない
target_link_libraries(VoiceBooth
    PRIVATE
        VoiceBoothFonts
        VoiceBoothI18n
        juce::juce_gui_basics
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# 出力・入力デバイス（B2 / B3）。UI_MOCK では juce_audio_devices をリンクしない
if(NOT VOICEBOOTH_UI_MOCK)
    target_sources(VoiceBooth PRIVATE app/audio/PlaybackEngine.cpp)
    target_link_libraries(VoiceBooth PRIVATE juce::juce_audio_devices)
    if(APPLE)
        # マイクの許可を AVFoundation に聞く（.mm = Objective-C++。中身は MicPermission.cpp）
        target_sources(VoiceBooth PRIVATE app/audio/MicPermission.mm)
        target_link_libraries(VoiceBooth PRIVATE "-framework AVFoundation")
    else()
        target_sources(VoiceBooth PRIVATE app/audio/MicPermission.cpp)
    endif()
endif()

# OS の「動きを減らす」設定（DESIGN 4.10）。Mac は NSWorkspace を使うので .mm（中身は ReducedMotion.cpp）
if(APPLE)
    target_sources(VoiceBooth PRIVATE app/ui/ReducedMotion.mm)
else()
    target_sources(VoiceBooth PRIVATE app/ui/ReducedMotion.cpp)
endif()

# Windows：m4a は Media Foundation で読む（OS 標準。MSVC は #pragma comment でもリンクされる）
if(WIN32)
    target_link_libraries(VoiceBooth PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()

# ---------------------------------------------------------------------------
# テスト（ctest）。DESIGN 17：長さ・サンプル位置の正しさを機械で確かめる
# ---------------------------------------------------------------------------
if(VOICEBOOTH_TESTS)
    enable_testing()
    juce_add_console_app(VoiceBoothTests PRODUCT_NAME "VoiceBoothTests")
    target_sources(VoiceBoothTests PRIVATE
        tests/TestMain.cpp
        tests/WaveformOverviewTests.cpp
        tests/SongLoaderTests.cpp
        tests/PlaybackCoreTests.cpp
        tests/InputMeterTests.cpp
        tests/DeviceRulesTests.cpp
        tests/LyricsImportTests.cpp
        tests/TempoTests.cpp
        tests/SongInfoTests.cpp
        tests/SystemCheckTests.cpp
        tests/AlignTests.cpp
        tests/SkinTests.cpp
        tests/MotionTests.cpp
        ${VOICEBOOTH_AUDIO_SOURCES}
        ${VOICEBOOTH_SONG_SOURCES}
        ${VOICEBOOTH_SKIN_SOURCES})
    target_include_directories(VoiceBoothTests PRIVATE app)
    target_compile_definitions(VoiceBoothTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_MODAL_LOOPS_PERMITTED=1    # 非同期の通知をテストで回すため
        VOICEBOOTH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    if(MSVC)
        target_compile_options(VoiceBoothTests PRIVATE /utf-8)
    endif()
    target_link_libraries(VoiceBoothTests
        PRIVATE
            juce::juce_audio_formats
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    if(WIN32)
        target_link_libraries(VoiceBoothTests PRIVATE mfplat mfreadwrite mfuuid ole32)
    endif()
    add_test(NAME VoiceBoothTests COMMAND VoiceBoothTests)
endif()
