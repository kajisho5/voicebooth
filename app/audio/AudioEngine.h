#pragma once

#include <juce_core/juce_core.h>
#include <memory>

/*  音声エンジンの境界（DESIGN 7）
    UI はこのインターフェース越しにだけ音声へ触る。B2 で再生（オフボ）を結線。入力は B3 以降。

    ルール（DESIGN 17）
      - オーディオスレッドでメモリ確保・ファイル I/O・長いロック待ちをしない
      - サンプル位置は int64
      - モニターバスと録音バスは分離（モニターリバーブは録音に入れない） */

namespace vb::audio
{
using int64 = juce::int64;
struct SongAudio;

struct InputLevel
{
    float peakDb = -100.0f;
    float rmsDb  = -100.0f;
    bool  clipped = false;
};

/** 出力デバイスの状態（ステータスバー用） */
struct OutputStatus
{
    bool open = false;
    juce::String deviceName, typeName, error;
    double sampleRate = 0.0;
    int bufferSize = 0;
    bool converting = false;    // 曲と SR が違い、試聴用に変換している
};

class AudioEngine
{
public:
    virtual ~AudioEngine() = default;

    // 曲（B2）
    virtual void setSong (std::shared_ptr<const SongAudio>) = 0;
    virtual bool hasSong() const = 0;

    // 輸送
    virtual void  play() = 0;
    virtual void  stop() = 0;
    virtual bool  isPlaying() const = 0;
    virtual void  seek (int64 sample) = 0;
    virtual int64 getPlayheadSample() const = 0;
    virtual bool  consumeReachedEnd() = 0;
    virtual void  setLoop (int64 startSample, int64 endSample, bool enabled) = 0;

    // モニター（オフボ）。fader は 0..1（0.75 = 0 dB）
    virtual void setBackingLevel (float fader, bool muted) = 0;

    virtual OutputStatus getOutputStatus() const = 0;

    // 入力（B3 以降）
    virtual InputLevel getInputLevel() const { return {}; }
    virtual int64 getLatencyCompensationSamples() const { return 0; }
};
} // namespace vb::audio
