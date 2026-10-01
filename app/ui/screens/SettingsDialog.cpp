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

    /** 「このパソコン」の 1 行目：OS · CPU · コア · メモリ · 空き（DESIGN 11.6.1） */
    juce::String systemSummary (const system::Info& i)
    {
        juce::StringArray parts { i.osName, i.cpuModel + " (" + i.architecture + ")",
                                  tr ("settings.system.cores").replace ("{0}", juce::String (i.physicalCores)),
                                  tr ("settings.system.memory").replace ("{0}", juce::String ((double) i.memoryMB / 1024.0, 1)) };
        if (i.freeDiskMB >= 0)
            parts.add (tr ("settings.system.disk").replace ("{0}", juce::String (i.freeDiskMB / 1024)));
        return parts.joinIntoString (utf8 (" \xc2\xb7 "));
    }

    juce::String itemNames (const juce::Array<system::Item>& items)
    {
        juce::StringArray names;
        for (auto item : items)
            names.add (tr (item == system::Item::cores  ? "settings.system.item.cores"
                         : item == system::Item::memory ? "settings.system.item.memory"
                                                        : "settings.system.item.disk"));
        return names.joinIntoString (tr ("settings.system.sep"));
    }

    /** 内蔵は「Booth — 夜のブース」、自作は「名前 — 作者」（作者が無ければ「自作」） */
    juce::StringArray skinNames (const std::vector<skin::Skin>& skins)
    {
        juce::StringArray a;
        for (auto& s : skins)
        {
            const auto sub = s.builtIn ? tr (skin::subtitleKey (s.id).toRawUTF8())
                                       : (s.author.isNotEmpty() ? s.author : tr ("settings.skin.mine"));
            a.add (s.name + utf8 (" \xe2\x80\x94 ") + sub);
        }
        return a;
    }

    int skinIndex (const std::vector<skin::Skin>& skins, const juce::String& id)
    {
        for (size_t i = 0; i < skins.size(); ++i)
            if (skins[i].id == id)
                return (int) i;
        return 0;
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

SettingsDialog::SettingsDialog (UiSession& u, std::vector<skin::Skin> skinList, const juce::String& currentSkin)
    : DialogPanel (tr ("settings.title"), tr ("settings.micro")), SessionView (u),
      skinChoices (std::move (skinList)),
      language (languageNames(), languageIndex (i18n::current())),
      skinPicker (skinNames (skinChoices), skinIndex (skinChoices, currentSkin)),
      editSkin (tr ("settings.skin.edit")),
      newSkin (tr ("settings.skin.new")),
      mode ({ tr ("mode.easy"), tr ("mode.standard"), tr ("mode.pro") }, (int) u->mode),
      tolerance ({ "20", "30", "50" }, u->pitchToleranceCents <= 20.0f ? 0 : (u->pitchToleranceCents <= 30.0f ? 1 : 2)),
      countIn ({ tr ("transport.countIn.off"), "1", "2" }, u->countInBars),
      crossfade ({ "0", "5", "8", "20" }, 2),
      octaveAlign (tr ("pitch.octaveAlign")),
      openSetup (tr ("settings.device.open")),
      cacheKey (tr ("settings.cache.change")),
      supportKey (tr ("settings.support.open")),
      systemInfo (system::gather (juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("VoiceBooth")))
{
    language.onChange = [this] (int i) { if (onLanguage) onLanguage (i18n::available()[(size_t) i].id); };
    skinPicker.onChange = [this] (int i) { if (onSkin && juce::isPositiveAndBelow (i, (int) skinChoices.size())) onSkin (skinChoices[(size_t) i].id); };
    editSkin.withIcon (Icon::edit);
    editSkin.onClick = [this] { if (onEditSkin) onEditSkin(); };
    newSkin.withIcon (Icon::plus);
    newSkin.onClick = [this] { if (onNewSkin) onNewSkin(); };
    mode.onChange = [this] (int i) { session.setMode ((project::Mode) i); };
    tolerance.onChange = [this] (int i) { const float v[] = { 20.0f, 30.0f, 50.0f }; session.setPitchTolerance (v[i]); };
    countIn.onChange = [this] (int i) { session.setCountIn (i); };

    octaveAlign.withLed().withToggle (false);
    octaveAlign.onClick = [this] { session.setOctaveAlign (! state().octaveAlign); };
    openSetup.withIcon (Icon::mic);
    openSetup.onClick = [this] { if (onOpenSetup) onOpenSetup(); };
    cacheKey.withIcon (Icon::folder);
    supportKey.withIcon (Icon::globe);
    supportKey.onClick = [] { juce::URL ("https://github.com/sponsors/kajisho5").launchInDefaultBrowser(); };

    // このパソコンが動作環境を満たすか（DESIGN 11.6.1）。足りなくても止めない、知らせるだけ
    const auto verdict = system::evaluate (systemInfo);
    juce::String systemValue;
    juce::Colour systemLed;
    switch (verdict.level)
    {
        case system::Level::ok:
            systemValue = tr ("settings.system.ok");
            systemLed = colours::signal;
            break;
        case system::Level::belowRecommended:
            systemValue = tr ("settings.system.belowRec").replace ("{0}", itemNames (verdict.belowRecommended));
            systemLed = colours::warn;
            break;
        case system::Level::belowMinimum:
            systemValue = tr ("settings.system.belowMin").replace ("{0}", itemNames (verdict.belowMinimum));
            systemLed = colours::bad;
            break;
    }

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
        { tr ("settings.system"),     systemSummary (systemInfo),      nullptr,      330, systemValue, systemLed },
        { tr ("settings.skin"),       tr ("settings.skin.note"),       &skinPicker,  juce::jmax (200, skinPicker.idealWidth()), {}, {}, { &newSkin, &editSkin } },
        { tr ("settings.support"),    tr ("settings.support.note"),    &supportKey,  0 },
    };

    for (auto& r : rows)
    {
        if (r.control != nullptr)
            addAndMakeVisible (r.control);
        for (auto* k : r.extras)
        {
            // control の左に置くキーの分だけ、文言の幅を詰める
            k->setSize (10, 32);
            r.controlWidth += juce::jmax (72, k->idealWidth()) + 8;
            addAndMakeVisible (k);
        }
    }

    addFooterKey (tr ("common.close"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });

    setSize (820, headerH + 14 + rowH * (int) rows.size() + footerH + 12);
    onSessionChanged (change::all);
}

