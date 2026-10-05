#include "Help.h"
#include "UiSession.h"
#include "models/ModelDownloader.h"

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
        { Topic::otherProblem,      "help.otherProblem.title",      "help.otherProblem.body",      Action::openIssues },
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
        case Action::openIssues:     return "help.action.issues";
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
} // namespace vb::help
