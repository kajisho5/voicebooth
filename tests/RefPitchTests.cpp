#include "analysis/RefPitch.h"

/*  お手本の音程（B9）：原曲 − カラオケ で声を取り出して音程を出す。自作の合成音で確かめる
    伴奏 = 打音 + 和音、原曲 = 前奏 + 伴奏（少し小さく：マスタリングの差）+ 声（倍音つきの旋律）、カラオケ = 別の長さの無音 + 伴奏 */

namespace vb::analysis
{
namespace
{
    constexpr double sr = 44100.0;
    constexpr double twoPi = 6.283185307179586;

    double midiHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

    struct Note { double start, end, midi; };   // 伴奏の時間（秒）

    std::vector<float> band (double seconds, int seed)
    {
        juce::Random rnd (seed);
        const auto n = (size_t) (seconds * sr);
        std::vector<float> x (n, 0.0f);
        for (double t = 0.1; t < seconds - 0.3;)
        {
            const auto start = (size_t) (t * sr);
            const auto amp = 0.3f + 0.3f * rnd.nextFloat();
            for (size_t i = 0; i < (size_t) (0.12 * sr) && start + i < n; ++i)
                x[start + i] += amp * (rnd.nextFloat() * 2.0f - 1.0f) * std::exp (-(float) i / (float) (0.025 * sr));
            t += 0.18 + 0.47 * rnd.nextDouble();
        }
        const int scale[] = { 0, 2, 4, 5, 7, 9 };
        for (double t = 0.0; t < seconds;)
        {
            const auto len = 1.5 + 1.5 * rnd.nextDouble();
            const auto root = 48 + scale[rnd.nextInt (6)];
            const auto s0 = (size_t) (t * sr), s1 = std::min (n, (size_t) ((t + len) * sr));
            for (const auto iv : { 0.0, 4.0, 7.0 })
            {
                const auto f = midiHz (root + iv);
                for (size_t i = s0; i < s1; ++i)
                {
                    const auto ph = twoPi * f * (double) (i - s0) / sr;
                    x[i] += (float) (0.05 * (std::sin (ph) + 0.5 * std::sin (2 * ph) + 0.25 * std::sin (3 * ph)));
                }
            }
            t += len;
        }
        return x;
    }

    /** 旋律（伴奏の時間）。3 秒目から 0.3〜0.9 秒の音符、間に 0.15〜0.4 秒の休み */
    std::vector<Note> melody (double seconds, int seed)
    {
        juce::Random rnd (seed);
        std::vector<Note> notes;
        for (double t = 3.0; t < seconds - 1.0;)
        {
            const auto len = 0.3 + 0.6 * rnd.nextDouble();
            notes.push_back ({ t, t + len, 57.0 + rnd.nextInt (15) });
            t += len + 0.15 + 0.25 * rnd.nextDouble();
        }
        return notes;
    }

    /** 原曲：lead 秒の前奏（別の音）＋ gain × 伴奏 ＋ 声 */
    std::vector<float> original (const std::vector<float>& b, const std::vector<Note>& notes, double lead, float gain)
    {
        const auto pre = (size_t) (lead * sr);
        std::vector<float> x (pre + b.size(), 0.0f);
        juce::Random rnd (3);
        for (size_t i = 0; i < pre; ++i)
            x[i] = 0.04f * (rnd.nextFloat() * 2.0f - 1.0f);
        for (size_t i = 0; i < b.size(); ++i)
            x[pre + i] = gain * b[i];
        for (auto& nt : notes)
        {
            double phase = 0.0;
            const auto s0 = pre + (size_t) (nt.start * sr), s1 = pre + (size_t) (nt.end * sr);
            for (size_t i = s0; i < s1 && i < x.size(); ++i)
            {
                phase += twoPi * midiHz (nt.midi) / sr;
                const auto env = std::min ({ 1.0, (double) (i - s0) / (0.02 * sr), (double) (s1 - i) / (0.02 * sr) });
                x[i] += (float) (0.2 * env * (0.6 * std::sin (phase) + 0.5 * std::sin (2 * phase) + 0.2 * std::sin (3 * phase)));
            }
        }
        return x;
    }

    std::vector<float> karaoke (const std::vector<float>& b, double lead)
    {
        std::vector<float> x ((size_t) (lead * sr), 0.0f);
        x.insert (x.end(), b.begin(), b.end());
        return x;
    }
}

class RefPitchTests : public juce::UnitTest
{
public:
    RefPitchTests() : juce::UnitTest ("RefPitch", "VoiceBooth") {}

