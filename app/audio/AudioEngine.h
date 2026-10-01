#pragma once

#include <juce_core/juce_core.h>

/*  音声エンジンの境界（DESIGN 7）
    Phase A では「形」だけ。実装は Phase B（B2 以降）で 1 本ずつ結線する。
    UI はこのインターフェース越しにだけ音声へ触る。

    ルール（DESIGN 17）
      - オーディオスレッドでメモリ確保・ファイル I/O・長いロック待ちをしない
      - サンプル位置は int64
      - モニターバスと録音バスは分離（モニターリバーブは録音に入れない） */

namespace vb::audio
{
using int64 = juce::int64;

struct InputLevel
{
    float peakDb = -100.0f;
    float rmsDb  = -100.0f;
    bool  clipped = false;
};

class AudioEngine
{
public:
    virtual ~AudioEngine() = default;

    // 輸送
    virtual void  play() = 0;
    virtual void  stop() = 0;
    virtual void  seek (int64 sample) = 0;
    virtual int64 getPlayheadSample() const = 0;
    virtual void  setLoop (int64 startSample, int64 endSample, bool enabled) = 0;

    // 入力
    virtual InputLevel getInputLevel() const = 0;
    virtual int64 getLatencyCompensationSamples() const = 0;
};

/** Phase A 用。何もしない。音声デバイスを開かない */
class NullAudioEngine final : public AudioEngine
{
public:
    void  play() override {}
    void  stop() override {}
    void  seek (int64 s) override { playhead = s; }
    int64 getPlayheadSample() const override { return playhead; }
    void  setLoop (int64, int64, bool) override {}
    InputLevel getInputLevel() const override { return {}; }
    int64 getLatencyCompensationSamples() const override { return 0; }

private:
    int64 playhead = 0;
};
} // namespace vb::audio
