#include "analysis/AnalysisCache.h"

/*  解析の結果の保存（AnalysisCache）：同じ入力なら同じ鍵・少しでも違えば別の鍵、保存したものが同じに戻る、壊れたファイルは使わない */

namespace vb::analysis::cache
{
class AnalysisCacheTests : public juce::UnitTest
{
public:
    AnalysisCacheTests() : juce::UnitTest ("AnalysisCache", "VoiceBooth") {}

    void runTest() override
    {
        std::vector<float> a (48000);
        for (size_t i = 0; i < a.size(); ++i)
            a[i] = (float) std::sin (0.01 * (double) i);

        beginTest ("the key is the same for the same samples and changes when one sample, the length, the rate or the model changes");
        {
            const auto h = hashSamples (a.data(), (juce::int64) a.size());
            expect (h == hashSamples (a.data(), (juce::int64) a.size()));
            auto b = a;
            b[30000] = std::nextafter (b[30000], 1.0f);   // いちばん小さい違い
            expect (h != hashSamples (b.data(), (juce::int64) b.size()));
            expect (h != hashSamples (a.data(), (juce::int64) a.size() - 1));
            expect (mix (h, 44100.0) != mix (h, 48000.0));
            expect (mix (h, juce::String ("rmvpe|a")) != mix (h, juce::String ("rmvpe|b")));
        }

        beginTest ("pitch frames come back exactly; a cut or foreign file is rejected");
        {
            const std::vector<std::pair<float, float>> f { { 5700.5f, 0.91f }, { 0.0f, 0.02f }, { 6012.25f, 0.99f } };
            std::vector<std::pair<float, float>> back;
            const auto data = encodePitch (f);
            expect (decodePitch (data, back));
            expect (back == f);

            juce::MemoryBlock cut (data.getData(), data.getSize() - 3);
            expect (! decodePitch (cut, back));
            expect (! decodePitch (encodeAlign ({}), back));
        }

        beginTest ("an alignment comes back exactly, with its covered spans");
        {
            AlignResult r;
            r.quality = AlignResult::Quality::rough;
            r.offsetSamples = -88200;
            r.tempoRatio = 1.0012;
            r.confidence = 0.42;
            r.windowsUsed = 30;
            r.windowsAgreeing = 27;
            r.covered = { { 0, 441000, -88200, 1.0012 }, { 500000, 900000, -90000, 1.0 } };
            AlignResult back;
            expect (decodeAlign (encodeAlign (r), back));
            expect (back.quality == r.quality && back.offsetSamples == r.offsetSamples && back.windowsUsed == 30 && back.windowsAgreeing == 27);
            expect (juce::exactlyEqual (back.tempoRatio, r.tempoRatio) && juce::exactlyEqual (back.confidence, r.confidence));
            expectEquals ((int) back.covered.size(), 2);
            expect (back.covered[1].karaokeStart == 500000 && back.covered[1].karaokeEnd == 900000 && back.covered[1].offsetSamples == -90000);
            expect (! decodeAlign (encodePitch ({}), back));
        }

        beginTest ("saved results load from the folder; with no folder nothing is saved; a broken file is not used");
        {
            const auto dir = juce::File::createTempFile ("vbcache");
            setFolder (dir);
            const std::vector<std::pair<float, float>> f { { 6000.0f, 0.9f } };
            std::vector<std::pair<float, float>> back;
            expect (! loadPitch (7, back));
            savePitch (7, f);
            expect (loadPitch (7, back) && back == f);
            expect (! loadPitch (8, back));

            for (auto& file : dir.findChildFiles (juce::File::findFiles, false, "pitch-*.bin"))
                file.replaceWithText ("broken");
            expect (! loadPitch (7, back));

            setFolder ({});
            savePitch (9, f);
            expect (! loadPitch (9, back));
            dir.deleteRecursively();
        }
    }
};

static AnalysisCacheTests analysisCacheTests;
} // namespace vb::analysis::cache
