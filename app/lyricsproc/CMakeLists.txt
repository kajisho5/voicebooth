# VoiceBoothLyrics：歌の認識の別プロセス（B17）。本体の CMake から ExternalProject で作る（Mac は arm64 と x86_64 を別々に作ってつなぐ）
# whisper.cpp v1.9.4（MIT）を取得して静的にリンクする。JUCE は使わない
cmake_minimum_required(VERSION 3.22)
project(VoiceBoothLyrics LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # VC ランタイムの DLL に頼らない
endif()

# 配る物なので、作ったパソコンに合わせた命令（-march=native）は使わない。
# x86_64 は AVX2 / FMA / F16C を前提にする（2013 年ごろ以降の CPU。無いパソコンでは本体が「使えない」と出す）
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_CURL OFF CACHE BOOL "" FORCE)
set(WHISPER_SDL2 OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)      # ランタイムの DLL / dylib を増やさない（ggml の自前のスレッド）
set(GGML_BLAS OFF CACHE BOOL "" FORCE)
set(GGML_CCACHE OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
set(GGML_CUDA OFF CACHE BOOL "" FORCE)
if(APPLE AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(GGML_METAL ON CACHE BOOL "" FORCE)
    set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)   # シェーダーを実行ファイルに入れる（別のファイルを置かない）
else()
    set(GGML_METAL OFF CACHE BOOL "" FORCE)
endif()
if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR (NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$"))
    set(GGML_AVX ON CACHE BOOL "" FORCE)
    set(GGML_AVX2 ON CACHE BOOL "" FORCE)
    set(GGML_FMA ON CACHE BOOL "" FORCE)
    set(GGML_F16C ON CACHE BOOL "" FORCE)
    set(GGML_AVX512 OFF CACHE BOOL "" FORCE)
endif()

include(FetchContent)
set(VB_WHISPER_DIR "" CACHE PATH "手元の whisper.cpp（空なら v1.9.4 を取得）")
if(VB_WHISPER_DIR)
    add_subdirectory("${VB_WHISPER_DIR}" whisper EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(whisper
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
        GIT_TAG        v1.9.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(whisper)   # 作るのは VoiceBoothLyrics に要る物だけ（本体の CMake が --target で頼む）
endif()

add_executable(VoiceBoothLyrics LyricsMain.cpp)
target_link_libraries(VoiceBoothLyrics PRIVATE whisper)
if(MSVC)
    target_compile_options(VoiceBoothLyrics PRIVATE /utf-8)
endif()
install(TARGETS VoiceBoothLyrics RUNTIME DESTINATION bin)
