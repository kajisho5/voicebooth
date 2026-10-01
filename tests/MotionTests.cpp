#include <juce_core/juce_core.h>
#include "ui/Motion.h"

/*  動き（DESIGN 4.10）の計算：ばね・吸い付き・感度・余韻・再生ヘッドの先回り。
    値の真実は数値（吸い付きで決まる値は既定値ちょうど、それ以外は丸めない）を確かめる */

namespace vb::motion
{
class MotionTests : public juce::UnitTest
{
public:
    MotionTests() : juce::UnitTest ("Motion", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("spring: settles on the target; segment spring overshoots a little, then lands");
        {
            Spring s (0.0f);
            float maxX = 0.0f;
            for (int i = 0; i < 60; ++i)   // 1 秒
            {
                s.step (1.0f, 1.0f / 60.0f, segment::springK, segment::springC);
                maxX = std::max (maxX, s.x);
            }
            expectGreaterThan (maxX, 1.01f);    // 少し行き過ぎる
            expectLessThan (maxX, 1.25f);       // 行き過ぎすぎない
            expect (segment::landed (s, 1.0f));
        }

        beginTest ("spring: frame rate does not change the motion much (4 ms sub-steps)");
        {
            Spring a (0.0f), b (0.0f);
            for (int i = 0; i < 12; ++i) a.step (1.0f, 1.0f / 60.0f, fader::springK, fader::springC);
            for (int i = 0; i < 6; ++i)  b.step (1.0f, 1.0f / 30.0f, fader::springK, fader::springC);
            expectWithinAbsoluteError (a.x, b.x, 0.02f);
        }

        beginTest ("spring: reduced motion jumps to the end state");
        {
            Spring s (0.2f);
            s.step (0.75f, 1.0f / 60.0f, fader::springK, fader::springC, true);
            expectEquals (s.x, 0.75f);
            expect (s.atRest (0.75f));
        }

        beginTest ("detent: dead zone holds the centre exactly, no jump when leaving, every value reachable");
        {
            const Detent d { 0.75, 0.03 };
            expectEquals (d.toValue (0.75), 0.75);
            expectEquals (d.toValue (0.77), 0.75);
            expectEquals (d.toValue (0.73), 0.75);
            expectWithinAbsoluteError (d.toValue (0.7801), 0.7501, 1e-12);   // 出たら続きから（飛ばない）
            expectWithinAbsoluteError (d.toValue (0.7199), 0.7499, 1e-12);
            for (auto v : { 0.0, 0.1234567, 0.7499, 0.75, 0.7501, 0.9, 1.0 })
                expectWithinAbsoluteError (d.toValue (d.toRaw (v)), v, 1e-12);   // どの値にも戻れる
        }

        beginTest ("fader: relative drag stops at 0 dB exactly, other values are not rounded");
        {
            const double travel = 150.0;
            const auto det = fader::detent (travel);
            double raw = det.toRaw (0.6123);   // 掴んだ所から（飛ばない）
            expectWithinAbsoluteError (fader::valueFromRaw (raw, det), 0.6123, 1e-12);

            // 上へ 1 px ずつ。0 dB の手前までは連続、0 dB で数 px 止まる
            int heldPixels = 0;
            double v = 0.0;
            for (int px = 0; px < 60; ++px)
            {
                raw += fader::dragDelta (-1.0, travel, false);
                v = fader::valueFromRaw (raw, det);
                if (juce::exactlyEqual (v, fader::unity)) ++heldPixels;
            }
            expectGreaterOrEqual (heldPixels, 8);    // 吸い付きの幅（detentPixels の前後）
            expectLessOrEqual (heldPixels, 12);
            expectGreaterThan (v, fader::unity);     // 通り過ぎられる

            double raw2 = det.toRaw (0.5);
            raw2 += fader::dragDelta (-3.3, travel, false);
            const auto v2 = fader::valueFromRaw (raw2, det);
            expectWithinAbsoluteError (v2, 0.5 + 3.3 / travel, 1e-12);   // 0.01 刻みに丸めていない
        }

        beginTest ("fader: Shift is fine (1/4), value stays within 0..1");
        {
            const double travel = 150.0;
            expectWithinAbsoluteError (fader::dragDelta (-10.0, travel, true), fader::dragDelta (-10.0, travel, false) * 0.25, 1e-12);
            const auto det = fader::detent (travel);
            double raw = det.toRaw (0.95);
            raw += fader::dragDelta (-500.0, travel, false);
            expectEquals (fader::valueFromRaw (raw, det), 1.0);
            raw += fader::dragDelta (20.0, travel, false);   // 上で止めてから戻すとすぐ下がる（生の位置も抑えてある）
            expectLessThan (fader::valueFromRaw (raw, det), 1.0);
        }

        beginTest ("encoder: fast turns move more than slow ones, Shift is finer");
        {
            const double range = 100.0;   // テンポ 50..150
            const auto slow = encoder::dragDelta (10.0, 200.0, range, false);   // 10 px を 0.2 秒
            const auto fast = encoder::dragDelta (10.0, 10.0, range, false);    // 10 px を 10 ms
            const auto fine = encoder::dragDelta (10.0, 200.0, range, true);
            expectGreaterThan (fast, slow * 2.0);
            expectLessThan (fine, slow);
            expectGreaterThan (slow, 0.0);
            expectLessThan (encoder::dragDelta (-10.0, 200.0, range, false), 0.0);
            expectLessOrEqual (fast, 10.0 * 0.003 * range * 4.0 + 1e-9);        // 速くても 4 倍まで
        }

        beginTest ("encoder: detent at the default (tempo 100%, key 0)");
        {
            const auto d = encoder::detent (100.0, 100.0);
            expectEquals (d.toValue (101.5), 100.0);
            expectEquals (d.toValue (98.5), 100.0);
            expectGreaterThan (d.toValue (103.0), 100.0);
            const auto k = encoder::detent (0.0, 12.0);
            expectEquals (k.toValue (0.2), 0.0);
            expectGreaterThan (k.toValue (0.4), 0.0);
        }

        beginTest ("key LED: lights at once, fades out with an afterglow");
        {
            float l = 0.0f;
            for (int i = 0; i < 6; ++i) l = key::ledLevel (l, true, 1.0f / 60.0f, false);   // 0.1 秒
            expectEquals (l, 1.0f);

            float t = 0.0f;
            for (int i = 0; i < 9; ++i) { l = key::ledLevel (l, false, 1.0f / 60.0f, false); t += 1.0f / 60.0f; }   // 0.15 秒
            expectGreaterThan (l, 0.1f);   // まだ余韻
            for (int i = 0; i < 30; ++i) l = key::ledLevel (l, false, 1.0f / 60.0f, false);   // さらに 0.5 秒
            expectEquals (l, 0.0f);

            expectEquals (key::ledLevel (0.3f, false, 0.001f, true), 0.0f);   // 動きを減らす：即
            expectEquals (key::ledLevel (0.3f, true, 0.001f, true), 1.0f);
        }

        beginTest ("REC breathing: slow (2.2 s), never fully dark");
        {
            expectWithinAbsoluteError (key::breathe (0.0), 1.0f, 1e-5f);
            expectWithinAbsoluteError (key::breathe (key::breathePeriod * 0.5), 0.55f, 1e-5f);
            expectWithinAbsoluteError (key::breathe (key::breathePeriod), 1.0f, 1e-4f);
        }

        beginTest ("update notice LED: blinks twice, then stays on");
        {
            int offs = 0;
            bool prevOn = true;
            for (double t = 0.0; t < 2.0; t += 0.01)
            {
                const bool on = notice::led (t) > 0.5f;
                if (prevOn && ! on) ++offs;
                prevOn = on;
            }
            expectEquals (offs, 2);
            expectEquals (notice::led (5.0), 1.0f);
        }

        beginTest ("shake: small, short, ends where it started");
        {
            expectEquals (shake::offset (0.0), 0.0f);
            expectEquals (shake::offset (shake::seconds), 0.0f);
            float maxAbs = 0.0f;
            for (double t = 0.0; t < shake::seconds; t += 0.005)
                maxAbs = std::max (maxAbs, std::abs (shake::offset (t)));
            expectGreaterThan (maxAbs, 1.5f);
            expectLessOrEqual (maxAbs, 3.0f);
        }

        beginTest ("playhead: no scroll before 70%, then the view runs ahead smoothly (no page jump)");
        {
            const std::int64_t len = 8 * 48000, song = 150 * 48000;
            std::int64_t start = 0;

            // 6 割の位置：動かない
            expectEquals (playhead::follow (start, len, len * 6 / 10, song, 1.0 / 30.0, false), (std::int64_t) 0);

            // 1 倍速で 10 秒再生：画面は少しずつ進み、ヘッドは 7 割の少し右に留まる（ページ送りしない）
            std::int64_t head = len * 7 / 10;
            std::int64_t maxStep = 0;
            for (int i = 0; i < 300; ++i)
            {
                head += 48000 / 30;
                const auto next = playhead::follow (start, len, head, song, 1.0 / 30.0, false);
                expectGreaterOrEqual (next, start);             // 戻らない
                maxStep = std::max (maxStep, next - start);
                start = next;
            }
            const auto at = (double) (head - start) / (double) len;
            expectGreaterThan (at, 0.70);
            expectLessThan (at, 0.76);
            expectLessThan ((double) maxStep, (double) len * 0.05);   // 1 フレームで画面の 5% 以上は動かない

            // 動きを減らす：7 割にぴったり
            expectEquals (playhead::follow (0, len, len * 8 / 10, song, 1.0 / 30.0, true), len * 8 / 10 - len * 7 / 10);
        }

        beginTest ("playhead: jumps when out of view (loop back / seek), clamps at the song end");
        {
            const std::int64_t len = 8 * 48000, song = 20 * 48000;
            // ループで頭へ戻った：その場で合わせる（頭の手前は 0）
            expectEquals (playhead::follow (10 * 48000, len, 48000, song, 1.0 / 30.0, false), (std::int64_t) 0);
            // 曲の終わり近く：画面は曲の終わりで止まる
            std::int64_t start = song - len - 1000;
            for (int i = 0; i < 200; ++i)
                start = playhead::follow (start, len, song - 10, song, 1.0 / 30.0, false);
            expectEquals (start, song - len);
        }
    }
};

static MotionTests motionTests;
} // namespace vb::motion
