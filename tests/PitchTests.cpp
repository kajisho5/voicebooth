#include "audio/PitchTracker.h"

/*  自分の声のリアルタイムピッチ（B8）。合成した声（倍音つき・ビブラート・雑音）で音程と位置を確かめる */

namespace vb
{
namespace pitch = audio::pitch;
using audio::int64;

namespace
{
    constexpr double twoPi = 6.283185307179586;

    /** 声らしい音：基音 + 倍音（2 倍音の方が強い。オクターブを取り違えやすい形）。ph は基音の位相 */
    float voicePhase (double ph, float level = 0.3f)
    {
        return level * (float) (0.5 * std::sin (ph) + 0.8 * std::sin (2.0 * ph) + 0.4 * std::sin (3.0 * ph) + 0.2 * std::sin (4.0 * ph)) / 1.9f;
    }

    float voice (double hz, double t, float level = 0.3f) { return voicePhase (twoPi * hz * t, level); }

    float cents (float midiA, float midiB) { return (midiA - midiB) * 100.0f; }

    /** トラッカーに通して点を集める（ブロック 512、曲は sample 0 から鳴っているつもり） */
    std::vector<audio::PitchFrame> track (double rate, double seconds, const std::function<float (double)>& signal,
                                          int64 songOffset = 0, bool playing = true)
    {
        audio::PitchTracker t;
        t.prepare (rate);
        const int block = 512;
        const auto total = (int64) (seconds * rate);
        std::vector<float> in (block);
        for (int64 a = 0; a < total; a += block)
        {
            for (int i = 0; i < block; ++i)
                in[(size_t) i] = signal ((double) (a + i) / rate);
            t.push (in.data(), block, songOffset + a, playing ? block : 0);
            if ((a / block) % 32 == 0)
                juce::Thread::sleep (1);   // 検出のスレッドに回す時間（リングは 2 秒分あるので落ちない）
        }
        std::vector<audio::PitchFrame> out;
        for (int i = 0; i < 200; ++i)
        {
            juce::Thread::sleep (10);
            t.pop (out);
            if (! out.empty() && out.back().songSample >= songOffset + total - (int64) (0.08 * rate))
                break;
        }
        t.release();
        t.pop (out);
        return out;
    }
}

class PitchTests : public juce::UnitTest
{
public:
    PitchTests() : juce::UnitTest ("Pitch", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("yin: voice-like tones from 82 to 1000 Hz within 5 cents, high confidence, no octave errors");
        {
            const double fs = 16000.0;
            const int window = 512, minLag = 14, maxLag = 291;
            std::vector<float> x ((size_t) (window + maxLag + 1)), scratch;
            for (auto hz : { 82.4, 110.0, 196.0, 261.6, 440.0, 659.3, 1000.0 })
            {
                for (size_t i = 0; i < x.size(); ++i)
                    x[i] = voice (hz, (double) i / fs);
                const auto e = pitch::yin (x.data(), window, minLag, maxLag, scratch);
                expectGreaterThan (e.lag, 0.0);
                const auto got = pitch::hzToMidi (fs / e.lag), want = pitch::hzToMidi (hz);
                expectLessThan (std::abs (cents (got, want)), 5.0f, juce::String (hz) + " Hz");
                expectGreaterThan (e.confidence, 0.85f);
            }
        }

        beginTest ("yin: white noise has low confidence");
        {
            const int window = 512, minLag = 14, maxLag = 291;
            std::vector<float> x ((size_t) (window + maxLag + 1)), scratch;
            juce::Random r (7);
            for (auto& v : x) v = r.nextFloat() * 2.0f - 1.0f;
            expectLessThan (pitch::yin (x.data(), window, minLag, maxLag, scratch).confidence, 0.5f);
        }

        beginTest ("tracker: 10 ms points, right note at 44.1 / 48 / 96 / 192 kHz, positions follow the song");
        {
            for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                const auto pts = track (rate, 1.5, [] (double t) { return voice (220.0, t); }, 100000);
                expectGreaterThan ((int) pts.size(), 120, juce::String (rate));
                int good = 0;
                for (size_t i = 1; i < pts.size(); ++i)
                {
                    expectGreaterThan (pts[i].songSample, pts[i - 1].songSample);
                    if (pts[i].confidence >= 0.5f && std::abs (cents (pts[i].midi, pitch::hzToMidi (220.0))) < 10.0f)
                        ++good;
                }
                expectGreaterThan (good, (int) pts.size() * 9 / 10, juce::String (rate));
                const auto step = (double) (pts.back().songSample - pts.front().songSample) / (double) (pts.size() - 1);
                expectWithinAbsoluteError (step / rate, 0.01, 0.0005);
                expectGreaterOrEqual (pts.front().songSample, (int64) 100000);
            }
        }

        beginTest ("tracker: a note change lands at the right song position (within 20 ms), a vibrato is followed");
        {
            const double rate = 48000.0;
            double phase = 0.0;   // 周波数を積分して位相にする（揺れても音程が正しい）
            const auto pts = track (rate, 2.0, [&phase, rate] (double t)
            {
                const auto hz = t < 1.0 ? 196.0 : 293.7 * std::pow (2.0, 0.5 * std::sin (twoPi * 5.5 * t) / 12.0);   // 1 秒から D4、±50 セントの揺れ
                phase += twoPi * hz / rate;
                return voicePhase (phase);
            });
            const auto d4 = pitch::hzToMidi (293.7);
            int64 change = -1;
            float maxDev = 0.0f;
            for (auto& p : pts)
            {
                if (change < 0 && p.confidence >= 0.5f && std::abs (cents (p.midi, d4)) < 60.0f)
                    change = p.songSample;
                if (p.songSample > (int64) (1.2 * rate) && p.confidence >= 0.5f)
                    maxDev = juce::jmax (maxDev, std::abs (cents (p.midi, d4)));
            }
            expectWithinAbsoluteError ((double) change / rate, 1.0, 0.02);
            expectGreaterThan (maxDev, 35.0f);    // 揺れが平らにならない
            expectLessThan (maxDev, 65.0f);
        }

        beginTest ("tracker: a jump between notes (E4 -> G4, A2 -> A3) leaves no confident point far from both notes");
        {
            const double rate = 48000.0;
            for (auto [a, b] : { std::pair<double, double> { 329.6, 392.0 }, { 110.0, 220.0 }, { 440.0, 196.0 } })
            {
                double phase = 0.0;
                const auto pts = track (rate, 1.2, [&phase, rate, a = a, b = b] (double t)
                {
                    phase += twoPi * (t < 0.6 ? a : b) / rate;
                    return voicePhase (phase);
                });
                const auto ma = pitch::hzToMidi (a), mb = pitch::hzToMidi (b);
                int stray = 0;
                for (auto& p : pts)
                    if (p.confidence >= 0.5f && std::abs (p.midi - ma) > 1.0f && std::abs (p.midi - mb) > 1.0f)
                        ++stray;
                expectEquals (stray, 0, juce::String (a) + " -> " + juce::String (b));
            }
        }

        beginTest ("tracker: quiet input is unvoiced; while the song is stopped no points come out");
        {
            const double rate = 48000.0;
            const auto quiet = track (rate, 0.6, [] (double t) { return voice (220.0, t, 0.001f); });
            for (auto& p : quiet)
                expectEquals (p.confidence, 0.0f);
            const auto stopped = track (rate, 0.6, [] (double t) { return voice (220.0, t); }, 0, false);
            expect (stopped.empty());
        }
    }
};

static PitchTests pitchTests;
} // namespace vb
