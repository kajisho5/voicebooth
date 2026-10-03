#include "Metronome.h"

namespace vb::audio
{
juce::int64 Metronome::Grid::beatSample (juce::int64 beat) const
{
    return downbeat + (juce::int64) std::llround ((double) beat * samplesPerBeat);
}

juce::int64 Metronome::Grid::firstBeatFrom (double pos) const
{
    // 拍の位置は丸めて置くので、割り算の答えの前後を確かめる（ちょうど拍の上の位置は、その拍）
    auto k = (juce::int64) std::ceil ((pos - (double) downbeat) / samplesPerBeat);
    while ((double) beatSample (k - 1) >= pos) --k;
    while ((double) beatSample (k) < pos)      ++k;
    return k;
}

bool Metronome::Grid::isDownbeat (juce::int64 beat) const
{
    const auto n = (juce::int64) beatsPerBar;
    return ((beat % n) + n) % n == 0;
}

void Metronome::setGrid (Grid g)
{
    spb = g.valid() ? g.samplesPerBeat : 0.0;
    downbeat = g.downbeat;
    perBar = juce::jmax (1, g.beatsPerBar);
    ++gridSerial;
}

Metronome::Grid Metronome::getGrid() const
{
    Grid g;
    g.samplesPerBeat = spb.load();
    g.downbeat = downbeat.load();
    g.beatsPerBar = perBar.load();
    return g;
}

void Metronome::prepare (double outputSampleRate)
{
    rate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
    needResync = true;
    left = 0;
    env = 0.0f;
}

float Metronome::voice() noexcept
{
    if (left <= 0)
        return 0.0f;
    // 頭が立った音（cos から始める）：1 サンプル目から最大。拍の位置がサンプル単位で聞き取れる
    const auto v = amp * env * (float) std::cos (phase);
    phase += phaseStep;
    env *= envMul;
    --left;
    return v;
}

float Metronome::next (double songPos, bool audible) noexcept
{
    if (const auto serial = gridSerial.load (std::memory_order_relaxed); serial != seenSerial)
    {
        seenSerial = serial;
        needResync = true;
    }

    const auto g = getGrid();
    if (! g.valid())
    {
        lastPos = songPos;
        return voice();
    }

    // 位置が戻った（ループ・シーク）・大きく飛んだ：次の拍を探し直す。ふつうは 1 サンプルで 1.5 × (384 / 44.1) 未満しか進まない
    if (needResync || songPos < lastPos || songPos > lastPos + 64.0)
    {
        nextBeat = g.firstBeatFrom (songPos);
        nextBeatPos = g.beatSample (nextBeat);
        needResync = false;
    }
    lastPos = songPos;

    if (songPos >= (double) nextBeatPos)
    {
        const bool accent = g.isDownbeat (nextBeat);
        do
        {
            ++nextBeat;
            nextBeatPos = g.beatSample (nextBeat);
        }
        while ((double) nextBeatPos <= songPos);   // 1 サンプルで拍をいくつも越えた（極端な値）時も 1 回だけ鳴らす

        if (audible)
        {
            const auto hz = accent ? accentHz : beatHz;
            amp = (accent ? accentPeak : beatPeak) * level.load (std::memory_order_relaxed);
            phase = 0.0;
            phaseStep = juce::MathConstants<double>::twoPi * hz / rate;
            env = 1.0f;
            envMul = (float) std::exp (-1.0 / (decaySeconds * rate));
            left = (int) std::lround (lengthSeconds * rate);
        }
    }
    return voice();
}
} // namespace vb::audio
