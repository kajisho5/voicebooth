#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "WaveformOverview.h"

/*  曲ファイルを開く（DESIGN 7.1 / Phase B1）
    形式・長さ・サンプリングレート・チャンネルを調べ、波形の概形を作る。
    重いのでバックグラウンドスレッドで行う（UI を止めない。DESIGN 11.3）。

    B1 の範囲
      - 読むだけ。音声デバイスは開かない、再生しない（B2）
      - 内部 wav（float32）への変換・プロジェクトへのコピーは B14 で行う
      - 対応形式は OS の読み手しだい（wav / aiff / flac / ogg は全 OS。
        mp3 は全 OS で同梱の minimp3（頭の位置を OS で変えない）、
        m4a は Win（Media Foundation）と Mac（Core Audio）。読み手の一式は registerSongFormats() */

namespace vb::audio
{
struct SongInfo
{
    juce::File file;
    juce::String formatName;     // "WAV file" など（読み手の名前。表示はしない）
    double sampleRate = 0.0;
    int numChannels = 0;
    int bitsPerSample = 0;
    bool floatingPoint = false;
    int64 lengthSamples = 0;

    juce::String extension() const { return file.getFileExtension().trimCharactersAtStart (".").toLowerCase(); }
};

struct LoadResult
{
    enum class Error { none, notFound, unsupported, empty, readFailed, cancelled };

    Error error = Error::none;
    SongInfo info;
    std::shared_ptr<const WaveformOverview> overview;

    bool ok() const { return error == Error::none; }
};

/** 翻訳キー（load.error.*） */
const char* errorKey (LoadResult::Error);

/** 曲として受け付ける拡張子（DESIGN 7.1）。読めるかどうかは開いてみるまで分からない */
const juce::StringArray& songExtensions();
bool hasSongExtension (const juce::File&);
juce::String songWildcard();   // "*.wav;*.flac;..."

/** 曲を開くための読み手一式。先に登録したものが優先：
    minimp3（mp3）→ Media Foundation（Windows の m4a / aac）→ JUCE 標準（wav / aiff / flac / ogg、Mac は Core Audio） */
void registerSongFormats (juce::AudioFormatManager&);

/** 同期で読む（テスト・スレッド本体から使う）
    progress は 0..1 を受け取り、false を返すと中止 */
LoadResult loadSong (const juce::File&, juce::AudioFormatManager&,
                     const std::function<bool (float)>& progress = {});

/** バックグラウンドで読む。結果はメッセージスレッドで onDone に届く
    所有者が先に消えても安全（デストラクタでスレッドを止め、通知も取り消す） */
class SongLoader : private juce::Thread, private juce::AsyncUpdater
{
public:
    SongLoader();
    ~SongLoader() override;

    /** 読み込み中なら中止してから始める */
    void start (const juce::File&, std::function<void (LoadResult)> onDone);
    void cancel();

    bool isLoading() const { return isThreadRunning(); }
    float getProgress() const { return progress.load(); }

private:
    void run() override;
    void handleAsyncUpdate() override;

    juce::AudioFormatManager formats;
    juce::File file;
    std::function<void (LoadResult)> callback;
    std::atomic<float> progress { 0.0f };

    juce::CriticalSection lock;
    LoadResult result;   // lock で保護

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongLoader)
};
} // namespace vb::audio
