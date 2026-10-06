#include "audio/GameSounds.h"

/*  待ち時間のゲームの音（GameSounds）：クリックが拍の頭で鳴る・4 拍ごとに高い・目標の音の高さ・止まる */

namespace vb::audio
{
class GameSoundsTests : public juce::UnitTest
{
public:
    GameSoundsTests() : juce::UnitTest ("GameSounds", "VoiceBooth") {}

    static std::vector<float> render (GameSounds& g, int total, int block = 480)
    {
        std::vector<float> out ((size_t) total, 0.0f);
        for (int at = 0; at < total; at += block)
        {
            float* ch[] = { out.data() + at };
            g.process (ch, 1, juce::jmin (block, total - at));
        }
        return out;
    }

    /** 無音からの立ち上がり（クリックの頭）の位置 */
    static std::vector<int> onsets (const std::vector<float>& x, float thr = 0.02f)
    {
        std::vector<int> at;
        int quiet = 1000;
        for (int i = 0; i < (int) x.size(); ++i)
        {
            if (std::abs (x[(size_t) i]) > thr && quiet >= 200) at.push_back (i);
            quiet = std::abs (x[(size_t) i]) > thr ? 0 : quiet + 1;
        }
        return at;
    }

    void runTest() override
    {
        beginTest ("clicks start on each beat (120 BPM at 48 kHz = every 24000 samples), counted from setBeat");
        {
            GameSounds g;
            g.prepare (48000.0);
            g.setBeat (120.0);
            const auto x = render (g, 48000 * 2 + 100);
            const auto o = onsets (x);
            expectEquals ((int) o.size(), 5);
            for (size_t i = 0; i < o.size(); ++i)
                expectWithinAbsoluteError (o[i], (int) i * 24000, 2);
            expectEquals (g.samplesRendered(), (juce::int64) x.size());
        }

        beginTest ("an odd tempo (97 BPM at 44.1 kHz) keeps every beat, rounded to the nearest sample");
        {
            GameSounds g;
            g.prepare (44100.0);
            g.setBeat (97.0);
            const auto spb = 44100.0 * 60.0 / 97.0;
            const auto o = onsets (render (g, (int) (spb * 8) + 10, 333));
            expectEquals ((int) o.size(), 9);
            for (size_t i = 0; i < o.size(); ++i)
                expectWithinAbsoluteError (o[i], (int) std::llround ((double) i * spb), 2);
        }

        beginTest ("every 4th beat is the high click (more zero crossings)");
        {
            GameSounds g;
            g.prepare (48000.0);
            g.setBeat (120.0);
            const auto x = render (g, 48000 * 2 + 2000);
            auto crossings = [&] (int from)
            {
                int n = 0;
                for (int i = from + 1; i < from + 1000; ++i)
                    if ((x[(size_t) i - 1] < 0.0f) != (x[(size_t) i] < 0.0f)) ++n;
                return n;
            };
            expectGreaterThan (crossings (0), crossings (24000) + 10);       // 1 拍目（高い） > 2 拍目
            expectGreaterThan (crossings (96000), crossings (72000) + 10);   // 5 拍目（高い） > 4 拍目
        }

        beginTest ("setBeat (0) stops the clicks and the clock; a new tempo starts again from beat 0");
        {
            GameSounds g;
            g.prepare (48000.0);
            g.setBeat (120.0);
            render (g, 30000);
            g.setBeat (0.0);
            const auto x = render (g, 48000);
            float peak = 0.0f;
            for (auto v : x) peak = juce::jmax (peak, std::abs (v));
            expectEquals (peak, 0.0f);
            expectEquals (g.samplesRendered(), (juce::int64) -1);
            g.setBeat (60.0);
            const auto o = onsets (render (g, 48000 + 10));
            expect (! o.empty() && o.front() <= 2);   // 0 拍目（正弦波の頭は 0 なので数サンプル後に立ち上がる）
            expectEquals ((int) o.size(), 2);
        }

        beginTest ("the target tone has the right pitch (A4 = 440 Hz), fades in and stops after its length");
        {
            GameSounds g;
            g.prepare (48000.0);
            g.playTone (69.0f, 0.5);
            const auto x = render (g, 48000);
            int n = 0;
            for (int i = 4800; i < 4800 + 9600; ++i)   // 0.1〜0.3 秒（なめらかに入った後）
                if ((x[(size_t) i - 1] < 0.0f) != (x[(size_t) i] < 0.0f)) ++n;
            expectWithinAbsoluteError ((double) n / 2.0 / 0.2, 440.0, 6.0);
            expectLessThan (std::abs (x[0]), 0.001f);               // 頭は 0 から
            float tail = 0.0f;
            for (int i = 24000; i < 48000; ++i) tail = juce::jmax (tail, std::abs (x[(size_t) i]));
            expectEquals (tail, 0.0f);                               // 0.5 秒で止まる
            expectEquals (g.samplesRendered(), (juce::int64) -1);   // 音だけならクリックの時計は動かない
        }

        beginTest ("stopping or replacing the tone midway fades it out instead of cutting it");
        {
            // 隣り合うサンプルの差：440 Hz・0.18 なら最大 0.18 × 2π × 440 / 48000 ≈ 0.0104。切ると 0.18 近く跳ぶ
            auto maxStep = [] (const std::vector<float>& x)
            {
                float m = 0.0f;
                for (size_t i = 1; i < x.size(); ++i) m = juce::jmax (m, std::abs (x[i] - x[i - 1]));
                return m;
            };
            GameSounds g;
            g.prepare (48000.0);
            g.playTone (69.0f, 2.0);
            auto x = render (g, 9601);    // 0.2 秒（鳴っている途中、周期の途中で止める）
            g.playTone (0.0f, 0.0);
            const auto y = render (g, 4800);
            x.insert (x.end(), y.begin(), y.end());
            expectLessThan (maxStep (x), 0.02f);
            float tail = 0.0f;
            for (size_t i = 9601 + 1200; i < x.size(); ++i) tail = juce::jmax (tail, std::abs (x[i]));
            expectEquals (tail, 0.0f);   // 20 ms で下げきる

            g.playTone (69.0f, 2.0);
            auto a = render (g, 9601);
            g.playTone (81.0f, 0.5);       // 鳴っている途中に別の音
            const auto b = render (g, 9600);
            a.insert (a.end(), b.begin(), b.end());
            expectLessThan (maxStep (a), 0.04f);   // 880 Hz の正弦波の差（約 0.021）程度まで
            int n = 0;
            for (size_t i = 9601 + 2400; i < 9601 + 7200; ++i)
                if ((a[i - 1] < 0.0f) != (a[i] < 0.0f)) ++n;
            expectWithinAbsoluteError ((double) n / 2.0 / 0.1, 880.0, 15.0);   // 次の音は鳴る
        }
    }
};

static GameSoundsTests gameSoundsTests;
} // namespace vb::audio
