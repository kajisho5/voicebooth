#include "Gallery.h"
#include "parts/Dropdown.h"

namespace vb
{
namespace
{
    constexpr int sectionTitleH = 26;
    constexpr int captionH = 16;

    juce::String pct (double v) { return juce::String (juce::roundToInt (v)); }
}

Gallery::Gallery()
{
    // --- Keys ---------------------------------------------------------------
    auto key = [this] (const juce::String& cap, const juce::String& text, KeyState s) -> KeyButton&
    {
        auto& k = make<KeyButton> (cap, text);
        k.setPreview (s);
        return k;
    };

    key ("normal", tr ("transport.clearRange.tooltip"), {});
    key ("hover", tr ("transport.clearRange.tooltip"), { true });
    key ("down", tr ("transport.clearRange.tooltip"), { true, true });
    key ("disabled", tr ("transport.clearRange.tooltip"), { false, false, false, false });

    make<KeyButton> ("LED off", tr ("transport.loop")).withIcon (Icon::loop).withLed().setPreview (KeyState {});
    make<KeyButton> ("LED on", tr ("transport.loop")).withIcon (Icon::loop).withLed().setPreview (KeyState { false, false, true });
    make<KeyButton> ("LED on hover", tr ("transport.click")).withIcon (Icon::metronome).withLed().setPreview (KeyState { true, false, true });

    make<KeyButton> ("icon", "").withIcon (Icon::play).setPreview (KeyState {});
    make<KeyButton> ("icon hover", "").withIcon (Icon::pause).setPreview (KeyState { true });
    make<KeyButton> ("rec off", "", KeyButton::Kind::rec).withIcon (Icon::rec).setPreview (KeyState {});
    make<KeyButton> ("rec on", "", KeyButton::Kind::rec).withIcon (Icon::rec).setPreview (KeyState { false, false, true });

    make<KeyButton> ("ghost", tr ("lyrics.pad"), KeyButton::Kind::ghost).withIcon (Icon::edit).setPreview (KeyState {});
    make<KeyButton> ("ghost hover", tr ("lyrics.pad"), KeyButton::Kind::ghost).withIcon (Icon::edit).setPreview (KeyState { true });

    make<KeyButton> ("M off", "M").withLatch (colours::warn).withFont (mono (10.5f, Weight::semibold)).setPreview (KeyState {});
    make<KeyButton> ("M on", "M").withLatch (colours::warn).withFont (mono (10.5f, Weight::semibold)).setPreview (KeyState { false, false, true });
    make<KeyButton> ("S on", "S").withLatch (colours::signal).withFont (mono (10.5f, Weight::semibold)).setPreview (KeyState { false, false, true });

    // --- Segmented ----------------------------------------------------------
    make<SegmentedKeys> ("mode", juce::StringArray { tr ("mode.easy"), tr ("mode.standard"), tr ("mode.pro") }, 1);
    make<SegmentedKeys> ("hover", juce::StringArray { tr ("transport.countIn.off"), "1", "2" }, 1).setPreviewHover (2);
    make<SegmentedKeys> ("rec mode", juce::StringArray { tr ("record.delivery"), tr ("record.practice") }, 0, colours::rec);

    // --- Encoders -----------------------------------------------------------
    make<EncoderBlock> ("unipolar", tr ("practice.tempo"), 50.0, 150.0, 100.0, 1.0, pct, "%").setCaption (tr ("practice.tempo.original", 120));
    make<EncoderBlock> ("slow", tr ("practice.tempo"), 50.0, 150.0, 75.0, 1.0, pct, "%").setCaption (tr ("practice.key.shifted"));
    make<EncoderBlock> ("bipolar 0", tr ("practice.key"), -6.0, 6.0, 0.0, 1.0,
                        [] (double v) { const auto k = juce::roundToInt (v); return (k > 0 ? "+" : "") + juce::String (k); },
                        "", true, colours::ref).setCaption (tr ("practice.key.original"));
    make<EncoderBlock> ("bipolar -3", tr ("practice.key"), -6.0, 6.0, -3.0, 1.0,
                        [] (double v) { const auto k = juce::roundToInt (v); return (k > 0 ? "+" : "") + juce::String (k); },
                        "", true, colours::ref).setCaption (tr ("practice.key.shifted"));
    auto& locked = make<EncoderBlock> ("locked", tr ("practice.tempo"), 50.0, 150.0, 100.0, 1.0, pct, "%");
    locked.setLocked (true);
    locked.setCaption (tr ("record.lockNote"));
    make<Encoder> ("hover", 0.0, 1.0, 0.25, 0.01).setPreviewHover (true);

    // --- Faders -------------------------------------------------------------
    make<ChannelStrip> ("", tr ("monitor.backing"), 0.51, 0.62f);
    make<ChannelStrip> ("", tr ("monitor.refMain"), 0.72, 0.48f, colours::ref);
    make<ChannelStrip> ("", tr ("monitor.refHarm"), 0.40, 0.30f, colours::ref);
    auto& me = make<ChannelStrip> ("", tr ("monitor.self"), 0.64, 0.80f);
    me.soloKey().setToggleState (true, juce::dontSendNotification);
    make<ChannelStrip> ("", tr ("monitor.reverb"), 0.25, -1.0f, colours::textDim, false, tr ("monitor.reverb.note"));

    // --- Meters -------------------------------------------------------------
    make<LedMeter> ("full  -12 / -18.4 / hold -9.6").setLevels (-12.0f, -18.4f, -9.6f, false);
    make<LedMeter> ("full  hot  -4 / -7", LedMeter::Style::full).setLevels (-4.0f, -7.0f, -2.5f, false);
    make<LedMeter> ("full  clip", LedMeter::Style::full).setLevels (0.0f, -3.5f, 0.0f, true);
    make<LedMeter> ("compact", LedMeter::Style::compact).setLevels (-12.0f, -18.4f, -9.6f, false);

    // --- Readouts -----------------------------------------------------------
    make<Readout> ("", tr ("transport.time")).setValue ("0:39.000", "/ 2:16");
    make<Readout> ("", tr ("transport.barBeat")).setValue ("20.3");
    make<Readout> ("", "TEMPO").setValue ("100%", "120 BPM");
    make<Readout> ("", "KEY").setValue ("0");
    make<TallyLamp> ("standby").setState (TallyLamp::State::standby);
    make<TallyLamp> ("play").setState (TallyLamp::State::play);
    make<TallyLamp> ("rec").setState (TallyLamp::State::rec);

    // --- Dropdown（items の最後に置き、SWITCH の行に並べる） --------------------
    {
        juce::StringArray langs;
        for (auto& l : i18n::available())
            langs.add (juce::String::fromUTF8 (l.nativeName));
        make<Dropdown> ("dropdown", langs, 0);
    }

    setSize (1440, 900);
}

void Gallery::resized()
{
    auto r = getLocalBounds().reduced (24, 20);
    r.removeFromTop (34);

    auto left = r.removeFromLeft (r.getWidth() * 62 / 100);
    r.removeFromLeft (24);
    auto right = r;

    keysArea    = left.removeFromTop (sectionTitleH + 3 * (32 + captionH + 10));
    left.removeFromTop (8);
    segArea     = left.removeFromTop (sectionTitleH + 34 + captionH + 6);
    left.removeFromTop (8);
    encArea     = left.removeFromTop (sectionTitleH + 150 + captionH);
    left.removeFromTop (8);
    meterArea   = left.removeFromTop (sectionTitleH + 2 * (36 + captionH + 6));
    left.removeFromTop (8);
    readoutArea = left;

    faderArea = right.removeFromTop (sectionTitleH + 260);
    right.removeFromTop (8);
    iconArea  = right.removeFromTop (sectionTitleH + 156);
    right.removeFromTop (8);
    tokenArea = right.removeFromTop (sectionTitleH + 150);
    right.removeFromTop (8);
    typeArea  = right;

    // items の並び（コンストラクタで作った順）
    constexpr size_t keysAt = 0, segAt = 16, encAt = 19, faderAt = 25, meterAt = 30, readoutAt = 34, lampAt = 38, dropAt = 41;

    auto place = [this] (size_t index, juce::Rectangle<int> cell)
    {
        items[index].comp->setBounds (cell.withTrimmedBottom (captionH));
    };

    // Keys: 3 行（4 / 7 / 5 個）
    {
        auto a = keysArea.withTrimmedTop (sectionTitleH);
        const int counts[] = { 4, 7, 5 };
        size_t i = keysAt;
        for (int row = 0; row < 3; ++row)
        {
            auto line = a.removeFromTop (32 + captionH + 10).withTrimmedBottom (10);
            for (int k = 0; k < counts[row]; ++k, ++i)
            {
                auto* kb = dynamic_cast<KeyButton*> (items[i].comp);
                kb->setSize (10, 32);
                const auto w = juce::jmax (kb->getButtonText().length() <= 1 ? 32 : 70, kb->idealWidth());
                place (i, line.removeFromLeft (w));
                line.removeFromLeft (16);
            }
        }
    }

    // Segmented
    {
        auto a = segArea.withTrimmedTop (sectionTitleH);
        for (size_t k = 0; k < 3; ++k)
        {
            auto* sk = dynamic_cast<SegmentedKeys*> (items[segAt + k].comp);
            place (segAt + k, a.removeFromLeft (sk->idealWidth()).withHeight (34 + captionH));
            a.removeFromLeft (24);
        }
        auto* dd = dynamic_cast<Dropdown*> (items[dropAt].comp);
        place (dropAt, a.removeFromLeft (juce::jmax (160, dd->idealWidth())).withHeight (34 + captionH));
    }

    // Encoders
    {
        auto a = encArea.withTrimmedTop (sectionTitleH);
        for (size_t k = 0; k < 6; ++k)
        {
            const auto cell = a.removeFromLeft (k == 5 ? 90 : 128);
            place (encAt + k, k == 5 ? cell.withHeight (90 + captionH) : cell.withHeight (150 + captionH));
            a.removeFromLeft (8);
        }
    }

    // Faders
    {
        auto a = faderArea.withTrimmedTop (sectionTitleH);
        for (size_t k = 0; k < 5; ++k)
        {
            place (faderAt + k, a.removeFromLeft (64));
            a.removeFromLeft (12);
        }
    }

    // Meters（2x2）
    {
        auto a = meterArea.withTrimmedTop (sectionTitleH);
        size_t i = meterAt;
        for (int row = 0; row < 2; ++row)
        {
            auto line = a.removeFromTop (36 + captionH + 6).withTrimmedBottom (6);
            auto l = line.removeFromLeft (line.getWidth() / 2).withTrimmedRight (24);
            place (i, l.withHeight (36 + captionH)); ++i;
            place (i, line.withHeight ((row == 1 ? 10 : 36) + captionH)); ++i;
        }
    }

    // Readouts + Tally
    {
        auto a = readoutArea.withTrimmedTop (sectionTitleH);
        auto line = a.removeFromTop (46);
        for (size_t k = 0; k < 4; ++k)
        {
            auto* ro = dynamic_cast<Readout*> (items[readoutAt + k].comp);
            ro->setBounds (line.removeFromLeft (juce::jmax (90, ro->idealWidth())));
            line.removeFromLeft (10);
        }
        a.removeFromTop (16);
        auto lamps = a.removeFromTop (28 + captionH);
        for (size_t k = 0; k < 3; ++k)
        {
            place (lampAt + k, lamps.removeFromLeft (TallyLamp::idealWidth()));
            lamps.removeFromLeft (16);
        }
    }
}

void Gallery::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);

    g.setColour (colours::text);
    g.setFont (sans (18.0f, Weight::semibold));
    g.drawText (tr ("gallery.title"), getLocalBounds().reduced (24, 18).removeFromTop (28), juce::Justification::centredLeft, false);

    auto title = [&] (juce::Rectangle<int> a, const juce::String& en, const juce::String& ja)
    {
        paint::sectionHeader (g, a.removeFromTop (18), en, ja);
        paint::hline (g, (float) a.getY() + 1.0f, (float) a.getX(), (float) a.getRight());
    };

    title (keysArea, "KEYS", tr ("gallery.keys"));
    title (segArea, "SWITCH", tr ("gallery.switch"));
    title (encArea, "ENCODER", tr ("gallery.encoder"));
    title (meterArea, "METER", tr ("gallery.meter"));
    title (readoutArea, "READOUT", tr ("gallery.readout"));
    title (faderArea, "FADER", tr ("gallery.fader"));
    title (iconArea, "ICONS", tr ("gallery.icons"));
    title (tokenArea, "TOKENS", tr ("gallery.tokens"));
    title (typeArea, "TYPE", tr ("gallery.type"));

    // キャプション
    g.setFont (mono (9.5f));
    for (auto& it : items)
    {
        if (it.caption.isEmpty()) continue;
        g.setColour (colours::textMute);
        g.drawText (it.caption, it.comp->getBounds().translated (0, it.comp->getHeight()).withHeight (captionH).expanded (20, 0),
                    juce::Justification::centredTop, false);
    }

    // アイコン一覧
    {
        auto a = iconArea.withTrimmedTop (sectionTitleH);
        const Icon all[] = { Icon::play, Icon::pause, Icon::stop, Icon::toStart, Icon::rec, Icon::loop, Icon::rangeIn,
                             Icon::rangeOut, Icon::close, Icon::metronome, Icon::gear, Icon::mic, Icon::headphones,
                             Icon::edit, Icon::compare, Icon::lock, Icon::chevronDown, Icon::chevronRight, Icon::minus, Icon::plus,
                             Icon::exportFile, Icon::folder, Icon::note, Icon::check, Icon::warning, Icon::globe,
                             Icon::download, Icon::shield };
        int col = 0;
        auto row = a.removeFromTop (40);
        for (auto ic : all)
        {
            if (col == 11) { row = a.removeFromTop (40); col = 0; }
            auto cell = row.removeFromLeft (44).toFloat();
            paint::inset (g, cell.reduced (3.0f));
            drawIcon (g, ic, cell.withSizeKeepingCentre (20.0f, 20.0f), colours::text);
            ++col;
        }
        auto sizes = a.removeFromTop (34);
        for (auto s : { 12.0f, 16.0f, 20.0f, 24.0f, 32.0f })
        {
            auto cell = sizes.removeFromLeft ((int) s + 16).toFloat();
            drawIcon (g, Icon::gear, cell.withSizeKeepingCentre (s, s), colours::textDim);
        }
    }

    // 色トークン
    {
        auto a = tokenArea.withTrimmedTop (sectionTitleH);
        struct Tok { const char* name; juce::Colour c; };
        const Tok toks[] = {
            { "bg0", colours::bg0 }, { "bg-deep", colours::bgDeep }, { "panel", colours::panel }, { "raised", colours::raised },
            { "line", colours::line }, { "text", colours::text }, { "text-dim", colours::textDim }, { "text-mute", colours::textMute },
            { "signal", colours::signal }, { "ref", colours::ref }, { "warn", colours::warn }, { "bad", colours::bad }, { "rec", colours::rec },
        };
        int col = 0;
        auto row = a.removeFromTop (60);
        for (auto& t : toks)
        {
            if (col == 7) { row = a.removeFromTop (60); col = 0; }
            auto cell = row.removeFromLeft (row.getWidth() / (7 - col)).reduced (3, 0);
            auto sw = cell.removeFromTop (30).toFloat();
            g.setColour (t.c);
            g.fillRoundedRectangle (sw, 3.0f);
            g.setColour (colours::line);
            g.drawRoundedRectangle (sw.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (colours::textDim);
            g.setFont (mono (9.5f));
            g.drawText (t.name, cell.removeFromTop (13), juce::Justification::centredLeft, false);
            g.setColour (colours::textMute);
            g.drawText ("#" + t.c.toDisplayString (false), cell.removeFromTop (13), juce::Justification::centredLeft, false);
            ++col;
        }
    }

    // 書体
    {
        auto a = typeArea.withTrimmedTop (sectionTitleH);
        auto line = [&] (const juce::Font& f, const juce::String& s, juce::Colour c)
        {
            g.setColour (c);
            g.setFont (f);
            g.drawText (s, a.removeFromTop ((int) f.getHeight() + 10), juce::Justification::centredLeft, true);
        };
        line (sans (22.0f, Weight::semibold), tr ("gallery.type.sample1"), colours::text);
        line (sans (14.0f, Weight::medium), "IBM Plex Sans JP Medium  " + tr ("gallery.type.sample2"), colours::text);
        line (sans (12.0f), "IBM Plex Sans JP Regular  " + tr ("gallery.type.sample3"), colours::textDim);
        line (mono (24.0f, Weight::semibold), "0:39.000  20.3  -12.0 dBFS", colours::text);
        line (mono (10.5f, Weight::semibold, 0.14f), "INPUT  PRACTICE  MONITOR  RECORD", colours::textDim);
    }
}
} // namespace vb
