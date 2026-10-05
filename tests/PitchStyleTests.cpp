#include "ui/PitchStyle.h"

/*  自分の音程の線の形（#28）：合う・ずれ・大きくずれを、色だけでなく形でも分ける。段階の境目は色（判定）と同じ */

namespace vb::pitchstyle
{
class PitchStyleTests : public juce::UnitTest
{
public:
    PitchStyleTests() : juce::UnitTest ("Pitch line style", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("levels follow the tolerance and the warning limit (the same as the colours)");
        {
            expect (levelFor (0.0f, 30.0f) == Level::ok);
            expect (levelFor (30.0f, 30.0f) == Level::ok, "on the edge is still in");
            expect (levelFor (-30.0f, 30.0f) == Level::ok);
            expect (levelFor (31.0f, 30.0f) == Level::near);
            expect (levelFor (-50.0f, 30.0f) == Level::near, "up to 50 cents is a small miss");
            expect (levelFor (51.0f, 30.0f) == Level::far);
            expect (levelFor (60.0f, 50.0f) == Level::near, "a wide tolerance moves the warning limit too (+20)");
            expect (levelFor (71.0f, 50.0f) == Level::far);
        }

        beginTest ("each level has its own shape: solid, long dashes, short and thicker dashes");
        {
            expect (dashes (Level::ok).empty(), "in tune is a solid line");
            const auto n = dashes (Level::near), f = dashes (Level::far);
            expect (n.size() == 2 && f.size() == 2);
            if (n.size() == 2 && f.size() == 2)
            {
                expectGreaterThan (n[0], f[0], "a small miss has longer dashes than a big miss");
                expect (n != f);
            }
            expectGreaterThan (width (Level::far), width (Level::ok), "a big miss is thicker");
        }

        beginTest ("a short blip does not chop the line into bits: it takes the shape before it");
        {
            const auto O = Level::ok, N = Level::near, F = Level::far;
            auto check = [this] (std::vector<Level> in, std::vector<Level> want, const juce::String& what)
            {
                expect (shapeLevels (in, 3) == want, what);
            };
            check ({ O, O, O, N, O, O, O }, { O, O, O, O, O, O, O }, "a 1-point miss stays solid");
            check ({ N, N, N, N, O, N, N, N }, { N, N, N, N, N, N, N, N }, "a 1-point return inside a miss stays dashed");
            check ({ O, O, O, N, N, N, N }, { O, O, O, N, N, N, N }, "a lasting miss changes the shape");
            check ({ N, O, O, O, O }, { O, O, O, O, O }, "a short start takes the next shape");
            check ({ O, O, O, N, N, N, F, N, N, N }, { O, O, O, N, N, N, N, N, N, N }, "a far blip inside a small miss stays a small miss");
            check ({ N }, { N }, "a single point is kept");
            check ({}, {}, "empty");
        }

        beginTest ("the mark tells which way to go");
        {
            expectEquals (correction (10.0f, 30.0f), 0, "no mark when in tune");
            expectEquals (correction (-40.0f, 30.0f), 1, "flat: go up");
            expectEquals (correction (80.0f, 30.0f), -1, "sharp: go down");
        }
    }
};

static PitchStyleTests pitchStyleTests;
} // namespace vb::pitchstyle
