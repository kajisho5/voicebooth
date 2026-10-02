#include "audio/MonitorMixer.h"
#include "audio/PlaybackCore.h"

namespace vb::audio
{
namespace
{
    constexpr double rate = 48000.0;
    constexpr double twoPi = 6.283185307179586;

    /** 出力 nch 本（初期値 fill）に input を block ずつ通す。戻り値は各チャンネルの出力 */
    std::vector<std::vector<float>> run (MonitorMixer& m, const std::vector<float>& input, int nch, int block,
                                         float fill = 0.0f, bool nullInput = false)
    {
        const auto total = (int) input.size();
        std::vector<std::vector<float>> out ((size_t) nch, std::vector<float> ((size_t) total, fill));
        std::vector<float*> ptrs ((size_t) nch);
        for (int done = 0; done < total;)
        {
            const auto n = juce::jmin (block, total - done);
            for (int c = 0; c < nch; ++c)
                ptrs[(size_t) c] = out[(size_t) c].data() + done;
            m.process (nullInput ? nullptr : input.data() + done, ptrs.data(), nch, n);
            done += n;
        }
        return out;
    }

    std::vector<float> sine (double seconds, float amp)
    {
        std::vector<float> v ((size_t) (seconds * rate));
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = amp * (float) std::sin (twoPi * 441.0 * (double) i / rate);
        return v;
    }

    float maxAbs (const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX)
    {
        float m = 0.0f;
        for (size_t i = from; i < std::min (to, v.size()); ++i)
            m = std::max (m, std::abs (v[i]));
        return m;
    }
}

class MonitorMixerTests : public juce::UnitTest
{
public:
    MonitorMixerTests() : juce::UnitTest ("MonitorMixer", "VoiceBooth") {}

    void runTest() override
    {
        const auto settle = (size_t) (0.05 * rate);   // なめらかに変わり終わった後

        beginTest ("0 dB: your voice goes to L and R unchanged, added on top of the backing");
        {
            MonitorMixer m;
            m.prepare (rate, 256);
            m.setGain (1.0f);
            const auto in = sine (0.2, 0.5f);
            const auto out = run (m, in, 3, 256, 0.25f);
            float worst = 0.0f;
            for (size_t i = settle; i < in.size(); ++i)
                for (int c = 0; c < 2; ++c)
                    worst = std::max (worst, std::abs (out[(size_t) c][i] - (0.25f + in[i])));
            expectLessThan (worst, 1.0e-6f);
            expectEquals (maxAbs (out[2]) , 0.25f);   // 3 ch 目は触らない
        }

        beginTest ("fader gain follows the backing fader law (0.375 = -12 dB)");
        {
            MonitorMixer m;
            m.prepare (rate, 512);
            m.setGain (PlaybackCore::faderToGain (0.375f));
            const auto in = sine (0.2, 0.5f);
            const auto out = run (m, in, 2, 512);
            const auto ratio = maxAbs (out[0], settle) / maxAbs (in, settle);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (ratio), -12.0f, 0.05f);
        }

        beginTest ("mute ramps down smoothly (no click) and then adds nothing");
        {
            MonitorMixer m;
            m.prepare (rate, 128);
            m.setGain (1.0f);
            std::vector<float> dc ((size_t) (0.3 * rate), 0.5f);
            auto out = run (m, std::vector<float> (dc.begin(), dc.begin() + (long) settle), 2, 128);
            m.setMuted (true);
            out = run (m, dc, 2, 128);
            // 1 サンプルあたりの変化は 20 ms のランプ分まで
            float maxStep = std::abs (out[0][0] - 0.5f);
            for (size_t i = 1; i < out[0].size(); ++i)
                maxStep = std::max (maxStep, std::abs (out[0][i] - out[0][i - 1]));
            expectLessThan (maxStep, 0.5f / (float) (0.02 * rate) * 1.5f);
            expectEquals (maxAbs (out[0], (size_t) (0.03 * rate)), 0.0f);
        }

