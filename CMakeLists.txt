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
# Rubber Band Library 4（練習用のテンポ・キー。DESIGN 7.2 / 11.4 / B11）
#   GPL-2.0-or-later（AGPL-3.0 と組み合わせられる。DESIGN 18）。1 ファイル版（single/RubberBandSingle.cpp）を
#   そのままビルドする：FFT・リサンプラは内蔵（Mac は vDSP）。スレッドは使わない（オーディオスレッドで回す）
#   ローカルにある場合: -DVOICEBOOTH_RUBBERBAND_DIR=/path/to/rubberband
# ---------------------------------------------------------------------------
set(VOICEBOOTH_RUBBERBAND_DIR "" CACHE PATH "Local Rubber Band checkout (empty = fetch from GitHub)")
set(VOICEBOOTH_RUBBERBAND_TAG "v4.0.0" CACHE STRING "Rubber Band tag used by FetchContent")
if(VOICEBOOTH_RUBBERBAND_DIR)
    set(VB_RUBBERBAND_SOURCE "${VOICEBOOTH_RUBBERBAND_DIR}")
else()
    include(FetchContent)
    FetchContent_Declare(rubberband
        GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
        GIT_TAG        ${VOICEBOOTH_RUBBERBAND_TAG}
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  no-cmake)   # CMakeLists の無い所を指す＝取ってくるだけ（meson のビルドは使わず、1 ファイル版を下で足す）
    FetchContent_MakeAvailable(rubberband)
    set(VB_RUBBERBAND_SOURCE "${rubberband_SOURCE_DIR}")
endif()
add_library(vb_rubberband STATIC "${VB_RUBBERBAND_SOURCE}/single/RubberBandSingle.cpp")
target_include_directories(vb_rubberband SYSTEM PUBLIC "${VB_RUBBERBAND_SOURCE}")
set_target_properties(vb_rubberband PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_definitions(vb_rubberband PRIVATE _USE_MATH_DEFINES NOMINMAX)
endif()
if(APPLE)
    target_link_libraries(vb_rubberband PUBLIC "-framework Accelerate")
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
    DOCUMENT_EXTENSIONS vbooth                      # プロジェクトファイル（B14）。Mac の Info.plist に入る
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
    app/audio/MonitorMixer.cpp
    app/audio/TakeRecorder.cpp
    app/audio/LatencyProbe.cpp
    app/audio/Retro.cpp
    app/audio/PitchTracker.cpp
    app/audio/Resample.cpp
    app/audio/DeviceRules.cpp)

# テイクと採用区間・書き出し（DESIGN 6 / 8 / 9：Phase B5〜）。UI に依存しない。テストからも使う
set(VOICEBOOTH_RECORD_SOURCES
    app/project/Comp.cpp
    app/project/ProjectFile.cpp
    app/export/ExportService.cpp)

# 曲の情報（DESIGN 7.5：歌詞の読み込み・テンポの手入力）。UI に依存しない。テストからも使う
set(VOICEBOOTH_SONG_SOURCES
    app/song/LyricsImport.cpp
    app/song/SongInfo.cpp
    app/system/SystemCheck.cpp
    app/analysis/Align.cpp
    app/analysis/RefPitch.cpp
    app/analysis/MusicInfo.cpp)

# スキン（DESIGN 4.11：色・.vbskin の読み書き・見やすさの点検）。UI に依存しない。テストからも使う
set(VOICEBOOTH_SKIN_SOURCES
    app/skin/Skin.cpp)

