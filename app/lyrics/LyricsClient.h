#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include "song/LyricsAlign.h"

/*  歌の認識プロセス VoiceBoothLyrics を本体から動かす（DESIGN 11.3 / B17）
    子プロセスで起動し、標準出力の ready / progress / piece / done / error を読む。入力は 16 kHz モノラルの WAV（お手本から取り出した声）。
    歌詞の文字は最初の手がかり（initial prompt）として渡す。中止は子プロセスを止める。コールバックはメッセージスレッドで呼ぶ */

namespace vb::lyrics
{
class LyricsClient : private juce::Thread
{
public:
    LyricsClient();
    ~LyricsClient() override;

    /** 本体の隣の VoiceBoothLyrics（無ければ存在しないファイル） */
    static juce::File executable();
    /** モデルのフォルダ（環境変数 VB_LYRICS_MODEL があればそこ、無ければアプリのデータの Models/lyrics/<id>） */
    static juce::File modelFolder();
    static juce::String modelId();
    static juce::File modelFile() { return modelFolder().getChildFile ("ggml-small.bin"); }
    static bool modelInstalled() { return modelFile().existsAsFile(); }
    /** このパソコンで動かせる（x86_64 は AVX2 が要る） */
    static bool cpuSupported();
    /** 認識に要る物（実行ファイル・モデル・CPU）がそろっている */
    static bool available();
    /** 歌詞の文字から認識の言語（ja / ko / zh / en） */
    static juce::String languageFor (const juce::String& lyricsText);

    struct Callbacks
    {
        std::function<void (float progress)> progress;
        std::function<void (bool ok, const juce::String& error, std::vector<song::RecognizedPiece> pieces)> done;   // "stopped" = 止めた
    };

    bool start (const juce::File& input, const juce::String& prompt, const juce::String& language, Callbacks);
    void stop();
    bool isBusy() const { return isThreadRunning(); }

private:
    void run() override;
    void finish (bool ok, const juce::String& error);

    juce::File input, promptFile;
    juce::String language;
    Callbacks callbacks;
    std::vector<song::RecognizedPiece> pieces;
    std::unique_ptr<juce::ChildProcess> child;
    juce::CriticalSection childLock;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
} // namespace vb::lyrics