        beginTest ("no reverb: nothing rings after you stop singing");
        {
            MonitorMixer m;
            m.prepare (rate, 256);
            m.setGain (1.0f);
            auto in = sine (0.3, 0.5f);
            in.resize ((size_t) (0.6 * rate), 0.0f);
            const auto out = run (m, in, 2, 256);
            expectEquals (maxAbs (out[0], (size_t) (0.3 * rate)), 0.0f);
        }

        beginTest ("monitor reverb: a stereo tail rings after the voice stops, then dies away");
        {
            MonitorMixer m;
            m.prepare (rate, 256);
            m.setGain (1.0f);
            m.setReverb (PlaybackCore::faderToGain (0.75f));
            auto in = sine (0.3, 0.5f);
            in.resize ((size_t) ((0.3 + MonitorMixer::tailSeconds + 1.0) * rate), 0.0f);
            const auto out = run (m, in, 2, 256);

            const auto stop = (size_t) (0.3 * rate);
            expectGreaterThan (maxAbs (out[0], stop + (size_t) (0.05 * rate), stop + (size_t) (0.3 * rate)), 1.0e-3f);
            bool differs = false;   // 左右で違う（広がりがある）
            for (size_t i = stop; i < stop + 4800 && ! differs; ++i)
                differs = std::abs (out[0][i] - out[1][i]) > 1.0e-4f;
            expect (differs);
            // 尾は聞こえない所まで消える（-100 dB 未満）
            expectLessThan (maxAbs (out[0], stop + (size_t) ((MonitorMixer::tailSeconds + 0.1) * rate)), 1.0e-5f);
        }

        beginTest ("muting yourself also stops feeding the reverb (send is after the fader)");
        {
            MonitorMixer m;
            m.setGain (1.0f);
            m.setReverb (1.0f);
            m.setMuted (true);       // ミュートのままデバイスが始まる
            m.prepare (rate, 256);
            const auto out = run (m, sine (1.0, 0.5f), 2, 256);
            expectEquals (maxAbs (out[0]), 0.0f);
            expectEquals (maxAbs (out[1]), 0.0f);
        }

        beginTest ("blocks longer than prepared are split; result does not depend on block size");
        {
            const auto in = sine (0.5, 0.4f);
            MonitorMixer a, b;
            a.prepare (rate, 128);
            b.prepare (rate, 128);
            for (auto* m : { &a, &b })
            {
                m->setGain (0.8f);
                m->setReverb (0.5f);
            }
            const auto big = run (a, in, 2, 4096);     // 準備した 128 より長いブロック
            const auto small = run (b, in, 2, 128);
            float worst = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (size_t i = 0; i < in.size(); ++i)
                    worst = std::max (worst, std::abs (big[(size_t) c][i] - small[(size_t) c][i]));
            expectLessThan (worst, 1.0e-6f);
        }

        beginTest ("your voice is limited to +-1 (ear protection); the backing is not touched");
        {
            MonitorMixer m;
            m.prepare (rate, 256);
            m.setGain (PlaybackCore::faderToGain (1.0f));   // +6 dB
            std::vector<float> loud ((size_t) (0.2 * rate), 0.9f);
            const auto out = run (m, loud, 2, 256, 0.3f);
            expectLessOrEqual (maxAbs (out[0], settle), 1.3f + 1.0e-6f);
            expectGreaterOrEqual (maxAbs (out[0], settle), 1.3f - 1.0e-6f);
        }

        beginTest ("mono output, missing input, unprepared mixer");
        {
            MonitorMixer m;
            m.prepare (rate, 256);
            m.setGain (1.0f);
            const auto in = sine (0.2, 0.5f);
            const auto mono = run (m, in, 1, 256);
            expectWithinAbsoluteError (mono[0].back(), in.back(), 1.0e-6f);

            MonitorMixer silentInput;
            silentInput.prepare (rate, 256);
            const auto none = run (silentInput, in, 2, 256, 0.1f, true);   // 入力なし（デバイスに入力が無い）
            expectEquals (maxAbs (none[0]), 0.1f);

            MonitorMixer unprepared;
            const auto untouched = run (unprepared, in, 2, 256, 0.2f);
            expectEquals (maxAbs (untouched[0]), 0.2f);
        }
    }
};

static MonitorMixerTests monitorMixerTests;
} // namespace vb::audio