void SettingsDialog::onSessionChanged (juce::uint32)
{
    const auto& s = state();
    mode.setSelected ((int) s.mode, juce::dontSendNotification);
    countIn.setSelected (s.countInBars, juce::dontSendNotification);
    octaveAlign.setToggleState (s.octaveAlign, juce::dontSendNotification);
    octaveAlign.setButtonText (s.octaveAlign ? tr ("common.on") : tr ("common.off"));

    // 録音中はスキンを切り替えない（DESIGN 4.11）
    for (auto* c : { (juce::Component*) &skinPicker, (juce::Component*) &editSkin, (juce::Component*) &newSkin })
    {
        c->setEnabled (! s.isRecording);
        c->setAlpha (s.isRecording ? 0.45f : 1.0f);
    }

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
        else if (! row.extras.empty())
        {
            auto area = c.removeFromRight (row.controlWidth);
            for (auto* key : row.extras)
            {
                const auto keyW = juce::jmax (72, key->idealWidth());
                key->setBounds (area.removeFromLeft (keyW).withSizeKeepingCentre (keyW, 32));
                area.removeFromLeft (8);
            }
            row.control->setBounds (area.withSizeKeepingCentre (area.getWidth(), 32));
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

        if (rows[i].control == nullptr && rows[i].value.isNotEmpty())
        {
            auto v = a.removeFromRight ((float) juce::jmax (260, rows[i].controlWidth));
            g.setFont (sans (12.0f));
            const auto textW = juce::jmin (v.getWidth() - 18.0f, textWidth (g.getCurrentFont(), rows[i].value) + 2.0f);
            if (! rows[i].led.isTransparent())
                paint::led (g, { v.getRight() - textW - 12.0f, v.getCentreY() }, 4.0f, rows[i].led, true);
            g.setColour (rows[i].led.isTransparent() ? colours::textDim : colours::text);
            g.drawText (rows[i].value, v, juce::Justification::centredRight, true);
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
