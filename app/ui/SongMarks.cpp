#include "SongMarks.h"
#include "Timeline.h"

namespace vb::marks
{
namespace
{
    enum MenuId
    {
        kindBase = 100,          // + presetKinds() の番号
        customName = 200,
        goTo, loop, remove, downbeat
    };

    void addKindItems (juce::PopupMenu& menu, const juce::String& current)
    {
        const auto& kinds = song::presetKinds();
        for (int i = 0; i < (int) kinds.size(); ++i)
            menu.addItem (kindBase + i, kindName (kinds[(size_t) i]), true, kinds[(size_t) i] == current);
        menu.addItem (customName, tr ("section.menu.customName"));
    }

    juce::PopupMenu::Options optionsFor (juce::Rectangle<int> screenArea)
    {
        return juce::PopupMenu::Options().withTargetScreenArea (screenArea).withStandardItemHeight (28);
    }
}

juce::String kindName (const juce::String& k)
{
    namespace kind = song::kind;
    if (k == kind::intro)      return tr ("section.kind.intro");
    if (k == kind::verseA)     return tr ("section.kind.verseA");
    if (k == kind::verseB)     return tr ("section.kind.verseB");
    if (k == kind::chorus)     return tr ("section.kind.chorus");
    if (k == kind::interlude)  return tr ("section.kind.interlude");
    if (k == kind::verseC)     return tr ("section.kind.verseC");
    if (k == kind::dropChorus) return tr ("section.kind.dropChorus");
    if (k == kind::lastChorus) return tr ("section.kind.lastChorus");
    if (k == kind::outro)      return tr ("section.kind.outro");
    if (k == kind::generic)    return tr ("section.kind.generic");
    return {};
}

juce::String sectionName (const song::Sections& list, int index)
{
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return {};

    const auto& s = list[(size_t) index];
    auto name = s.isCustom() ? s.name : kindName (s.kind);
    if (name.isEmpty())
        name = kindName (song::kind::generic);

    const auto n = song::sectionNumbers (list)[(size_t) index];
    return n > 0 ? tr ("section.numbered", name, n) : name;
}

juce::String positionText (const dummy::Session& s, int64 sample)
{
    if (! s.tempoKnown())
        return formatTime (sample, s.sampleRate(), true);
    const auto bb = s.barBeatAt (sample);
    return juce::String (bb.bar) + "." + juce::String (bb.beat);
}

void showNameMenu (UiSession& session, Actions& actions, int index, juce::Rectangle<int> screenArea)
{
    const auto& list = session->project.sections;
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("section.menu.name"));
    addKindItems (menu, list[(size_t) index].kind);

    auto* u = &session;
    auto* a = &actions;
    menu.showMenuAsync (optionsFor (screenArea), [u, a, index] (int result)
    {
        const auto& kinds = song::presetKinds();
        if (result >= kindBase && result < kindBase + (int) kinds.size())
            u->renameSection (index, kinds[(size_t) (result - kindBase)]);
        else if (result == customName && a->editSectionName)
            a->editSectionName (index);
    });
}

void showSectionMenu (UiSession& session, Actions& actions, int index, juce::Rectangle<int> screenArea)
{
    const auto& list = session->project.sections;
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;

    session.selectSection (index);

    juce::PopupMenu menu;
    menu.addSectionHeader (sectionName (list, index));
    addKindItems (menu, list[(size_t) index].kind);
    menu.addSeparator();
    menu.addItem (goTo, tr ("section.menu.goTo"));
    menu.addItem (loop, tr ("section.menu.loop"));
    menu.addSeparator();
    menu.addItem (remove, tr ("section.menu.remove"));

    auto* u = &session;
    auto* a = &actions;
    menu.showMenuAsync (optionsFor (screenArea), [u, a, index] (int result)
    {
        const auto& kinds = song::presetKinds();
        if (result >= kindBase && result < kindBase + (int) kinds.size())
            u->renameSection (index, kinds[(size_t) (result - kindBase)]);
        else if (result == customName && a->editSectionName) a->editSectionName (index);
        else if (result == goTo)   u->goToSection (index);
        else if (result == loop)   u->loopSection (index);
        else if (result == remove) u->removeSection (index);
    });
}

void showRulerMenu (UiSession& session, Actions& actions, int64 sample, bool snap, juce::Rectangle<int> screenArea)
{
    juce::PopupMenu here;
    addKindItems (here, {});

    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("section.menu.here", positionText (session.get(), snap ? song::snapToBar (session->project.tempo, sample, session->sampleRate()) : sample)));
    menu.addSubMenu (tr ("section.menu.addHere"), here);
    menu.addItem (downbeat, tr ("section.menu.downbeatHere"));

    auto* u = &session;
    auto* a = &actions;
    menu.showMenuAsync (optionsFor (screenArea), [u, a, sample, snap] (int result)
    {
        const auto& kinds = song::presetKinds();
        if (result >= kindBase && result < kindBase + (int) kinds.size())
            u->addSection (sample, kinds[(size_t) (result - kindBase)], {}, snap);
        else if (result == customName)
        {
            const auto index = u->addSection (sample, song::kind::generic, {}, snap);
            if (a->editSectionName) a->editSectionName (index);
        }
        else if (result == downbeat)
            u->setDownbeat (sample);   // 1 小節目の頭は吸い付かせない（その位置そのもの）
    });
}
} // namespace vb::marks
