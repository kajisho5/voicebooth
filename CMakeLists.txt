cmake_minimum_required(VERSION 3.22)

# macOS：Apple シリコンと Intel の両方で動くユニバーサル版（2026-10-01 決定。DESIGN 11.6）。
# 最低は macOS 11 Big Sur。project() より前に置くこと（Mac 以外では使われない）。
# 片方だけ作る時は -DCMAKE_OSX_ARCHITECTURES=arm64 のように上書きする
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

project(VoiceBooth VERSION 0.2.0 LANGUAGES C CXX)

# プレリリースの印（beta.1 → アプリのバージョンは 0.1.0-beta.1）。CI がタグ（v0.1.0-beta.1）から付ける。手元のビルドは空（= 0.1.0）。
# 更新の確認（DESIGN 11.7）が、いまのバージョンがベータかどうか・どのバージョンより新しいかを正しく比べるために使う
set(VOICEBOOTH_VERSION_SUFFIX "" CACHE STRING "Pre-release label appended to the version (e.g. beta.1)")
if(VOICEBOOTH_VERSION_SUFFIX)
    set(VB_FULL_VERSION "${PROJECT_VERSION}-${VOICEBOOTH_VERSION_SUFFIX}")
else()
    set(VB_FULL_VERSION "${PROJECT_VERSION}")
