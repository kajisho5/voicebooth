#include "audio/InputMeter.h"

namespace vb::audio
{
namespace
{
    constexpr double twoPi = 6.283185307179586;

    /** 合成した信号を block サンプルずつメーターに流す。t は通しのサンプル位置（続きから流せる） */
    template <typename Fn>
    void feed (InputMeter& m, double rate, int block, double seconds, juce::int64& t, Fn&& signal)
    {
        std::vector<float> buf ((size_t) block);
        const auto total = (juce::int64) std::llround (seconds * rate);
        for (juce::int64 done = 0; done < total;)
        {
            const auto n = (int) std::min<juce::int64> (block, total - done);
            for (int i = 0; i < n; ++i)
                buf[(size_t) i] = signal ((double) (t + i) / rate);
            m.process (buf.data(), n);
            done += n;
            t += n;
        }
    }

    float gainOf (float db) { return juce::Decibels::decibelsToGain (db); }

    // 997 Hz（ブロックの長さと周期がそろわない周波数）
    auto sine (float db) { return [a = gainOf (db)] (double sec) { return a * (float) std::sin (twoPi * 997.0 * sec); }; }
    auto dc (float value) { return [value] (double) { return value; }; }
    auto silence() { return [] (double) { return 0.0f; }; }

    struct Setting { double rate; int block; };
    const Setting settings[] = { { 44100.0, 64 }, { 48000.0, 512 }, { 96000.0, 37 }, { 22050.0, 1000 } };
}

class InputMeterTests : public juce::UnitTest
{
public:
    InputMeterTests() : juce::UnitTest ("InputMeter", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("sine at -12 dBFS reads -12 peak and -15 RMS at any SR / block size");
        for (auto st : settings)
        {
            InputMeter m;
            m.prepare (st.rate);
            juce::int64 t = 0;
            feed (m, st.rate, st.block, 1.0, t, sine (-12.0f));
            const auto l = m.read();
            const auto where = juce::String (st.rate) + " Hz / " + juce::String (st.block);
            expectWithinAbsoluteError (l.peakDb, -12.0f, 0.05f, where);
            expectWithinAbsoluteError (l.holdDb, -12.0f, 0.05f, where);
            expectWithinAbsoluteError (l.rmsDb, -12.0f - 3.0103f, 0.05f, where);   // 正弦波の RMS = ピーク -3.01 dB
            expect (! l.clipped, where);
            expect (! m.isDigitalSilence());
        }

        beginTest ("RMS is a 300 ms window");
        for (auto st : settings)
        {
            InputMeter m;
            m.prepare (st.rate);
            juce::int64 t = 0;
            const auto full = -12.0f - 3.0103f;
            const auto where = juce::String (st.rate);

            feed (m, st.rate, st.block, 0.155, t, sine (-12.0f));
            expectWithinAbsoluteError (m.read().rmsDb, full - 3.0103f, 0.25f, where);   // 窓の半分 → 1/2 のエネルギー
            feed (m, st.rate, st.block, 0.16, t, sine (-12.0f));
            expectWithinAbsoluteError (m.read().rmsDb, full, 0.05f, where);              // 300 ms で満ちる

            feed (m, st.rate, st.block, 0.155, t, silence());
            expectWithinAbsoluteError (m.read().rmsDb, full - 3.0103f, 0.25f, where);
            feed (m, st.rate, st.block, 0.16, t, silence());
            expectEquals (m.read().rmsDb, InputMeter::floorDb, where);                   // 300 ms で抜ける
        }

        beginTest ("hold stays 1.5 s, then peak and hold fall 20 dB/s");
        for (auto st : settings)
        {
            InputMeter m;
            m.prepare (st.rate);
            juce::int64 t = 0;
            const auto top = juce::Decibels::gainToDecibels (0.5f);   // -6.02 dBFS
            const auto where = juce::String (st.rate) + " Hz / " + juce::String (st.block);
            const auto tol = 0.25f + 20.0f * (float) st.block / (float) st.rate;    // ブロック 1 つぶんのずれまで

            feed (m, st.rate, st.block, 0.1, t, dc (0.5f));
            expectWithinAbsoluteError (m.read().peakDb, top, 0.01f, where);

            feed (m, st.rate, st.block, 1.0, t, silence());
            expectWithinAbsoluteError (m.read().holdDb, top, 0.01f, where);              // まだ保っている
            expectWithinAbsoluteError (m.read().peakDb, top - 20.0f, tol, where);        // ピークは 1 秒で -20 dB

            feed (m, st.rate, st.block, 0.4, t, silence());
            expectWithinAbsoluteError (m.read().holdDb, top, 0.01f, where);              // 1.4 秒

            feed (m, st.rate, st.block, 0.6, t, silence());                              // 2.0 秒：0.5 秒ぶん下がる
            expectWithinAbsoluteError (m.read().holdDb, top - 10.0f, tol, where);
            expectWithinAbsoluteError (m.read().peakDb, top - 40.0f, tol, where);
        }

        beginTest ("a louder peak restarts the hold; a quieter one does not");
        {
            InputMeter m;
            m.prepare (48000.0);
            juce::int64 t = 0;
            feed (m, 48000.0, 256, 0.05, t, dc (0.25f));
            feed (m, 48000.0, 256, 1.0, t, silence());
            feed (m, 48000.0, 256, 0.05, t, dc (0.1f));       // 低い山：ホールドは -12 のまま
            expectWithinAbsoluteError (m.read().holdDb, juce::Decibels::gainToDecibels (0.25f), 0.01f);
            feed (m, 48000.0, 256, 0.05, t, dc (0.5f));       // 高い山：-6 に上がって保ち直す
            feed (m, 48000.0, 256, 1.4, t, silence());
            expectWithinAbsoluteError (m.read().holdDb, juce::Decibels::gainToDecibels (0.5f), 0.01f);
        }

        beginTest ("clip latches at -0.1 dBFS until reset");
        {
            InputMeter m;
            m.prepare (48000.0);
            juce::int64 t = 0;
            feed (m, 48000.0, 128, 0.2, t, dc (gainOf (-0.2f)));
            expect (! m.read().clipped);                        // -0.2 dBFS は点かない

            // 1 サンプルだけの -0.1 dBFS
            std::vector<float> spike (480, 0.0f);
            spike[123] = -gainOf (InputMeter::clipDb);          // 負の側でも
            m.process (spike.data(), (int) spike.size());
            expect (m.read().clipped);

            feed (m, 48000.0, 128, 5.0, t, silence());
            expect (m.read().clipped);                          // 消すまで残る
            m.resetClip();
            expect (! m.read().clipped);
            feed (m, 48000.0, 128, 0.5, t, sine (-1.0f));
            expect (! m.read().clipped);
            feed (m, 48000.0, 128, 0.1, t, dc (1.0f));
            expect (m.read().clipped);
        }

        beginTest ("long silence: floor and digital silence after 2 s");
        {
            InputMeter m;
            m.prepare (44100.0);
            juce::int64 t = 0;
            feed (m, 44100.0, 512, 0.5, t, sine (-20.0f));
            feed (m, 44100.0, 512, 1.9, t, silence());
            expect (! m.isDigitalSilence());
            feed (m, 44100.0, 512, 0.2, t, silence());
            expect (m.isDigitalSilence());
            feed (m, 44100.0, 512, 8.0, t, silence());
            const auto l = m.read();
            expectEquals (l.peakDb, InputMeter::floorDb);
            expectEquals (l.holdDb, InputMeter::floorDb);
            expectEquals (l.rmsDb, InputMeter::floorDb);

            // ごく小さい雑音（-120 dBFS）でも 0 ではないので「完全な無音」ではない
            feed (m, 44100.0, 512, 0.1, t, dc (1.0e-6f));
            expect (! m.isDigitalSilence());
            expectEquals (m.read().peakDb, InputMeter::floorDb);   // 表示は下限
        }

        beginTest ("reset clears levels; null input and broken samples are silence");
        {
            InputMeter m;
            m.prepare (48000.0);
            juce::int64 t = 0;
            feed (m, 48000.0, 256, 0.5, t, dc (1.0f));
            m.reset();
            auto l = m.read();
            expectEquals (l.peakDb, InputMeter::floorDb);       // UI にはすぐ
            expect (! l.clipped);
            m.process (nullptr, 256);
            l = m.read();
            expectEquals (l.peakDb, InputMeter::floorDb);
            expectEquals (l.holdDb, InputMeter::floorDb);
            expectEquals (l.rmsDb, InputMeter::floorDb);

            std::vector<float> broken (256, std::numeric_limits<float>::quiet_NaN());
            broken[10] = std::numeric_limits<float>::infinity();
            m.process (broken.data(), (int) broken.size());
            feed (m, 48000.0, 256, 0.5, t, sine (-12.0f));
            expectWithinAbsoluteError (m.read().rmsDb, -15.05f, 0.1f);   // 壊れた値で止まらない
        }

        beginTest ("level verdict (DESIGN 5)");
        {
            using V = InputMeter::Verdict;
            expect (InputMeter::judge (-1.0f) == V::hot);
            expect (InputMeter::judge (-2.9f) == V::hot);
            expect (InputMeter::judge (-3.0f) == V::ok);
            expect (InputMeter::judge (-9.0f) == V::ok);
            expect (InputMeter::judge (-20.0f) == V::ok);
            expect (InputMeter::judge (-20.1f) == V::low);
            expect (InputMeter::judge (InputMeter::floorDb) == V::low);
        }

        beginTest ("clip threshold constant matches -0.1 dBFS");
        {
            expectWithinAbsoluteError (0.98855309f, juce::Decibels::decibelsToGain (InputMeter::clipDb), 1.0e-6f);
        }
    }
};

static InputMeterTests inputMeterTests;
} // namespace vb::audio
