#include "Reasons.h"

namespace vb::i18n
{
ReasonKey reasonKey (const juce::String& reasonIn)
{
    const auto reason = reasonIn.trim();
    // 決まった文字列
    struct Exact { const char* text; const char* key; };
    static const Exact exact[] = {
        { "file exists",                            "reason.fileExists" },
        { "no sample rate",                         "reason.noDevice" },
        { "no device",                              "reason.noDevice" },
        { "no output device",                       "reason.noDevice" },
        { "no input",                               "reason.noInput" },
        { "no output",                              "reason.noOutput" },
        { "sample rate",                            "reason.sampleRate" },
        { "recording",                              "reason.recording" },
        { "can't create WAV writer",                "reason.writeFailed" },
        { "refmix: can't create WAV writer",        "reason.writeFailed" },
        { "write failed or cancelled",              "reason.writeFailed" },
        { "refmix: write failed",                   "reason.writeFailed" },
        { "can't write output",                     "reason.writeFailed" },
        { "can't write the off vocal",              "reason.writeFailed" },
        { "cancelled",                              "reason.cancelled" },
        { "nothing recorded",                       "reason.nothingRecorded" },
        { "no song",                                "reason.noSong" },
        { "busy",                                   "reason.busy" },
        { "can't read the original",                "reason.cantReadOriginal" },
        { "can't prepare the input",                "reason.cantReadOriginal" },
        { "can't read the result",                  "reason.noResult" },
        { "missing output",                         "reason.noResult" },
        { "can't start the separator",              "reason.cantStart" },
        { "the separator stopped unexpectedly",     "reason.crashed" },
        { "bad length",                             "reason.badLength" },
        { "the model list's signature doesn't match", "reason.badSignature" },
        { "the model isn't in the list",            "reason.modelNotListed" },
        { "not enough disk space",                  "reason.diskFull" },
        { "verification failed",                    "reason.verifyFailed" },
    };
    for (auto& e : exact)
        if (reason == e.text)
            return { e.key, {} };

    // 頭が決まっていて、後ろにファイル名・URL などが付く物
    struct Prefix { const char* text; const char* key; };
    static const Prefix prefixes[] = {
        { "can't reach ",                   "reason.cantReach" },
        { "bad model list: ",               "reason.badModelList" },
        { "missing take ",                  "reason.missingTake" },
        { "can't read ",                    "reason.cantRead" },
        { "can't move to ",                 "reason.cantWrite" },
        { "can't move ",                    "reason.cantWrite" },
        { "can't create ",                  "reason.cantWrite" },
        { "can't write ",                   "reason.cantWrite" },
        { "refmix: can't render ",          "reason.cantRender" },
        { "can't open ",                    "reason.cantOpenDevice" },
    };
    for (auto& p : prefixes)
        if (reason.startsWith (p.text))
            return { p.key, reason.substring ((int) std::strlen (p.text)).trim() };

    // "sample rate of take3 differs from the song"
    if (reason.startsWith ("sample rate of ") && reason.endsWith (" differs from the song"))
        return { "reason.rateDiffers", reason.fromFirstOccurrenceOf ("sample rate of ", false, false)
                                             .upToLastOccurrenceOf (" differs from the song", false, false) };
    return {};
}
} // namespace vb::i18n
