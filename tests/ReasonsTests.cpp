#include "i18n/Reasons.h"
#include "i18n/I18n.h"

namespace vb::i18n
{
/*  失敗の理由（処理が返す英語の決まった文字列）→ 翻訳表のキー（#23）。知らない理由はそのまま */
class ReasonsTests : public juce::UnitTest
{
public:
    ReasonsTests() : juce::UnitTest ("Reasons", "VoiceBooth") {}

    void expectKey (const char* reason, const char* key, const juce::String& arg = {})
    {
        const auto r = reasonKey (reason);
        expect (r.key != nullptr && juce::String (r.key) == key, juce::String (reason) + " -> " + (r.key != nullptr ? r.key : "null"));
        expectEquals (r.arg, arg, reason);
    }

    void runTest() override
    {
        beginTest ("fixed reasons from recording, export, separation and models");
        {
            expectKey ("file exists", "reason.fileExists");
            expectKey ("write failed or cancelled", "reason.writeFailed");
            expectKey ("refmix: write failed", "reason.writeFailed");
            expectKey ("busy", "reason.busy");
            expectKey ("can't read the original", "reason.cantReadOriginal");   // "can't read " の頭より先に決まった文字列
            expectKey ("the separator stopped unexpectedly", "reason.crashed");
            expectKey ("the model list's signature doesn't match", "reason.badSignature");
            expectKey ("  cancelled  ", "reason.cancelled");
        }

        beginTest ("reasons with a file name or URL after a fixed start");
        {
            expectKey ("can't reach https://example.com/models.json", "reason.cantReach", "https://example.com/models.json");
            expectKey ("can't read Audio/Takes/Main_take1.wav", "reason.cantRead", "Audio/Takes/Main_take1.wav");
            expectKey ("can't write zip", "reason.cantWrite", "zip");
            expectKey ("missing take take3", "reason.missingTake", "take3");
            expectKey ("bad model list: missing \"models\"", "reason.badModelList", "missing \"models\"");
            expectKey ("sample rate of take2 differs from the song", "reason.rateDiffers", "take2");
        }

        beginTest ("reasons found in the bug check (2026-10-05): file name in front, separation and model failures");
        {
            expectKey ("vocal_dry.wav: missing take take3", "reason.missingTake", "take3");
            expectKey ("Main_take1.wav: write failed (the file is incomplete)", "reason.writeFailed");
            expectKey ("separation failed", "reason.separationFailed");
            expectKey ("lead separation failed", "reason.separationFailed");
            expectKey ("separation failed: std::bad_alloc", "reason.separationFailedDetail", "std::bad_alloc");
            expectKey ("can't load model: model.onnx", "reason.cantLoadModel", "model.onnx");
            expectKey ("input must be 44100 Hz", "reason.cantReadOriginal");
            expect (reasonKey ("Audio/x.wav: busy").key == nullptr, "a path in front is not taken as a file name");
        }

        beginTest ("placeholders are filled in one pass (a {1} inside a value stays as it is)");
        {
            expectEquals (substitute ("{0} is too long (up to {1} minutes)", { "a{1}.mp3", "20" }),
                          juce::String ("a{1}.mp3 is too long (up to 20 minutes)"));
            expectEquals (substitute ("{1} / {0} / {2}", { "x", "y" }), juce::String ("y / x / {2}"), "a missing value stays as it is");
            expectEquals (substitute ("{} {a} {0", { "x" }), juce::String ("{} {a} {0"));
        }

        beginTest ("unknown reasons (OS messages) are kept as they are");
        {
            expect (reasonKey ("Access is denied.").key == nullptr);
            expect (reasonKey ({}).key == nullptr);
            expect (reasonKey ("busy now").key == nullptr);
        }
    }
};

static ReasonsTests reasonsTests;
} // namespace vb::i18n
