#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioEngine.h"
#include <array>
#include <atomic>

/*  入力メーター（DESIGN 4.8 / 5 / Phase B3）。デバイスに依存しない中身
    process() をオーディオスレッドから、read() / resetClip() / reset() をメッセージスレッドから呼ぶ。

    - ピーク：ブロックの最大値。下がる時は 20 dB/秒でゆっくり
    - ホールド：最大値を 1.5 秒保ち、その後 20 dB/秒で下がる
    - RMS：直近 300 ms（10 ms × 30 区切り）の二乗平均。区切りが埋まるたびに更新
    - クリップ：-0.1 dBFS 以上のサンプルが 1 つでもあれば点灯。消すまで残る
    - 完全な無音：0 ちょうどが 2 秒続く（マイクの許可なし・ミュートの疑い。静かな部屋でも雑音があるので 0 にはならない）
    時間はすべて秒で決め、SR とブロックの長さに左右されない。

    ルール（DESIGN 17）：process() はメモリ確保・ロックをしない。UI とは atomic だけで受け渡す */

namespace vb::audio
{
class InputMeter
{
public:
    static constexpr float  holdSeconds      = 1.5f;
    static constexpr float  decayDbPerSecond = 20.0f;
    static constexpr int    rmsSlices        = 30;      // 10 ms × 30 = 300 ms
    static constexpr double sliceSeconds     = 0.01;
    static constexpr float  clipDb           = -0.1f;
    static constexpr double silenceSeconds   = 2.0;
    static constexpr float  floorDb          = -100.0f;

    /** デバイスが始まる前に呼ぶ（オーディオスレッドは止まっている）。状態も消す */
    void prepare (double sampleRate);

    /** オーディオスレッド。samples が nullptr なら無音として数える */
    void process (const float* samples, int numSamples) noexcept;

    /** UI：いまの値（dBFS。下限 floorDb） */
    InputLevel read() const;

    /** UI：クリップ表示を消す */
    void resetClip() { clip = false; }

    /** UI：全部消す（デバイスの切り替え時など）。次の process() の頭で反映 */
    void reset();

    bool isDigitalSilence() const { return silent.load(); }

    /** 入力セットアップのレベル判定（DESIGN 5：-3 dBFS 超は下げて / -20 dBFS 未満は上げて / それ以外 OK） */
    enum class Verdict { low, ok, hot };
    static Verdict judge (float peakDb);

private:
    void clearState() noexcept;

    // オーディオスレッドだけが触る（prepare はオーディオが止まっている時だけ）
    double rate = 48000.0;
    int sliceLength = 480, sliceFill = 0, sliceIndex = 0;
    double sliceSum = 0.0;
    std::array<double, rmsSlices> slices {};
    float peak = 0.0f, hold = 0.0f;
    juce::int64 holdLeft = 0, zeroRun = 0;

    // UI へ渡す値（直線の振幅）
    std::atomic<float> peakOut { 0.0f }, holdOut { 0.0f }, rmsOut { 0.0f };
    std::atomic<bool> clip { false }, silent { false }, resetRequested { false };
};
} // namespace vb::audio