    void runTest() override
    {
        const double seconds = 30.0;
        const auto b = band (seconds, 5);
        const auto notes = melody (seconds, 9);

        beginTest ("same mix: original minus karaoke gives the melody on the karaoke's timeline");
        {
            const auto orig = original (b, notes, 2.5, 0.93f);
            const auto kar = karaoke (b, 0.8);
            const auto align = alignReference (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr);
            expect (align.found());
            const auto r = referencePitch (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr, align);
            expect (r.status == RefPitchResult::Status::ok);
            expectLessThan (r.cleanDb, -30.0f);

            // 音符の中（頭と終わり 40 ms を除く）はその音、休み（音符から 60 ms 以上離れた所）は声なし
            int inNote = 0, right = 0, inRest = 0, voicedRest = 0;
            for (auto& p : r.points)
            {
                const auto t = (double) p.songSample / sr - 0.8;   // 伴奏の時間
                const Note* in = nullptr;
                bool nearNote = false;
                for (auto& nt : notes)
                {
                    if (t >= nt.start + 0.04 && t < nt.end - 0.04) in = &nt;
                    if (t >= nt.start - 0.06 && t < nt.end + 0.06) nearNote = true;
                }
                if (in != nullptr)
                {
                    ++inNote;
                    if (p.confidence >= 0.5f && std::abs (p.midi - (float) in->midi) * 100.0f < 15.0f)
                        ++right;
                }
                else if (! nearNote && t > 3.0)
                {
                    ++inRest;
                    if (p.confidence >= 0.5f)
                        ++voicedRest;
                }
            }
            expectGreaterThan (inNote, 1000);
            expectGreaterThan (right, inNote * 95 / 100);
            expectLessThan (voicedRest, juce::jmax (1, inRest / 20));
        }

        beginTest ("separated vocals (B16): the melody lands on the karaoke's timeline through the alignment");
        {
            const auto orig = original (b, notes, 2.5, 0.93f);
            const auto kar = karaoke (b, 0.8);
            const auto align = alignReference (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr);
            expect (align.found());
            // 分離で取り出した声のつもり：伴奏なしの原曲（原曲の時間）
            const auto vocals = original (std::vector<float> (b.size(), 0.0f), notes, 2.5, 0.0f);
            const auto r = pitchFromVocals (vocals.data(), (juce::int64) vocals.size(), (juce::int64) kar.size(), sr, align);
            expect (r.status == RefPitchResult::Status::ok);
            int inNote = 0, right = 0;
            for (auto& p : r.points)
            {
                const auto t = (double) p.songSample / sr - 0.8;
                for (auto& nt : notes)
                    if (t >= nt.start + 0.04 && t < nt.end - 0.04)
                    {
                        ++inNote;
                        if (p.confidence >= 0.5f && std::abs (p.midi - (float) nt.midi) * 100.0f < 15.0f)
                            ++right;
                    }
            }
            expectGreaterThan (inNote, 1000);
            expectGreaterThan (right, inNote * 97 / 100);
        }

        beginTest ("a different mix (re-recorded karaoke), a faster version or another song: no reference line");
        {
            const auto orig = original (b, notes, 2.5, 1.0f);

            // 別の録音：同じ譜面だが打音の雑音と和音の鳴りが違う（相関はあるが引けない）
            const auto other = band (seconds, 6);
            auto rerecorded = karaoke (b, 0.8);
            for (size_t i = 0; i < other.size(); ++i)
                rerecorded[(size_t) (0.8 * sr) + i] = 0.6f * rerecorded[(size_t) (0.8 * sr) + i] + 0.6f * other[i];
            const auto a1 = alignReference (orig.data(), (juce::int64) orig.size(), rerecorded.data(), (juce::int64) rerecorded.size(), sr);
            const auto r1 = referencePitch (orig.data(), (juce::int64) orig.size(), rerecorded.data(), (juce::int64) rerecorded.size(), sr, a1);
            expect (r1.status != RefPitchResult::Status::ok);
            expect (r1.points.empty());

            // 速さ違いの版（比が 1 でない）は引かない
            const auto kar = karaoke (b, 0.8);
            auto stretched = alignReference (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr);
            stretched.tempoRatio = 1.002;
            expect (referencePitch (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr, stretched).status
                    == RefPitchResult::Status::needsSeparation);

            // 合わない（時間合わせの結果が無い）
            expect (referencePitch (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr, AlignResult {}).status
                    == RefPitchResult::Status::notAligned);
        }

        beginTest ("progress is reported and cancelling stops with no points");
        {
            const auto orig = original (b, notes, 1.0, 1.0f);
            const auto kar = karaoke (b, 1.0);
            const auto align = alignReference (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr);
            float last = 0.0f;
            const auto r = referencePitch (orig.data(), (juce::int64) orig.size(), kar.data(), (juce::int64) kar.size(), sr, align,
                                           [&last] (float p) { last = p; return p < 0.5f; });
            expect (r.status == RefPitchResult::Status::cancelled);
            expect (r.points.empty());
            expectGreaterOrEqual (last, 0.5f);
        }
    }
};

static RefPitchTests refPitchTests;
} // namespace vb::analysis
