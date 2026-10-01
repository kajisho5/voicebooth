#include "SettingsDialog.h"

namespace vb
{
namespace
{
    constexpr int rowH = 46;

    juce::StringArray languageNames()
    {
        juce::StringArray a;
        for (auto& l : i18n::available())
            a.add (juce::String::fromUTF8 (l.nativeName));
        return a;
    }

    int languageIndex (i18n::Language lang)
    {
        const auto& list = i18n::available();
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].id == lang)
                return (int) i;
        return 0;
    }
}

SettingsDialog::SettingsDialog (UiSession& u)
    : DialogPanel (tr ("settings.title"), tr ("settings.micro")), SessionView (u),
      language (languageNames(), languageIndex (i18n::current())),
      mode ({ tr ("mode.easy"), tr ("mode.standard"), tr ("mode.pro") }, (int) u->mode),
      tolerance ({ "20", "30", "50" }, u->pitchToleranceCents <= 20.0f ? 0 : (u->pitchToleranceCents <= 30.0f ? 1 : 2)),
      countIn ({ tr ("transport.countIn.off"), "1", "2" }, u->countInBars),
      crossfade ({ "0", "5", "8", "20" }, 2),
      octaveAlign (tr ("pitch.octaveAlign")),
      openSetup (tr ("settings.device.open")),
      cacheKey (tr ("settings.cache.change"))
{
    language.onChange = [this] (int i) { if (onLanguage) onLanguage (i18n::available()[(size_t) i].id); };
    mode.onChange = [this] (int i) { session.setMode ((project::Mode) i); };
    tolerance.onChange = [this] (int i) { const float v[] = { 20.0f, 30.0f, 50.0f }; session.setPitchTolerance (v[i]); };
    countIn.onChange = [this] (int i) { session.setCountIn (i); };

    octaveAlign.withLed().withToggle (false);
    octaveAlign.onClick = [this] { session.setOctaveAlign (! state().octaveAlign); };
    openSetup.withIcon (Icon::mic);
    openSetup.onClick = [this] { if (onOpenSetup) onOpenSetup(); };
    cacheKey.withIcon (Icon::folder);

    for (auto* s : { &tolerance, &countIn, &crossfade })
        s->setFont (mono (11.5f, Weight::medium));

    rows = {
        { tr ("settings.language"),   tr ("settings.language.note"),   &language,    juce::jmax (220, language.idealWidth()) },
        { tr ("settings.mode"),       tr ("settings.mode.note"),       &mode,        260 },
        { tr ("settings.tolerance"),  tr ("settings.tolerance.note"),  &tolerance,   200 },
        { tr ("settings.octave"),     tr ("settings.octave.note"),     &octaveAlign, 0 },
        { tr ("settings.countIn"),    {},                              &countIn,     200 },
        { tr ("settings.crossfade"),  tr ("settings.crossfade.note"),  &crossfade,   240 },
        { tr ("settings.device"),     tr ("settings.device.note"),     &openSetup,   0 },
        { tr ("settings.cache"),      utf8 ("~/Music/VoiceBooth/Cache"), &cacheKey,  0 },
        { tr ("settings.theme"),      tr ("settings.theme.note"),      nullptr,      0 },
    };

    for (auto& r : rows)
        if (r.control != nullptr)
            addAndMakeVisible (r.control);

    addFooterKey (tr ("common.close"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });

    setSize (760, headerH + 14 + rowH * (int) rows.size() + footerH + 12);
    onSessionChanged (change::all);
}

void SettingsDialog::onSessionChanged (juce::uint32)
{
    const auto& s = state();
    mode.setSelected ((int) s.mode, juce::dontSendNotification);
    countIn.setSelected (s.countInBars, juce::dontSendNotification);
    octaveAlign.setToggleState (s.octaveAlign, juce::dontSendNotification);
    octaveAlign.setButtonText (s.octaveAlign ? tr ("common.on") : tr ("common.off"));

    // クロスフェードはプロのみ編集（DESIGN 6.4）
    crossfade.setEnabled (s.mode == project::Mode::pro);
    crossfade.setAlpha (crossfade.isEnabled() ? 1.0f : 0.45f);
    repaint();
}

void SettingsDialog::layoutBody (juce::Rectangle<int> r)
{
    rowAreas.clear();
    for (auto& row : rows)
    {
        auto a = r.removeFromTop (rowH);
        rowAreas.push_back (a);

        if (row.control == nullptr) continue;

        auto c = a.removeFromRight (juce::jmax (260, row.controlWidth));
        if (auto* k = dynamic_cast<KeyButton*> (row.control))
        {
            k->setSize (10, 32);
            const auto w = juce::jmax (96, k->idealWidth());
            k->setBounds (c.removeFromRight (w).withSizeKeepingCentre (w, 32));
        }
        else
        {
            row.control->setBounds (c.removeFromRight (row.controlWidth).withSizeKeepingCentre (row.controlWidth, 32));
        }
    }
}

void SettingsDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto a = rowAreas[i].toFloat();
        if (i + 1 < rows.size())
            paint::hline (g, a.getBottom() - 1.0f, a.getX(), a.getRight(), colours::grid);

        auto text = a.withTrimmedRight ((float) juce::jmax (260, rows[i].controlWidth) + 20.0f);
        g.setColour (colours::text);
        g.setFont (sans (13.0f, Weight::medium));
        g.drawText (rows[i].label, text.removeFromTop (rows[i].note.isEmpty() ? text.getHeight() : text.getHeight() * 0.55f),
                    rows[i].note.isEmpty() ? juce::Justification::centredLeft : juce::Justification::bottomLeft, true);
        if (rows[i].note.isNotEmpty())
        {
            g.setColour (colours::textMute);
            g.setFont (sans (11.0f));
            g.drawText (rows[i].note, text, juce::Justification::topLeft, true);
        }

        if (rows[i].control == nullptr)
        {
            g.setColour (colours::textDim);
            g.setFont (sans (12.0f));
            g.drawText (tr ("settings.theme.value"), a.removeFromRight (260.0f), juce::Justification::centredRight, false);
        }
    }

    // ピッチ許容・クロスフェードの単位
    for (auto* c : { (juce::Component*) &tolerance, (juce::Component*) &crossfade })
    {
        const auto b = c->getBounds();
        paint::microLabel (g, juce::Rectangle<float> ((float) b.getX() - 46.0f, (float) b.getY(), 40.0f, (float) b.getHeight()),
                           c == &tolerance ? tr ("unit.cent") : tr ("unit.ms"), colours::textMute, juce::Justification::centredRight);
    }
}
} // namespace vb
