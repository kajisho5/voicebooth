#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

/*  モニターの帯のメーター（オフボ・お手本・クリック。2026-10-02）
    フェーダーの後（ミュート・ソロも込み）で、耳に行く量を測る。オーディオスレッドがブロックの最大値を入れ、UI が dB で読む。

    - 上がる時はすぐ、下がる時は 20 dB/秒（入力メーターと同じ落ち方。InputMeter）
    - UI とは atomic 1 つで受け渡す（ロック・確保なし。DESIGN 17）
    - 帯の横の LED は -48〜0 dBFS を 0..1 に並べる（meterFraction。自分の声の帯と同じ目盛り） */

namespace vb::audio
{
class LevelFollower
{
public:
    static constexpr float decayDbPerSecond = 20.0f;
    static constexpr float floorDb = -100.0f;

    /** デバイスが始まる時（オーディオスレッドは止まっている） */
    void prepare (double sampleRate) noexcept
    {
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        peak = 0.0f;
        out.store (0.0f);
    }

    /** オーディオスレッド：ブロック（numSamples）の最大値（直線の振幅） */
    void push (float blockPeak, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;
        if (! std::isfinite (blockPeak))
            blockPeak = 0.0f;
        const auto fall = (float) std::pow (10.0, -(double) decayDbPerSecond * ((double) numSamples / rate) / 20.0);
        peak = juce::jmax (blockPeak, peak * fall);
        if (peak < 1.0e-5f)   // -100 dBFS より下は 0（デノーマルを避ける）
            peak = 0.0f;
        out.store (peak, std::memory_order_relaxed);
    }

    /** UI：いまの値（dBFS、下限 floorDb） */
    float readDb() const { return juce::Decibels::gainToDecibels (out.load (std::memory_order_relaxed), floorDb); }

private:
    double rate = 48000.0;
    float peak = 0.0f;                 // オーディオスレッドだけ
    std::atomic<float> out { 0.0f };
};

/** 帯の横の LED メーター：-48〜0 dBFS を 0..1 に（-48 より下は消灯、0 を超えたら振り切り） */
inline float meterFraction (float dbfs)
{
    return juce::jmap (juce::jlimit (-48.0f, 0.0f, dbfs), -48.0f, 0.0f, 0.0f, 1.0f);
}
} // namespace vb::audio
