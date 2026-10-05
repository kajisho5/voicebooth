#include "Help.h"
#include "UiSession.h"
#include "models/ModelDownloader.h"
#include "i18n/I18n.h"

namespace vb::help
{
namespace
{
    struct Info { Topic topic; const char* title; const char* body; Action action; };
    constexpr Info infos[numTopics] = {
        { Topic::noSound,           "help.noSound.title",           "help.noSound.body",           Action::openSetup },
        { Topic::noMic,             "help.noMic.title",             "help.noMic.body",             Action::openSetup },
        { Topic::monitorLate,       "help.monitorLate.title",       "help.monitorLate.body",       Action::openSetup },
        { Topic::recordOffset,      "help.recordOffset.title",      "help.recordOffset.body",      Action::openLatency },
        { Topic::recordNotStarting, "help.recordNotStarting.title", "help.recordNotStarting.body", Action::none },
        { Topic::noGuideLine,       "help.noGuideLine.title",       "help.noGuideLine.body",       Action::downloadModels },
        { Topic::guideOffset,       "help.guideOffset.title",       "help.guideOffset.body",       Action::none },
        { Topic::separationSlow,    "help.separationSlow.title",    "help.separationSlow.body",    Action::none },
        { Topic::noHarmonyLine,     "help.noHarmonyLine.title",     "help.noHarmonyLine.body",     Action::downloadModels },
        { Topic::modelDownload,     "help.modelDownload.title",     "help.modelDownload.body",     Action::downloadModels },
        { Topic::dropouts,          "help.dropouts.title",          "help.dropouts.body",          Action::openSetup },
        { Topic::exportWhere,       "help.exportWhere.title",       "help.exportWhere.body",       Action::none },
        { Topic::otherProblem,      "help.otherProblem.title",      "help.otherProblem.body",      Action::report },
    };
}

const char* titleKey (Topic t) { return infos[(size_t) t].title; }
const char* bodyKey (Topic t)  { return infos[(size_t) t].body; }
Action action (Topic t)        { return infos[(size_t) t].action; }

const char* actionKey (Action a)
{
    switch (a)
    {
        case Action::none:           break;
        case Action::openSetup:      return "help.action.setup";
        case Action::openLatency:    return "help.action.latency";
        case Action::downloadModels: return "help.action.models";
        case Action::report:         return "help.action.report";
    }
    return nullptr;
}

std::vector<Item> items (const UiSession& session)
{
    const auto& s = session.get();
    const bool songOpen = s.engineAttached && s.songOriginal != nullptr;

    // いまの状態に関係するか（分かるものだけ。分からないものは印を付けない）
    auto relevant = [&] (Topic t)
    {
        switch (t)
        {
            case Topic::noSound:        return s.engineAttached && ! s.output.open;
            case Topic::noMic:          return s.engineAttached && (! s.input.open || s.input.permission == audio::MicPermission::denied);
            case Topic::separationSlow: return s.separating;
            case Topic::noGuideLine:    return songOpen && s.guideNeedsSeparation && ! session.separationAvailable();
            case Topic::guideOffset:    return songOpen && s.guideAlignRough && ! s.refPitch.empty();
            case Topic::modelDownload:  return s.modelDl.stage == (int) models::DownloadStatus::Stage::interrupted
                                            || s.modelDl.stage == (int) models::DownloadStatus::Stage::failed;
            case Topic::monitorLate: case Topic::recordOffset: case Topic::recordNotStarting: case Topic::noHarmonyLine:
            case Topic::dropouts: case Topic::exportWhere: case Topic::otherProblem:
                break;
        }
        return false;
    };

    std::vector<Item> first, rest;
    for (auto& i : infos)
        (relevant (i.topic) ? first : rest).push_back ({ i.topic, relevant (i.topic) });
    first.insert (first.end(), rest.begin(), rest.end());
    return first;
}

//==============================================================================
juce::String guideUrl()
{
    const juce::String code (i18n::codeOf (i18n::current()));
    return "https://github.com/kajisho5/voicebooth/blob/main/" + (code == "ja" ? juce::String ("README.md") : "README." + code + ".md");
}

namespace
{
    const char* modeName (project::Mode m)
    {
        switch (m)
        {
            case project::Mode::easy:     return "easy";
            case project::Mode::standard: return "standard";
            case project::Mode::pro:      return "pro";
        }
        return "?";
    }

    const char* problemName (audio::InputProblem p)
    {
        using P = audio::InputProblem;
        switch (p)
        {
            case P::none:             return "none";
            case P::noDevice:         return "no device";
            case P::openFailed:       return "open failed";
            case P::noChannels:       return "no input channels";
            case P::permissionAsking: return "asking for permission";
            case P::permissionDenied: return "permission denied";
            case P::stalled:          return "stalled";
        }
        return "?";
    }

