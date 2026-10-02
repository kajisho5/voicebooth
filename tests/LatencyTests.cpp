#include "audio/LatencyProbe.h"

/*  往復の遅れの実測（B6）。測定音を「機器」に通したつもりで遅らせ・小さくし・雑音を足して、
    遅れがサンプル単位で当たること、測れない時に理由を返すことを確かめる */

namespace vb
{
namespace latency = audio::latency;

namespace
{
    using latency::int64;

    /** 出力した測定音を delay サンプル遅らせ、gain を掛け、雑音を足して入力にする（ループバックのつもり） */
    std::vector<float> loopback (const latency::Plan& p, int64 delay, float gain, float noise, int seed = 1)
    {
        const auto signal = latency::makeSignal (p);
        std::vector<float> in ((size_t) p.totalLength, 0.0f);
        juce::Random r (seed);
        for (int64 i = 0; i < p.totalLength; ++i)
        {
            const auto src = i - delay;
            const auto v = src >= 0 ? signal[(size_t) src] * gain : 0.0f;
            in[(size_t) i] = v + (r.nextFloat() * 2.0f - 1.0f) * noise;
        }
        return in;
    }
}

class LatencyTests : public juce::UnitTest
{
public:
    LatencyTests() : juce::UnitTest ("Latency", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("fft: forward then inverse gives the input back (times n)");
        {
            std::vector<double> re (64), im (64, 0.0);
            for (size_t i = 0; i < re.size(); ++i)
                re[i] = std::sin ((double) i * 0.37) + (double) (i % 5);
            const auto orig = re;
            latency::fft (re, im, false);
            latency::fft (re, im, true);
            double worst = 0.0;
            for (size_t i = 0; i < re.size(); ++i)
                worst = juce::jmax (worst, std::abs (re[i] / 64.0 - orig[i]));
            expectLessThan (worst, 1.0e-9);
        }

        beginTest ("signal: 5 chirps at -12 dBFS, spaced longer than the longest measurable delay");
        {
            const auto p = latency::planFor (48000.0);
            expectEquals (p.bursts, 5);
            expectGreaterThan (p.period, p.maxLatency + p.chirpLength);
            const auto s = latency::makeSignal (p);
            expectEquals ((int64) s.size(), p.totalLength);
            float peak = 0.0f;
            for (auto v : s) peak = juce::jmax (peak, std::abs (v));
            expectWithinAbsoluteError (peak, 0.25f, 0.01f);
            expectEquals (s[(size_t) p.lead - 1], 0.0f);
            expectLessThan (p.totalLength, (int64) (4.0 * 48000.0));   // 4 秒未満で終わる
        }

        beginTest ("analyse: the delay comes back sample-exact at 44.1 / 48 / 96 / 192 kHz");
        {
            for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                const auto p = latency::planFor (rate);
                for (auto ms : { 0.0, 2.9, 10.7, 23.4, 120.0, 450.0 })
                {
                    const auto delay = (int64) std::llround (ms * 0.001 * rate);
                    const auto in = loopback (p, delay, 0.05f, 0.001f);   // 小さく（-38 dBFS）・雑音 -60 dBFS
                    const auto r = latency::analyse (in.data(), (int64) in.size(), p);
                    expect (r.ok(), juce::String (rate) + " Hz " + juce::String (ms) + " ms");
                    expectEquals (r.samples, delay);
                    expectEquals (r.agreeing, 5);
                    expectEquals (r.spreadMs, 0.0);
                    expect (! r.clipped);
                }
            }
        }

        beginTest ("analyse: inverted polarity and a louder room reflection still give the direct path");
        {
            const double rate = 48000.0;
            const auto p = latency::planFor (rate);
            const int64 delay = 1234;
            auto in = loopback (p, delay, -0.05f, 0.0005f);
            const auto refl = loopback (p, delay + 400, 0.09f, 0.0f);   // 8 ms 後の反響の方が大きい
            for (size_t i = 0; i < in.size(); ++i)
                in[i] += refl[i];
            const auto r = latency::analyse (in.data(), (int64) in.size(), p);
            expect (r.ok());
            expectEquals (r.samples, delay);
        }

        beginTest ("analyse: silence, noise only, jittering arrivals and clipping are reported");
        {
            const double rate = 48000.0;
            const auto p = latency::planFor (rate);

            std::vector<float> zero ((size_t) p.totalLength, 0.0f);
            expect (latency::analyse (zero.data(), (int64) zero.size(), p).status == latency::Result::Status::silent);

            const auto noise = loopback (p, 0, 0.0f, 0.05f);
            expect (latency::analyse (noise.data(), (int64) noise.size(), p).status == latency::Result::Status::weak);

            // 1 回ごとに 3 ms ずつずれる（Bluetooth の揺れのつもり）：そろわない
            const auto signal = latency::makeSignal (p);
            std::vector<float> jitter ((size_t) p.totalLength, 0.0f);
            for (int b = 0; b < p.bursts; ++b)
            {
                const auto d = 1000 + b * 144;
                for (int i = 0; i < p.chirpLength; ++i)
                    jitter[(size_t) (p.burstStart (b) + d + i)] += signal[(size_t) (p.burstStart (b) + i)] * 0.1f;
            }
            const auto rj = latency::analyse (jitter.data(), (int64) jitter.size(), p);
            expect (rj.status == latency::Result::Status::unstable);
            expect (! rj.ok());

            // 割れていても遅れは測れる（音量を下げる案内は出す）
            auto loud = loopback (p, 777, 6.0f, 0.0f);
            for (auto& v : loud) v = juce::jlimit (-1.0f, 1.0f, v);
            const auto rl = latency::analyse (loud.data(), (int64) loud.size(), p);
            expect (rl.ok());
            expect (rl.clipped);
            expectEquals (rl.samples, (int64) 777);
        }

        beginTest ("probe: block by block through a device model (buffer 256, 3 blocks + 37 samples) measures the round trip");
        {
            const double rate = 48000.0;
            const int block = 256;
            const int64 deviceDelay = 3 * block + 37;   // 出力に書いてから入力に戻るまで

            latency::Probe probe;
            probe.start (rate);
            expect (probe.isRunning());

            std::vector<float> line;                     // 出力したもの（全部）
            std::vector<float> out0 (block), out1 (block), in (block);
            float* outs[] = { out0.data(), out1.data() };
            int64 pos = 0;
            for (int guard = 0; probe.isRunning() && guard < 10000; ++guard)
            {
                for (int i = 0; i < block; ++i)
                {
                    const auto src = pos + i - deviceDelay;
                    in[(size_t) i] = src >= 0 && src < (int64) line.size() ? line[(size_t) src] * 0.2f : 0.0f;
                }
                // 出力の先を見ないよう、このブロックの出力はまだ line に入っていない（遅れは 1 ブロック以上）
                expect (probe.process (in.data(), outs, 2, block));
                line.insert (line.end(), out0.begin(), out0.end());
                for (int i = 0; i < block; ++i)
                    expectEquals (out1[(size_t) i], out0[(size_t) i]);   // 全チャンネル同じ
                pos += block;
            }
            expect (probe.isFinished());
            expect (! probe.process (in.data(), outs, 2, block));     // 終わった後は何もしない

            const auto r = latency::analyse (probe.captured().data(), (int64) probe.captured().size(), probe.plan());
            expect (r.ok());
            expectEquals (r.samples, deviceDelay);
        }
    }
};

static LatencyTests latencyTests;
} // namespace vb