endif()

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
# Monocypher 4.0.3（CC0 / BSD-2-Clause）：モデルの一覧（manifest）の Ed25519 署名を確かめる（DESIGN 11.7 / B16）
#   Ed25519（SHA-512）は optional/monocypher-ed25519.c。持ち主は標準の Ed25519 の道具（openssl / Python）で署名できる
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(monocypher
    GIT_REPOSITORY https://github.com/LoupVaillant/Monocypher.git
    GIT_TAG        4.0.3
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  no-cmake)
FetchContent_MakeAvailable(monocypher)
add_library(vb_monocypher STATIC "${monocypher_SOURCE_DIR}/src/monocypher.c" "${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c")
target_include_directories(vb_monocypher SYSTEM PUBLIC "${monocypher_SOURCE_DIR}/src")
set_target_properties(vb_monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Linux（開発用）：モデルのダウンロード（juce::URL）に libcurl を使う。Win / Mac は OS の仕組み（curl は使わない）
set(VB_USE_CURL 0)
if(UNIX AND NOT APPLE)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        set(VB_USE_CURL 1)
    endif()
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
        resources/i18n/zh-Hant.json
        resources/i18n/es.json
        resources/i18n/pt-BR.json
        resources/i18n/id.json
        resources/i18n/vi.json
        resources/i18n/tr.json
        resources/i18n/de.json
        resources/i18n/fr.json)

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------
# アプリアイコン（brand/build_brand.py で生成。Mac は余白＋影つき、Win はフルブリード）
if(APPLE)
    set(VOICEBOOTH_ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/brand/out/icons/juce_icon_big_mac.png")
else()
    set(VOICEBOOTH_ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/brand/out/icons/juce_icon_big_win.png")
endif()

if(VB_USE_CURL)
    set(VB_NEEDS_CURL TRUE)
else()
    set(VB_NEEDS_CURL FALSE)
endif()

juce_add_gui_app(VoiceBooth
    NEEDS_CURL   ${VB_NEEDS_CURL}
    ICON_BIG     "${VOICEBOOTH_ICON_BIG}"
    ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/brand/out/icons/juce_icon_small.png"
    PRODUCT_NAME "VoiceBooth"
    COMPANY_NAME "SEVENTHWELL"
    BUNDLE_ID    "com.seventhwell.voicebooth"   # TODO: 署名主体の決定後に確定（DESIGN 19）
    VERSION      ${PROJECT_VERSION}
    DOCUMENT_EXTENSIONS vbooth                      # プロジェクトファイル（B14）。Mac の Info.plist に入る
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "VoiceBooth uses the microphone to record your vocals.")   # 言語ごとの文は下の InfoPlist.strings（#23）

# 音声ファイルの読み込み（UI に依存しない。テストからも使う）
set(VOICEBOOTH_AUDIO_SOURCES
    app/audio/WaveformOverview.cpp
    app/audio/SongLoader.cpp
    app/audio/MediaFoundationFormat.cpp
    app/audio/Mp3Format.cpp
    app/audio/Mp4Gapless.cpp
    app/audio/PlaybackCore.cpp
    app/audio/Metronome.cpp
    app/audio/GameSounds.cpp
    app/audio/InputMeter.cpp
    app/audio/MonitorMixer.cpp
    app/audio/TakeRecorder.cpp
    app/audio/LatencyProbe.cpp
    app/audio/Retro.cpp
    app/audio/PitchTracker.cpp
    app/audio/Resample.cpp
    app/audio/PitchShift.cpp
    app/audio/DeviceRules.cpp)

# テイクと採用区間・書き出し（DESIGN 6 / 8 / 9：Phase B5〜）。UI に依存しない。テストからも使う
set(VOICEBOOTH_RECORD_SOURCES
    app/project/Comp.cpp
    app/project/TakeCompare.cpp
    app/project/ProjectFile.cpp
    app/export/ExportService.cpp
    app/export/DeliveryPack.cpp)

# 曲の情報（DESIGN 7.5：歌詞の読み込み・テンポの手入力）。UI に依存しない。テストからも使う
set(VOICEBOOTH_SONG_SOURCES
    app/song/LyricsImport.cpp
    app/song/SongInfo.cpp
    app/system/SystemCheck.cpp
    app/system/AppCache.cpp
    app/system/Background.cpp
    app/update/UpdateCheck.cpp
    app/analysis/Align.cpp
    app/analysis/RefPitch.cpp
    app/analysis/MusicInfo.cpp
    app/analysis/Separation.cpp
    app/analysis/OffVocal.cpp
    app/analysis/TakeStats.cpp
    app/analysis/KeySuggest.cpp
    app/analysis/GuideClean.cpp
    app/analysis/Rmvpe.cpp
    app/analysis/AnalysisCache.cpp
    app/separation/SeparatorClient.cpp
    app/models/ModelManifest.cpp
    app/models/ModelDownloader.cpp
    app/i18n/Reasons.cpp)

# スキン（DESIGN 4.11：色・.vbskin の読み書き・見やすさの点検）。UI に依存しない。テストからも使う
set(VOICEBOOTH_SKIN_SOURCES
    app/skin/Skin.cpp)

# 画面と状態（UI）。アプリと、UiSession を音声なしで動かすテスト（VoiceBoothSessionTests）で使う
set(VOICEBOOTH_UI_SOURCES
    app/i18n/I18n.cpp
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
    app/ui/UiSessionProject.cpp
    app/ui/UiSessionUpdate.cpp
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
    app/ui/screens/WaitGame.cpp
    app/ui/screens/SetupWizard.cpp
    app/ui/screens/ExportDialog.cpp
    app/ui/screens/SettingsDialog.cpp
    app/ui/screens/WelcomeScreen.cpp
    app/ui/screens/UpdateDialog.cpp
    app/ui/screens/AboutDialog.cpp
    app/ui/screens/SkinEditor.cpp
    app/ui/screens/SkinTemplates.cpp
    app/ui/screens/SongInfoDialog.cpp
    app/ui/screens/LyricsDialog.cpp
    app/ui/screens/RangeDialog.cpp
    app/ui/screens/TakeCompareDialog.cpp)
# OS の「動きを減らす」設定（DESIGN 4.10）。Mac は NSWorkspace を使うので .mm（中身は ReducedMotion.cpp）
if(APPLE)
    list(APPEND VOICEBOOTH_UI_SOURCES app/ui/ReducedMotion.mm)
else()
    list(APPEND VOICEBOOTH_UI_SOURCES app/ui/ReducedMotion.cpp)
endif()

target_sources(VoiceBooth PRIVATE
    app/Main.cpp
    ${VOICEBOOTH_AUDIO_SOURCES}
    ${VOICEBOOTH_RECORD_SOURCES}
    ${VOICEBOOTH_SONG_SOURCES}
    ${VOICEBOOTH_SKIN_SOURCES}
    ${VOICEBOOTH_UI_SOURCES})

target_include_directories(VoiceBooth PRIVATE app)

target_compile_definitions(VoiceBooth PRIVATE
    VOICEBOOTH_UI_MOCK=$<BOOL:${VOICEBOOTH_UI_MOCK}>
    JUCE_WEB_BROWSER=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:VoiceBooth,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:VoiceBooth,JUCE_VERSION>"
    VOICEBOOTH_VERSION_STRING="${VB_FULL_VERSION}")

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
        vb_monocypher
        juce::juce_cryptography
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


# Windows：m4a は Media Foundation で読む（OS 標準。MSVC は #pragma comment でもリンクされる）
if(WIN32)
    target_link_libraries(VoiceBooth PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()

# ---------------------------------------------------------------------------
# Mac のマイク許可の文を 12 言語に（#23）。翻訳表（resources/i18n/*.json）の system.microphonePermission から
# <言語>.lproj/InfoPlist.strings を作り、VoiceBooth.app/Contents/Resources に置く。macOS は使う人の言語の物を表示する
# （無い言語は Info.plist の英語）。書いた後に plutil で形を確かめる
# ---------------------------------------------------------------------------
if(APPLE)
    set(vb_lproj_root "${CMAKE_BINARY_DIR}/lproj")
    file(REMOVE_RECURSE "${vb_lproj_root}")
    file(GLOB vb_i18n_tables "${CMAKE_SOURCE_DIR}/resources/i18n/*.json")
    foreach(table ${vb_i18n_tables})
        get_filename_component(lang "${table}" NAME_WE)
        file(READ "${table}" vb_json)
        string(JSON vb_mic GET "${vb_json}" "system.microphonePermission")
        string(REPLACE "\\" "\\\\" vb_mic "${vb_mic}")
        string(REPLACE "\"" "\\\"" vb_mic "${vb_mic}")
        file(WRITE "${vb_lproj_root}/${lang}.lproj/InfoPlist.strings" "\"NSMicrophoneUsageDescription\" = \"${vb_mic}\";\n")
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${vb_i18n_tables})
    add_custom_command(TARGET VoiceBooth POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_directory "${vb_lproj_root}" "$<TARGET_FILE_DIR:VoiceBooth>/../Resources")
    foreach(table ${vb_i18n_tables})
        get_filename_component(lang "${table}" NAME_WE)
        add_custom_command(TARGET VoiceBooth POST_BUILD
                           COMMAND plutil -lint "$<TARGET_FILE_DIR:VoiceBooth>/../Resources/${lang}.lproj/InfoPlist.strings")
    endforeach()
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
# ボーカル分離の別プロセス VoiceBoothSeparator（DESIGN 11.3 / B16）
#   ONNX Runtime 1.22.0（MIT）の公式ビルド済みを configure 時に取る（SHA-256 で確かめる）。1.22.0 は Intel Mac も入った
#   universal2 がある最後のバージョン（1.23 以降は Apple シリコンだけ）。本体には ORT をリンクしない（分離プロセスだけ）
#   手元の ORT を使う時は -DVOICEBOOTH_ORT_DIR=/path/to/onnxruntime-<os>-<バージョン>、作らない時は -DVOICEBOOTH_SEPARATOR=OFF
# ---------------------------------------------------------------------------
option(VOICEBOOTH_SEPARATOR "Build VoiceBoothSeparator (vocal separation with ONNX Runtime)" ON)
if(VOICEBOOTH_SEPARATOR AND NOT VOICEBOOTH_UI_MOCK)
    set(VOICEBOOTH_ORT_DIR "" CACHE PATH "Local ONNX Runtime package (folder with include/ and lib/)")
    if(VOICEBOOTH_ORT_DIR)
        set(VB_ORT "${VOICEBOOTH_ORT_DIR}")
    else()
        if(WIN32)
            set(vb_ort_url  https://github.com/microsoft/onnxruntime/releases/download/v1.22.0/onnxruntime-win-x64-1.22.0.zip)
            set(vb_ort_hash 174c616efc0271194488642a72f1a514e01487da4dfe84c49296d66e40ebe0da)
        elseif(APPLE)
            set(vb_ort_url  https://github.com/microsoft/onnxruntime/releases/download/v1.22.0/onnxruntime-osx-universal2-1.22.0.tgz)
            set(vb_ort_hash cfa6f6584d87555ed9f6e7e8a000d3947554d589efe3723b8bfa358cd263d03c)
        else()
            set(vb_ort_url  https://github.com/microsoft/onnxruntime/releases/download/v1.22.0/onnxruntime-linux-x64-1.22.0.tgz)
            set(vb_ort_hash 8344d55f93d5bc5021ce342db50f62079daf39aaafb5d311a451846228be49b3)
        endif()
        include(FetchContent)
        FetchContent_Declare(onnxruntime
            URL      ${vb_ort_url}
            URL_HASH SHA256=${vb_ort_hash}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            SOURCE_SUBDIR no-cmake)
        FetchContent_MakeAvailable(onnxruntime)
        set(VB_ORT "${onnxruntime_SOURCE_DIR}")
    endif()

    add_library(vb_onnxruntime SHARED IMPORTED)
    set_target_properties(vb_onnxruntime PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${VB_ORT}/include")
    if(WIN32)
        set_target_properties(vb_onnxruntime PROPERTIES IMPORTED_LOCATION "${VB_ORT}/lib/onnxruntime.dll"
                                                        IMPORTED_IMPLIB   "${VB_ORT}/lib/onnxruntime.lib")
        set(VB_ORT_RUNTIME_FILES "${VB_ORT}/lib/onnxruntime.dll" "${VB_ORT}/lib/onnxruntime_providers_shared.dll")
    elseif(APPLE)
        set_target_properties(vb_onnxruntime PROPERTIES IMPORTED_LOCATION "${VB_ORT}/lib/libonnxruntime.1.22.0.dylib")
        set(VB_ORT_RUNTIME_FILES "${VB_ORT}/lib/libonnxruntime.1.22.0.dylib")
    else()
        set_target_properties(vb_onnxruntime PROPERTIES IMPORTED_LOCATION "${VB_ORT}/lib/libonnxruntime.so.1.22.0"
                                                        IMPORTED_SONAME   "libonnxruntime.so.1")
        set(VB_ORT_RUNTIME_FILES "${VB_ORT}/lib/libonnxruntime.so.1.22.0" "${VB_ORT}/lib/libonnxruntime.so.1")
    endif()

    juce_add_console_app(VoiceBoothSeparator PRODUCT_NAME "VoiceBoothSeparator")
    target_sources(VoiceBoothSeparator PRIVATE app/separator/SeparatorMain.cpp app/analysis/Separation.cpp app/analysis/Rmvpe.cpp)
    target_include_directories(VoiceBoothSeparator PRIVATE app)
    target_compile_definitions(VoiceBoothSeparator PRIVATE JUCE_WEB_BROWSER=0 JUCE_STANDALONE_APPLICATION=1)
    if(MSVC)
        target_compile_options(VoiceBoothSeparator PRIVATE /utf-8)
    endif()
    target_link_libraries(VoiceBoothSeparator
        PRIVATE
            vb_onnxruntime
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    # ORT のライブラリは分離プロセスの隣に置く（Linux / Mac は実行ファイルの場所から探す）
    if(APPLE)
        set_target_properties(VoiceBoothSeparator PROPERTIES BUILD_RPATH "@loader_path" INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        set_target_properties(VoiceBoothSeparator PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
    endif()
    foreach(f ${VB_ORT_RUNTIME_FILES})
        add_custom_command(TARGET VoiceBoothSeparator POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different "${f}" "$<TARGET_FILE_DIR:VoiceBoothSeparator>")
    endforeach()

    # 本体の隣（Mac は VoiceBooth.app/Contents/MacOS）にも置く：本体はそこから起動し、インストーラ・DMG にもそのまま入る
    add_dependencies(VoiceBooth VoiceBoothSeparator)
    add_custom_command(TARGET VoiceBooth POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:VoiceBoothSeparator>" "$<TARGET_FILE_DIR:VoiceBooth>")
    foreach(f ${VB_ORT_RUNTIME_FILES})
        add_custom_command(TARGET VoiceBooth POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different "${f}" "$<TARGET_FILE_DIR:VoiceBooth>")
    endforeach()
    target_compile_definitions(VoiceBooth PRIVATE VOICEBOOTH_HAS_SEPARATOR=1)
endif()

# ---------------------------------------------------------------------------
# 同梱するもののライセンス全文（#15）。GPL / AGPL は条文を、MIT / BSD / OFL は著作権表示を、配る物と一緒に渡す。
# 取ってきたソース（JUCE・Rubber Band・ONNX Runtime・Monocypher・ASIO SDK）の中のファイルをそのまま写す（中身を変えない）。
# 本体の隣の licenses/（Mac は VoiceBooth.app/Contents/Resources/licenses）に置き、インストーラー・DMG にもそのまま入る。
# 「このアプリについて」の［ライセンス全文］はこのフォルダを開く
# ---------------------------------------------------------------------------
if(VOICEBOOTH_JUCE_DIR)
    set(VB_JUCE_SOURCE "${VOICEBOOTH_JUCE_DIR}")
else()
    set(VB_JUCE_SOURCE "${juce_SOURCE_DIR}")
endif()
set(VB_LICENSES_DIR "${CMAKE_BINARY_DIR}/licenses")
file(REMOVE_RECURSE "${VB_LICENSES_DIR}")
# 「元のファイル|置く名前」
set(vb_license_files
    "${CMAKE_SOURCE_DIR}/LICENSE|VoiceBooth-AGPL-3.0.txt"
    "${VB_JUCE_SOURCE}/LICENSE.md|JUCE-LICENSE.md"
    "${VB_RUBBERBAND_SOURCE}/COPYING|RubberBand-GPL-2.0.txt"
    "${monocypher_SOURCE_DIR}/LICENCE.md|Monocypher-LICENCE.md"
    "${CMAKE_SOURCE_DIR}/third_party/minimp3/LICENSE|minimp3-CC0.txt"
    "${CMAKE_SOURCE_DIR}/resources/fonts/OFL-IBMPlexSansJP.txt|IBMPlexSansJP-OFL-1.1.txt"
    "${CMAKE_SOURCE_DIR}/resources/fonts/OFL-IBMPlexMono.txt|IBMPlexMono-OFL-1.1.txt"
    "${CMAKE_SOURCE_DIR}/third_party/licenses/GPL-3.0.txt|GPL-3.0.txt")
if(VB_ORT)
    list(APPEND vb_license_files "${VB_ORT}/LICENSE|ONNXRuntime-MIT.txt"
                                 "${VB_ORT}/ThirdPartyNotices.txt|ONNXRuntime-ThirdPartyNotices.txt")
endif()
if(VB_ASIO_SDK)
    list(APPEND vb_license_files "${VB_ASIO_SDK}/LICENSE.txt|ASIO-SDK-LICENSE.txt")
endif()
foreach(entry ${vb_license_files})
    string(REPLACE "|" ";" pair "${entry}")
    list(GET pair 0 vb_src)
    list(GET pair 1 vb_dst)
    if(NOT EXISTS "${vb_src}")
        message(FATAL_ERROR "License file not found: ${vb_src}")
    endif()
    configure_file("${vb_src}" "${VB_LICENSES_DIR}/${vb_dst}" COPYONLY)
endforeach()
configure_file("${CMAKE_SOURCE_DIR}/third_party/licenses/README.txt" "${VB_LICENSES_DIR}/README.txt" COPYONLY)
if(APPLE)
    set(vb_licenses_dest "$<TARGET_FILE_DIR:VoiceBooth>/../Resources/licenses")
else()
    set(vb_licenses_dest "$<TARGET_FILE_DIR:VoiceBooth>/licenses")
endif()
add_custom_command(TARGET VoiceBooth POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory "${VB_LICENSES_DIR}" "${vb_licenses_dest}")

# ---------------------------------------------------------------------------
# テスト（ctest）。DESIGN 17：長さ・サンプル位置の正しさを機械で確かめる
# ---------------------------------------------------------------------------
if(VOICEBOOTH_TESTS)
    enable_testing()
    juce_add_console_app(VoiceBoothTests PRODUCT_NAME "VoiceBoothTests" NEEDS_CURL ${VB_NEEDS_CURL})
    target_sources(VoiceBoothTests PRIVATE
        tests/TestMain.cpp
        tests/ReasonsTests.cpp
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
        tests/DeliveryPackTests.cpp
        tests/SeparationTests.cpp
        tests/ModelDownloadTests.cpp
        tests/TakeStatsTests.cpp
        tests/KeySuggestTests.cpp
        tests/GuideCleanTests.cpp
        tests/RmvpeTests.cpp
        tests/AnalysisCacheTests.cpp
        tests/ClickTests.cpp
        tests/GameSoundsTests.cpp
        tests/UpdateCheckTests.cpp
        tests/TakeCompareTests.cpp
        tests/PitchShiftTests.cpp
        tests/BackgroundTests.cpp
        ${VOICEBOOTH_AUDIO_SOURCES}
        ${VOICEBOOTH_RECORD_SOURCES}
        ${VOICEBOOTH_SONG_SOURCES}
        ${VOICEBOOTH_SKIN_SOURCES})
    target_include_directories(VoiceBoothTests PRIVATE app)
    target_compile_definitions(VoiceBoothTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_MODAL_LOOPS_PERMITTED=1    # 非同期の通知をテストで回すため
        VOICEBOOTH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
        VOICEBOOTH_VERSION_STRING="${VB_FULL_VERSION}")
    if(MSVC)
        target_compile_options(VoiceBoothTests PRIVATE /utf-8)
    endif()
    target_link_libraries(VoiceBoothTests
        PRIVATE
            vb_rubberband
            vb_monocypher
            juce::juce_cryptography
            juce::juce_audio_formats
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    if(WIN32)
        target_link_libraries(VoiceBoothTests PRIVATE mfplat mfreadwrite mfuuid ole32)
    endif()
    add_test(NAME VoiceBoothTests COMMAND VoiceBoothTests)

    # UiSession（状態・保存・開く）を音声の機器なしで動かすテスト（2026-10-04）。偽のエンジン（tests/session/FakeEngine.h）をつなぎ、
    # 「開く → 変える → 保存 → 開き直す」で中身が戻るかを確かめる。UI のソースを全部入れるので別の実行ファイルにする
    juce_add_console_app(VoiceBoothSessionTests PRODUCT_NAME "VoiceBoothSessionTests" NEEDS_CURL ${VB_NEEDS_CURL})
    target_sources(VoiceBoothSessionTests PRIVATE
        tests/TestMain.cpp
        tests/session/SessionSaveTests.cpp
        tests/session/SessionSeparationTests.cpp
        ${VOICEBOOTH_AUDIO_SOURCES}
        ${VOICEBOOTH_RECORD_SOURCES}
        ${VOICEBOOTH_SONG_SOURCES}
        ${VOICEBOOTH_SKIN_SOURCES}
        ${VOICEBOOTH_UI_SOURCES})
    target_include_directories(VoiceBoothSessionTests PRIVATE app tests)
    target_compile_definitions(VoiceBoothSessionTests PRIVATE
        VOICEBOOTH_UI_MOCK=0
        VB_TEST_CATEGORY="VoiceBoothSession"
        JUCE_WEB_BROWSER=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_APPLICATION_NAME_STRING="VoiceBooth"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
        VOICEBOOTH_VERSION_STRING="${VB_FULL_VERSION}")
    if(MSVC)
        target_compile_options(VoiceBoothSessionTests PRIVATE /utf-8)
    endif()
    target_link_libraries(VoiceBoothSessionTests
        PRIVATE
            VoiceBoothFonts
            VoiceBoothI18n
            vb_rubberband
            vb_monocypher
            juce::juce_cryptography
            juce::juce_gui_basics
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    if(WIN32)
        target_link_libraries(VoiceBoothSessionTests PRIVATE mfplat mfreadwrite mfuuid ole32)
    endif()
    add_test(NAME VoiceBoothSessionTests COMMAND VoiceBoothSessionTests)
endif()
