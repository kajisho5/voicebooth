#include "audio/Retro.h"
#include "audio/TakeRecorder.h"

/*  遡及録音（B7）：REC を押し遅れた時に、フレーズの頭まで遡るか */

namespace vb
{
namespace retro = audio::retro;

namespace
{
    /** 10 ms ごとのピーク列を作る。voice の区間（秒）だけ声（-12 dBFS）、ほかは雑音（-70 dBFS） */
    std::vector<float> peaks (double seconds, std::initializer_list<std::pair<double, double>> voice, float noise = 0.0003f)
    {
        std::vector<float> p ((size_t) juce::roundToInt (seconds * 100.0), noise);
        for (auto [a, b] : voice)
            for (int i = juce::roundToInt (a * 100.0); i < juce::roundToInt (b * 100.0) && i < (int) p.size(); ++i)
                p[(size_t) i] = 0.25f;
        return p;
    }
}

class RetroTests : public juce::UnitTest
{
public:
    RetroTests() : juce::UnitTest ("Retro", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("pressed late inside a phrase: goes back to the phrase head minus 50 ms");
        {
            // 前のフレーズ 2.0–4.0 秒、休み、今のフレーズ 5.2 秒から。6.0 秒で押した
            const auto p = peaks (8.0, { { 2.0, 4.0 }, { 5.2, 8.0 } });
            expectEquals (retro::phraseStartFrame (p.data(), (int) p.size(), 600), 515);
        }

        beginTest ("pressed before singing: starts where pressed");
        {
            const auto p = peaks (8.0, { { 2.0, 4.0 }, { 6.5, 8.0 } });
            expectEquals (retro::phraseStartFrame (p.data(), (int) p.size(), 600), 600);
        }

        beginTest ("a short breath (under 250 ms) does not split the phrase");
        {
            const auto p = peaks (8.0, { { 3.0, 4.5 }, { 4.65, 8.0 } });   // 150 ms の息継ぎ
            expectEquals (retro::phraseStartFrame (p.data(), (int) p.size(), 600), 295);
        }

        beginTest ("singing without a gap for more than 6 s: starts where pressed (don't wipe a long stretch)");
        {
            const auto p = peaks (12.0, { { 1.0, 12.0 } });
            expectEquals (retro::phraseStartFrame (p.data(), (int) p.size(), 1000), 1000);
        }

        beginTest ("the recording began inside the phrase: starts from the first sample");
        {
            const auto p = peaks (3.0, { { 0.0, 3.0 } });
            expectEquals (retro::phraseStartFrame (p.data(), (int) p.size(), 250), 0);
        }

        beginTest ("a noisy room raises the threshold above the noise floor");
        {
            const auto p = peaks (8.0, { { 5.0, 8.0 } }, 0.02f);           // 雑音 -34 dBFS
            expectEquals (retro::phraseStartFrame (p.data(), (int) p.size(), 600), 495);
            const auto loudNoise = peaks (8.0, {}, 0.02f);
            expectEquals (retro::phraseStartFrame (loudNoise.data(), (int) loudNoise.size(), 600), 600);
        }

        beginTest ("press beyond what has arrived uses the last arrived frame; empty input");
        {
            const auto p = peaks (6.0, { { 4.0, 6.0 } });
            expectEquals (retro::phraseStartFrame (p.data(), (int) p.size(), 650), 395);
            expectEquals (retro::phraseStartFrame (nullptr, 0, 40), 40);
            expectEquals (retro::phraseStartFrame (p.data(), (int) p.size(), 0), 0);
        }

        beginTest ("recorder envelope: one peak per 10 ms of written input, silence for a vanished input");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("VoiceBoothTests-env-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
            audio::TakeRecorder r;
            expect (r.begin (dir.getChildFile ("t.wav"), 48000.0).isEmpty());
            std::vector<float> loud (480, 0.5f), quiet (480, 0.01f);
            r.process (quiet.data(), 480, 0, 480, false);
            r.process (loud.data(), 480, 480, 480, false);
            r.process (nullptr, 480, 960, 480, false);
            r.process (loud.data(), 240, 1440, 240, false);           // 半分だけ（まだ 1 つにならない）
            std::vector<float> env;
            expectEquals (r.copyEnvelope (env), 480);
            expectEquals ((int) env.size(), 3);
            expectWithinAbsoluteError (env[0], 0.01f, 1.0e-6f);
            expectWithinAbsoluteError (env[1], 0.5f, 1.0e-6f);
            expectEquals (env[2], 0.0f);
            expectEquals (r.getStartSample(), (juce::int64) 0);
            r.finish();
            dir.deleteRecursively();
        }
    }
};

static RetroTests retroTests;
} // namespace vb
