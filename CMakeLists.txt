cmake_minimum_required(VERSION 3.22)

# macOS: arm64 優先（ユニバーサルは P2 以降）。project() より前に置くこと。
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(VoiceBooth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Phase A: 見た目だけ。音声デバイスを一切開かないビルド（DESIGN 11.1 UI_MOCK=1）
# ---------------------------------------------------------------------------
option(VOICEBOOTH_UI_MOCK "UI only build. Never opens audio devices (Phase A)" ON)

if(NOT VOICEBOOTH_UI_MOCK)
    message(FATAL_ERROR
        "Phase B（結線）は未実装です。-DVOICEBOOTH_UI_MOCK=ON でビルドしてください。")
endif()

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
        resources/i18n/en.json)

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------
juce_add_gui_app(VoiceBooth
    PRODUCT_NAME "VoiceBooth"
    COMPANY_NAME "SEVENTHWELL"
    BUNDLE_ID    "com.seventhwell.voicebooth"   # TODO: 署名主体の決定後に確定（DESIGN 19）
    VERSION      ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "VoiceBooth はボーカル録音のためにマイクを使用します。")

target_sources(VoiceBooth PRIVATE
    app/Main.cpp
    app/i18n/I18n.cpp
    # テーマ
    app/ui/Theme.cpp
    app/ui/VoiceBoothLookAndFeel.cpp
    # 部品
    app/ui/parts/Icons.cpp
    app/ui/parts/KeyButton.cpp
    app/ui/parts/Encoder.cpp
    app/ui/parts/ConsoleFader.cpp
    app/ui/parts/LedMeter.cpp
    app/ui/parts/Readout.cpp
    app/ui/Gallery.cpp
    # 状態
    app/ui/DummySession.cpp
    app/ui/UiSession.cpp
    # 画面
    app/ui/LaneCommon.cpp
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
    app/ui/screens/SettingsDialog.cpp)

target_include_directories(VoiceBooth PRIVATE app)

target_compile_definitions(VoiceBooth PRIVATE
    VOICEBOOTH_UI_MOCK=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:VoiceBooth,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:VoiceBooth,JUCE_VERSION>")

# 日本語リテラルを UTF-8 として扱う
if(MSVC)
    target_compile_options(VoiceBooth PRIVATE /utf-8)
endif()

# UI_MOCK では juce_audio_* をリンクしない（音声デバイスを開けない構成にする）
target_link_libraries(VoiceBooth
    PRIVATE
        VoiceBoothFonts
        VoiceBoothI18n
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
