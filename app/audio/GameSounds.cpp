#include "GameSounds.h"
#include <cmath>

namespace vb::audio
{
void GameSounds::prepare (double sr)
{
    if (sr > 0.0)
        sampleRate.store (sr);
}

void GameSounds::setBeat (double newBpm)
{
    wantBpm.store (juce::jlimit (0.0, 300.0, newBpm));
    beatSerial.fetch_add (1);
}

void GameSounds::playTone (float midi, double seconds)
{
    wantToneMidi.store (midi);
    wantToneSeconds.store (juce::jmax (0.0, seconds));
    toneSerial.fetch_add (1);
}

void GameSounds::process (float* const* outputs, int numOutputs, int numSamples) noexcept
{
    const auto sr = sampleRate.load();
    if (const auto s = beatSerial.load (std::memory_order_acquire); s != seenBeatSerial)
    {
        seenBeatSerial = s;
        bpm = wantBpm.load();
        beatCounter = 0;
        clickLeft = 0;
        rendered.store (bpm > 0.0 ? 0 : -1, std::memory_order_release);
    }
    if (const auto s = toneSerial.load (std::memory_order_acquire); s != seenToneSerial)
    {
        seenToneSerial = s;
        const auto midi = wantToneMidi.load();
        const auto freq = midi > 0.0f ? 440.0 * std::pow (2.0, (midi - 69.0) / 12.0) : 0.0;
        const auto total = freq > 0.0 ? (juce::int64) (wantToneSeconds.load() * sr) : 0;
        if (toneLeft > 0)
        {
            // 鳴っている途中：20 ms で下げきってから次へ（いきなり切り替え・止めて「プツッ」と鳴っていた。監査 2026-10-06）
            toneLeft = juce::jmin (toneLeft, juce::jmax ((juce::int64) 1, (juce::int64) (0.02 * sr)));
            nextToneFreq = freq;
            nextToneTotal = total;
        }
        else
        {
            toneFreq = freq;
            toneTotal = toneLeft = total;
            tonePhase = 0.0;
            nextToneTotal = 0;
        }
    }
    if ((bpm <= 0.0 && toneLeft <= 0 && nextToneTotal <= 0) || outputs == nullptr || numOutputs <= 0)
        return;

    const auto spb = bpm > 0.0 ? sr * 60.0 / bpm : 0.0;
    const auto clickLen = (juce::int64) (clickSeconds * sr);
    const auto fade = juce::jmax ((juce::int64) 1, (juce::int64) (0.02 * sr));
    for (int i = 0; i < numSamples; ++i)
    {
        float v = 0.0f;
        if (bpm > 0.0)
        {
            // 拍の頭（n 拍目は round(n × 1 拍のサンプル数)）に来たら鳴らし始める。4 拍ごとに高い音
            const auto beat = (juce::int64) std::llround ((double) beatCounter / spb);   // いちばん近い拍
            if ((juce::int64) std::llround ((double) beat * spb) == beatCounter)
            {
                clickLeft = clickLen;
                clickPhase = 0.0;
                clickFreq = beat % 4 == 0 ? 1760.0 : 1320.0;
            }
            if (clickLeft > 0)
            {
                const auto age = (double) (clickLen - clickLeft) / (double) clickLen;   // 0..1
                v += clickGain * (float) (std::sin (clickPhase) * std::exp (-6.0 * age));
                clickPhase += juce::MathConstants<double>::twoPi * clickFreq / sr;
                --clickLeft;
            }
            ++beatCounter;
        }
        if (toneLeft > 0)
        {
            const auto done = toneTotal - toneLeft;
            const auto env = (float) juce::jmin (1.0, juce::jmin ((double) done, (double) toneLeft) / (double) fade);
            v += toneGain * env * (float) std::sin (tonePhase);
            tonePhase += juce::MathConstants<double>::twoPi * toneFreq / sr;
            if (tonePhase > juce::MathConstants<double>::twoPi) tonePhase -= juce::MathConstants<double>::twoPi;
            --toneLeft;
        }
        else if (nextToneTotal > 0)
        {
            toneFreq = nextToneFreq;
            toneTotal = toneLeft = nextToneTotal;
            tonePhase = 0.0;
            nextToneTotal = 0;
        }
        for (int c = 0; c < numOutputs; ++c)
            if (outputs[c] != nullptr)
                outputs[c][i] += v;
    }
    if (bpm > 0.0)
        rendered.store (beatCounter, std::memory_order_release);
}
} // namespace vb::audio
