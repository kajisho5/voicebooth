#include "song/Tempo.h"

namespace vb::song
{
class TempoTests : public juce::UnitTest
{
public:
    TempoTests() : juce::UnitTest ("Tempo", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("tap tempo: needs 4 taps, averages intervals");
        {
            TapTempo t;
            expectEquals (t.tap (0.0), 0.0);
            expectEquals (t.tap (0.5), 0.0);
            expectEquals (t.tap (1.0), 0.0);
            expectEquals (t.tap (1.5), 120.0);   // 0.5 秒間隔 = 120 BPM
            expectEquals (t.tap (2.0), 120.0);
            expectLessThan (t.jitterMs(), 0.001);
        }

        beginTest ("tap tempo: uneven taps average out, jitter reported");
        {
            TapTempo t;
            double now = 10.0;
            for (auto dt : { 0.0, 0.48, 0.52, 0.49, 0.51 })
                t.tap (now += dt);
            expectWithinAbsoluteError (t.bpm(), 120.0, 0.01);
            expectGreaterThan (t.jitterMs(), 10.0);
        }

        beginTest ("tap tempo: a 2 s gap starts over, only recent taps count");
        {
            TapTempo t;
            for (int i = 0; i < 6; ++i) t.tap (i * 1.0);      // 60 BPM
            expectEquals (t.bpm(), 60.0);
            t.tap (5.0 + 2.5);                                 // 2 秒以上あいた
            expectEquals (t.count(), 1);
            expectEquals (t.bpm(), 0.0);

            TapTempo u;                                        // 迷った叩き始めは捨てる
            double now = 0.0;
            u.tap (now);
            u.tap (now += 0.9);
            for (int i = 0; i < 8; ++i) u.tap (now += 0.4);   // 150 BPM
            expectEquals (u.bpm(), 150.0);
        }

        beginTest ("x2 / /2 stay in range and round to 2 decimals");
        {
            expectEquals (doubledBpm (61.5), 123.0);
            expectEquals (halvedBpm (246.0), 123.0);
            expectEquals (doubledBpm (180.0), 180.0);   // 360 は範囲外
            expectEquals (halvedBpm (50.0), 50.0);      // 25 は範囲外
            expectEquals (roundBpm (127.996), 128.0);
            expectEquals (halvedBpm (127.98), 63.99);
        }
    }
};

static TempoTests tempoTests;
} // namespace vb::song
