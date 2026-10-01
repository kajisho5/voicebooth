#include "MonitorMixer.h"

namespace vb::audio
{
void MonitorMixer::prepare (double sampleRate, int maxBlockSize)
{
    maxBlock = juce::jmax (64, maxBlockSize);
    scratch.setSize (3, maxBlock, false, true, false);

    // 小さめの部屋。明るすぎない（歌いやすさ優先。DESIGN 18.1「モニター薄いリバーブ」）
    juce::Reverb::Parameters p;
    p.roomSize = 0.55f;
    p.damping = 0.45f;
    p.wetLevel = 0.33f;
    p.dryLevel = 0.0f;      // 素の声は自分のフェーダーで別に足す
    p.width = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);
    reverb.setSampleRate (sampleRate);
    reverb.reset();

    smoothedGain.reset (sampleRate, 0.02);   // 20 ms でなめらかに
    smoothedSend.reset (sampleRate, 0.05);
    smoothedGain.setCurrentAndTargetValue (muted.load() ? 0.0f : gain.load());
    smoothedSend.setCurrentAndTargetValue (reverbAmount.load());

    tailLength = (juce::int64) (sampleRate * tailSeconds);
    tailLeft = 0;
    prepared = true;
}

void MonitorMixer::reset()
{
    reverb.reset();
    tailLeft = 0;
}

void MonitorMixer::process (const float* input, float* const* out, int numChannels, int numSamples) noexcept
{
    if (! prepared || out == nullptr || numChannels <= 0)
        return;

    for (int done = 0; done < numSamples;)
    {
        const auto n = juce::jmin (maxBlock, numSamples - done);
        float* shifted[2] = { out[0] != nullptr ? out[0] + done : nullptr,
                              numChannels > 1 && out[1] != nullptr ? out[1] + done : nullptr };
        processChunk (input != nullptr ? input + done : nullptr, shifted, juce::jmin (2, numChannels), n);
        done += n;
    }
}

void MonitorMixer::processChunk (const float* input, float* const* out, int numChannels, int n) noexcept
{
    smoothedGain.setTargetValue (muted.load() ? 0.0f : gain.load());
    smoothedSend.setTargetValue (reverbAmount.load());

    // 何も鳴らす物が無い（ミュートで下がりきって、リバーブも鳴り終わった）なら何もしない。
    // 送りはフェーダーの後なので、自分が 0 ならリバーブにも入らない
    const bool gainSilent = ! smoothedGain.isSmoothing() && smoothedGain.getTargetValue() <= 0.0f;
    if (gainSilent && tailLeft <= 0)
        return;

    auto* dry = scratch.getWritePointer (0);
    auto* wetL = scratch.getWritePointer (1);
    auto* wetR = scratch.getWritePointer (2);

    // 自分（フェーダー後）と、リバーブへの送り
    bool anySend = false;
    for (int i = 0; i < n; ++i)
    {
        const auto x = input != nullptr ? input[i] : 0.0f;
        dry[i] = x * smoothedGain.getNextValue();
        const auto send = smoothedSend.getNextValue();
        wetL[i] = dry[i] * send;
        anySend = anySend || send > 0.0f;
    }

    // リバーブ：送りがある間と、止めてから尾が鳴り終わるまで回す
    bool wet = false;
    if (anySend)
        tailLeft = tailLength;
    if (anySend || tailLeft > 0)
    {
        juce::FloatVectorOperations::copy (wetR, wetL, n);
        reverb.processStereo (wetL, wetR, n);
        tailLeft = anySend ? tailLength : juce::jmax ((juce::int64) 0, tailLeft - n);
        wet = true;
    }

    // 出力に足す（自分の分だけ ±1 に収める）
    for (int c = 0; c < numChannels; ++c)
    {
        auto* o = out[c];
        if (o == nullptr)
            continue;
        const auto* w = c == 0 ? wetL : wetR;
        for (int i = 0; i < n; ++i)
        {
            auto m = dry[i];
            if (wet)
                m += numChannels == 1 ? 0.5f * (wetL[i] + wetR[i]) : w[i];
            o[i] += juce::jlimit (-1.0f, 1.0f, m);
        }
    }
}
} // namespace vb::audio