    juce::String rateAndBuffer (double rate, int buffer)
    {
        return juce::String (juce::roundToInt (rate)) + " Hz, buffer " + juce::String (buffer);
    }
}

juce::String environmentReport (const UiSession& session)
{
    const auto& s = session.get();
    juce::StringArray lines;
    lines.add ("VoiceBooth " + update::currentVersion());
    lines.add ("OS: " + juce::SystemStats::getOperatingSystemName() + (juce::SystemStats::isOperatingSystem64Bit() ? " (64-bit)" : ""));
    lines.add ("CPU: " + juce::SystemStats::getCpuModel().trim() + ", " + juce::String (juce::SystemStats::getNumCpus()) + " threads");
    lines.add ("Memory: " + juce::String (juce::roundToInt (juce::SystemStats::getMemorySizeInMegabytes() / 1024.0)) + " GB");
    lines.add ("Language: " + juce::String (i18n::codeOf (i18n::current())) + ", mode: " + modeName (s.mode));

    if (! s.engineAttached)
    {
        lines.add ("Audio: no audio engine");
    }
    else
    {
        lines.add ("Audio driver: " + (s.output.typeName.isNotEmpty() ? s.output.typeName : s.input.typeName));
        lines.add ("Output: " + (s.output.open ? s.output.deviceName + " (" + rateAndBuffer (s.output.sampleRate, s.output.bufferSize) + ")"
                                                : "not open" + (s.output.error.isNotEmpty() ? " (" + s.output.error + ")" : juce::String()))
                   + (s.output.stalled ? ", stalled" : ""));
        lines.add ("Input: " + (s.input.open ? s.input.deviceName + ", ch " + juce::String (s.input.channel + 1) + "/" + juce::String (s.input.numChannels)
                                                 + " (" + rateAndBuffer (s.input.sampleRate, s.input.bufferSize) + ")"
                                              : "not open (" + juce::String (problemName (s.input.problem)) + ")"
                                                 + (s.input.error.isNotEmpty() ? " " + s.input.error : juce::String()))
                   + (s.input.silent ? ", silent" : ""));
        const auto l = latencyDisplay (s);
        lines.add ("Latency: " + (l.known ? juce::String (l.samples) + " samples, "
                                              + (l.manual ? "manual" : l.measured ? "measured" : l.estimated ? "estimated from buffer" : "reported by device")
                                           : juce::String ("unknown")));
    }

    if (s.songOriginal != nullptr)
        lines.add ("Song: " + juce::String (s.songRate) + " Hz, project " + juce::String (s.sampleRate()) + " Hz, recording "
                   + juce::String (s.targetRate()) + " Hz " + (s.recordFloat ? "32-bit float" : "24-bit"));
    else
        lines.add ("Song: none");

    using SC = separation::SeparatorClient;
    lines.add (juce::String ("Models: separation ") + (SC::modelInstalled() ? "yes" : "no") + ", lead/harmony " + (SC::karaokeInstalled() ? "yes" : "no")
               + ", pitch " + (SC::pitchModelFile().existsAsFile() ? "yes" : "no") + (s.separating ? ", separating now" : ""));
    return lines.joinIntoString ("\n");
}

namespace
{
    juce::String issueHead()
    {
        return "### " + i18n::tr ("report.issue.what") + "\n\n\n"
             + "### " + i18n::tr ("report.issue.steps") + "\n\n\n"
             + "### " + i18n::tr ("report.issue.env") + "\n\n";
    }
}

namespace
{
    juce::String bodyWith (const juce::String& environment)
    {
        return issueHead() + "```\n" + environment + "\n```\n";
    }
}

juce::String issueBody (const UiSession& session)
{
    return bodyWith (environmentReport (session));
}

juce::String reportUrl (const UiSession& session)
{
    return reportUrl (environmentReport (session));
}

juce::String reportUrl (const juce::String& environment)
{
    const auto title = i18n::tr ("report.issue.title");
    const auto full = newIssueUrl (title, bodyWith (environment));
    if (full.length() <= maxUrlLength)
        return full;
    return newIssueUrl (title, issueHead() + i18n::tr ("report.issue.paste") + "\n");
}

juce::String newIssueUrl (const juce::String& title, const juce::String& body)
{
    return juce::String (issuesUrl) + "/new?title=" + juce::URL::addEscapeChars (title, true)
         + "&body=" + juce::URL::addEscapeChars (body, true);
}
} // namespace vb::help