target_sources(VoiceBooth PRIVATE
    app/Main.cpp
    app/i18n/I18n.cpp
    ${VOICEBOOTH_AUDIO_SOURCES}
    ${VOICEBOOTH_RECORD_SOURCES}
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
        vb_rubberband
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
# Windows：ASIO（DESIGN 5 / 11.1 / B13）
#   Steinberg ASIO SDK 2.3.4（2025-10-15）は Steinberg の独自ライセンスと GPL v3 のデュアル。ここでは GPL v3 で使う
#   （AGPL-3.0 と組み合わせられる）。SDK はリポジトリに入れない：configure 時に Steinberg の公式の zip を取り、
#   SHA-256 で中身を確かめる。手元の SDK を使う時は -DVOICEBOOTH_ASIO_SDK_DIR=/path/to/ASIOSDK、使わない時は -DVOICEBOOTH_ASIO=OFF
#   （ASIO が無くても WASAPI で動く）。ASIO のロゴは使わない（Steinberg の使用ガイドラインに従う）
# ---------------------------------------------------------------------------
if(WIN32 AND NOT VOICEBOOTH_UI_MOCK)
    option(VOICEBOOTH_ASIO "Enable ASIO (fetches the Steinberg ASIO SDK, GPLv3 option)" ON)
    set(VOICEBOOTH_ASIO_SDK_DIR "" CACHE PATH "Local ASIO SDK (folder that contains common/iasiodrv.h)")
    if(VOICEBOOTH_ASIO)
        if(VOICEBOOTH_ASIO_SDK_DIR)
            set(VB_ASIO_SDK "${VOICEBOOTH_ASIO_SDK_DIR}")
        else()
            # zip の中にロゴのファイル名で「®」があり、CMake の展開は日本語版 Windows（CP932）で失敗しうるので、
            # 取るのは file(DOWNLOAD)（SHA-256 で確かめる）、展開は PowerShell の Expand-Archive にする
            set(VB_ASIO_ROOT "${CMAKE_BINARY_DIR}/_deps/asiosdk")
            set(VB_ASIO_ZIP "${VB_ASIO_ROOT}/ASIO-SDK_2.3.4_2025-10-15.zip")
            if(NOT EXISTS "${VB_ASIO_ROOT}/ASIOSDK/common/iasiodrv.h")
                file(MAKE_DIRECTORY "${VB_ASIO_ROOT}")
                file(DOWNLOAD https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip "${VB_ASIO_ZIP}"
                     EXPECTED_HASH SHA256=d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca
                     STATUS vb_asio_status)
                list(GET vb_asio_status 0 vb_asio_code)
                if(NOT vb_asio_code EQUAL 0)
                    message(FATAL_ERROR "Can't download the ASIO SDK (${vb_asio_status}). Use -DVOICEBOOTH_ASIO_SDK_DIR=... or -DVOICEBOOTH_ASIO=OFF.")
                endif()
                execute_process(COMMAND powershell -NoProfile -NonInteractive -Command
                                "Expand-Archive -LiteralPath '${VB_ASIO_ZIP}' -DestinationPath '${VB_ASIO_ROOT}' -Force"
                                RESULT_VARIABLE vb_asio_unzip)
                if(NOT vb_asio_unzip EQUAL 0)
                    message(FATAL_ERROR "Can't extract the ASIO SDK. Use -DVOICEBOOTH_ASIO_SDK_DIR=... or -DVOICEBOOTH_ASIO=OFF.")
                endif()
            endif()
            set(VB_ASIO_SDK "${VB_ASIO_ROOT}/ASIOSDK")
        endif()
        if(NOT EXISTS "${VB_ASIO_SDK}/common/iasiodrv.h")
            message(FATAL_ERROR "ASIO SDK not found at ${VB_ASIO_SDK} (expected common/iasiodrv.h). Use -DVOICEBOOTH_ASIO=OFF to build without ASIO.")
        endif()
        target_include_directories(VoiceBooth SYSTEM PRIVATE "${VB_ASIO_SDK}/common")
        target_compile_definitions(VoiceBooth PRIVATE JUCE_ASIO=1)
        message(STATUS "VoiceBooth: ASIO enabled (${VB_ASIO_SDK})")
    endif()
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
        tests/MonitorMixerTests.cpp
        tests/RecordTests.cpp
        tests/ResampleTests.cpp
        tests/LatencyTests.cpp
        tests/RetroTests.cpp
        tests/PitchTests.cpp
        tests/RefPitchTests.cpp
        tests/MusicInfoTests.cpp
        tests/ProjectFileTests.cpp
        ${VOICEBOOTH_AUDIO_SOURCES}
        ${VOICEBOOTH_RECORD_SOURCES}
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
            vb_rubberband
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
