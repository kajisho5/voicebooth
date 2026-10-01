#include "InputMeter.h"

namespace vb::audio
{
namespace
{
    // -100 dBFS より小さい値は 0 に（デノーマルを避ける・下限の表示をそろえる）
    constexpr float floorGain = 1.0e-5f;

    /** n サンプルぶん、decayDbPerSecond で下げる倍率 */
    float decayFactor (juce::int64 n, double rate) noexcept
    {
        const auto seconds = (double) n / rate;
        return (float) std::pow (10.0, -(double) InputMeter::decayDbPerSecond * seconds / 20.0);
    }
}

void InputMeter::prepare (double sampleRate)
{
    rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    sliceLength = juce::jmax (1, (int) std::lround (rate * sliceSeconds));
    clearState();
    resetRequested = false;
    clip = false;
}

void InputMeter::reset()
{
    resetRequested = true;
    clip = false;
    peakOut = holdOut = rmsOut = 0.0f;
    silent = false;
}

void InputMeter::clearState() noexcept
{
    sliceFill = sliceIndex = 0;
    sliceSum = 0.0;
    slices.fill (0.0);
    peak = hold = 0.0f;
    holdLeft = zeroRun = 0;
    peakOut = holdOut = rmsOut = 0.0f;
    silent = false;
}

void InputMeter::process (const float* samples, int numSamples) noexcept
{
    if (resetRequested.exchange (false))
        clearState();

    if (numSamples <= 0)
        return;

    constexpr float clipGain = 0.98855309f;   // -0.1 dBFS（clipDb）。static の初期化はロックになりうるので定数で
    float blockPeak = 0.0f;
    bool anyNonZero = false, clipped = false;

    for (int i = 0; i < numSamples; ++i)
    {
        auto x = samples != nullptr ? samples[i] : 0.0f;
        if (! std::isfinite (x)) x = 0.0f;   // 壊れた値でメーターを止めない

        const auto a = std::abs (x);
        blockPeak = juce::jmax (blockPeak, a);
        anyNonZero = anyNonZero || ! juce::exactlyEqual (x, 0.0f);
        clipped = clipped || a >= clipGain;

        // RMS：10 ms ごとに区切って二乗和を残す
        sliceSum += (double) x * (double) x;
        if (++sliceFill >= sliceLength)
        {
            slices[(size_t) sliceIndex] = sliceSum;
            sliceIndex = (sliceIndex + 1) % rmsSlices;
            sliceSum = 0.0;
            sliceFill = 0;
        }
    }

    // ピーク：上がる時はすぐ、下がる時はゆっくり
    const auto fall = decayFactor (numSamples, rate);
    peak = juce::jmax (blockPeak, peak * fall);

    // ホールド：1.5 秒保ってから下がる
    if (blockPeak >= hold)
    {
        hold = blockPeak;
        holdLeft = (juce::int64) std::llround ((double) holdSeconds * rate);
    }
    else if (holdLeft >= numSamples)
    {
        holdLeft -= numSamples;
    }
    else
    {
        hold *= decayFactor (numSamples - holdLeft, rate);
        holdLeft = 0;
    }
    hold = juce::jmax (hold, peak);

    if (peak < floorGain) peak = 0.0f;
    if (hold < floorGain) hold = 0.0f;

    double total = 0.0;
    for (auto s : slices)
        total += s;
    const auto rms = (float) std::sqrt (total / ((double) rmsSlices * (double) sliceLength));

    zeroRun = anyNonZero ? 0 : zeroRun + numSamples;

    peakOut = peak;
    holdOut = hold;
    rmsOut = rms < floorGain ? 0.0f : rms;
    silent = (double) zeroRun >= silenceSeconds * rate;
    if (clipped)
        clip = true;
}

InputLevel InputMeter::read() const
{
    InputLevel l;
    l.peakDb  = juce::Decibels::gainToDecibels (peakOut.load(), floorDb);
    l.holdDb  = juce::Decibels::gainToDecibels (holdOut.load(), floorDb);
    l.rmsDb   = juce::Decibels::gainToDecibels (rmsOut.load(), floorDb);
    l.clipped = clip.load();
    return l;
}

InputMeter::Verdict InputMeter::judge (float peakDb)
{
    if (peakDb > -3.0f)  return Verdict::hot;
    if (peakDb < -20.0f) return Verdict::low;
    return Verdict::ok;
}
} // namespace vb::audio
