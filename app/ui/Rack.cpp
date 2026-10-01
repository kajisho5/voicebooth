#include "Rack.h"

namespace vb
{
void RackModule::paint (juce::Graphics& g)
{
    paint::sectionHeader (g, getLocalBounds().reduced (metrics::pad, 0).withHeight (headerH).withTrimmedTop (4), en, ja);
}

//==============================================================================
InputModule::InputModule (UiSession& u, Actions& a)
    : RackModule (tr ("rack.input"), tr ("rack.input.sub")), SessionView (u), actions (a)
{
    meter.setLevels (u->inputPeakDb, u->inputRmsDb, u->inputPeakHoldDb, u->inputClipped);
    meter.onClick = [this] { session.resetInputClip(); };
    meter.setTooltip (tr ("meter.clip.tooltip"));
    addAndMakeVisible (meter);

    buffer.setButtonText (juce::String (u->bufferSize) + " smp");
    buffer.withIcon (Icon::chevronDown).withFont (mono (10.5f));
    buffer.setTooltip (tr ("rack.input.buffer.tooltip"));
    buffer.onClick = [this] { if (actions.openSettings) actions.openSettings(); };
    addChildComponent (buffer);
}

void InputModule::onSessionChanged (juce::uint32 c)
{
    const auto& s = state();
    if (c & (change::meter | change::device))
    {
        meter.setLevels (s.inputPeakDb, s.inputRmsDb, s.inputPeakHoldDb, s.inputClipped);
        repaint (readoutArea);
    }
    if (c & change::device)
    {
        buffer.setButtonText (juce::String (s.bufferSize) + " smp");
        repaint();
    }
    if (c & change::mode)
    {
        resized();
        repaint();
    }
}

void InputModule::resized()
{
    auto r = content();
    deviceArea = r.removeFromTop (20);

    // バッファ：プロは編集できるキー、標準は数値表示、簡単は「自動」（DESIGN 2）
    buffer.setVisible (state().mode == project::Mode::pro);
    if (buffer.isVisible())
    {
        buffer.setSize (10, 22);
        const auto w = buffer.idealWidth();
        buffer.setBounds (deviceArea.withLeft (deviceArea.getRight() - w).withSizeKeepingCentre (w, 22));
    }

    r.removeFromTop (6);
    meter.setBounds (r.removeFromTop (38));
    r.removeFromTop (4);
    readoutArea = r;
}

void InputModule::paint (juce::Graphics& g)
{
    RackModule::paint (g);
    const auto& s = state();

    // デバイス
    {
        auto r = deviceArea.toFloat();
        drawIcon (g, Icon::mic, r.removeFromLeft (16.0f).withSizeKeepingCentre (14.0f, 14.0f), colours::textDim);
        r.removeFromLeft (6.0f);

        if (buffer.isVisible())
            r.removeFromRight ((float) buffer.getWidth() + 6.0f);
        else
        {
            const auto drv = s.mode == project::Mode::easy ? tr ("rack.input.auto")
                                                           : s.driver + " " + juce::String (s.bufferSize);
            const auto df = mono (10.0f);
            g.setColour (colours::textMute);
            g.setFont (df);
            g.drawText (drv, r.removeFromRight (textWidth (df, drv) + 2.0f), juce::Justification::centredRight, false);
        }

        // 入力が無い時は警告色（DESIGN 4.1）。機器名はデータなので翻訳しない
        const bool missing = s.engineAttached && ! s.input.open;
        g.setColour (missing ? colours::warn : colours::text.withAlpha (0.9f));
        g.setFont (sansFor (inputDisplayName (s), 12.0f));
        g.drawText (inputDisplayName (s), r, juce::Justification::centredLeft, true);
    }

    // 数値：PEAK / RMS / 判定 / レイテンシ
    {
        auto r = readoutArea;
        auto row = r.removeFromTop (22).toFloat();
        const auto lf = mono (9.5f, Weight::medium, 0.12f);
        const auto vf = mono (13.0f, Weight::semibold);

        auto value = [&] (const juce::String& label, const juce::String& v)
        {
            paint::microLabel (g, row.removeFromLeft (textWidth (lf, label) + 6.0f), label, colours::textMute);
            g.setColour (colours::text);
            g.setFont (vf);
            g.drawText (v, row.removeFromLeft (textWidth (vf, v) + 2.0f), juce::Justification::centredLeft, false);
            row.removeFromLeft (14.0f);
        };
        value (tr ("meter.peak"), formatDb (s.inputPeakDb));
        value (tr ("meter.rms"), formatDb (s.inputRmsDb));

        // 適正 / 調整の札（入力が無い時は出さない）
        if (! s.engineAttached || s.input.open)
        {
            const bool inTarget = s.inputPeakDb >= LedMeter::targetLow && s.inputPeakDb <= LedMeter::targetHigh;
            const auto text = inTarget ? tr ("meter.ok") : tr ("meter.adjust");
            const auto cf = sans (11.0f, Weight::medium);
            const auto cw = textWidth (cf, text) + 26.0f;
            const auto chip = row.removeFromRight (cw).withSizeKeepingCentre (cw, 20.0f);
            const auto c = inTarget ? colours::signal : colours::warn;
            g.setColour (c.withAlpha (0.14f));
            g.fillRoundedRectangle (chip, 3.0f);
            paint::led (g, { chip.getX() + 9.0f, chip.getCentreY() }, 2.4f, c, true);
            g.setColour (c);
            g.setFont (cf);
            g.drawText (text, chip.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);
        }

        // レイテンシ：実デバイスは申告値（実測は B6）と明記。UI_MOCK はダミー
        r.removeFromTop (4);
        const auto ld = latencyDisplay (s);
        auto lat = r.removeFromTop (16).toFloat();
        paint::microLabel (g, lat.removeFromLeft (textWidth (lf, tr ("meter.latency")) + 10.0f), tr ("meter.latency"), colours::textMute);
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        const auto text = ! ld.known  ? juce::String ("-")
                        : ld.reported ? tr ("meter.latency.reported", juce::String (ld.ms, 1), ld.samples)
                                      : tr ("meter.latency.value", juce::String (ld.ms, 1), ld.samples);
        g.drawText (text, lat, juce::Justification::centredLeft, true);
    }
}

//==============================================================================
PracticeModule::PracticeModule (UiSession& u, Actions& a)
    : RackModule (tr ("rack.practice"), tr ("rack.practice.sub")), SessionView (u), actions (a),
      tempo (tr ("practice.tempo"), 50.0, 150.0, u->tempoPercent, 1.0,
             [] (double v) { return juce::String (juce::roundToInt (v)); }, "%"),
      key (tr ("practice.key"), -6.0, 6.0, u->keyShift, 1.0,
           [] (double v) { const auto k = juce::roundToInt (v); return (k > 0 ? "+" : "") + juce::String (k); },
           {}, true, colours::ref)
{
    tempo.encoder().setDefaultValue (100.0);   // 原速で吸い付く・ダブルクリックで原速（DESIGN 4.10）
    key.encoder().setDefaultValue (0.0);
    tempo.onChange = [this] (double v) { if (! syncing && actions.requestTempo) actions.requestTempo (juce::roundToInt (v)); };
    key.onChange   = [this] (double v) { if (! syncing && actions.requestKey) actions.requestKey (juce::roundToInt (v)); };

    addAndMakeVisible (tempo);
    addAndMakeVisible (key);
    onSessionChanged (change::all);
}

void PracticeModule::onSessionChanged (juce::uint32 changes)
{
    if ((changes & (change::practice | change::transport | change::songInfo)) == 0)
        return;

    const auto& s = state();
    const juce::ScopedValueSetter<bool> guard (syncing, true);
    tempo.encoder().setValue (s.tempoPercent, juce::dontSendNotification);
    key.encoder().setValue (s.keyShift, juce::dontSendNotification);

    if (! s.tempoKnown())
        tempo.setCaption (tr ("practice.tempo.unknown"));   // テンポが分からない間は BPM を出さない
    else
        tempo.setCaption (s.tempoPercent == 100 ? tr ("practice.tempo.original", song::formatBpm (s.bpm()))
                                                : tr ("practice.tempo.bpm", song::formatBpm (s.bpm() * s.tempoPercent / 100.0)));
    key.setCaption (s.keyShift == 0 ? tr ("practice.key.original") : tr ("practice.key.shifted"));

    const bool lock = session.deliveryLocked();
    tempo.setLocked (lock);
    key.setLocked (lock);
    repaint();
}

void PracticeModule::resized()
{
    auto r = content();
    tempo.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (4, 0));
    key.setBounds (r.reduced (4, 0));
}

//==============================================================================
MonitorModule::MonitorModule (UiSession& u)
    : RackModule (tr ("rack.monitor"), tr ("rack.monitor.sub")), SessionView (u)
{
    backingStrip = strips.add (new ChannelStrip (tr ("monitor.backing"), u->offVocalGain, 0.62f));
    strips.add (new ChannelStrip (tr ("monitor.refMain"), u->mainGain, 0.48f, colours::ref));
    harmStrip = strips.add (new ChannelStrip (tr ("monitor.refHarm"), u->harmonyGain, 0.22f, colours::ref));
    strips.add (new ChannelStrip (tr ("monitor.self"), u->monitorGain, 0.70f));
    strips.add (new ChannelStrip (tr ("monitor.reverb"), u->monitorReverb, -1.0f, colours::textDim, false, tr ("monitor.reverb.note")));

    for (auto* st : strips)
        addAndMakeVisible (st);

    // オフボのフェーダーとミュートは再生の音量に効く（B2）。ほかは B4 / B12 まで見た目だけ
    backingStrip->fader().onValueChange = [this] { session.setBackingLevel ((float) backingStrip->fader().getValue()); };
    backingStrip->muteKey().onClick = [this] { session.setBackingMuted (backingStrip->muteKey().getToggleState()); };
    onSessionChanged (change::monitor);
}

void MonitorModule::onSessionChanged (juce::uint32 c)
{
    if (c & change::mode)
        resized();

    if (c & change::monitor)
    {
        backingStrip->fader().setValue (state().offVocalGain, juce::dontSendNotification);
        backingStrip->muteKey().setToggleState (state().backingMuted, juce::dontSendNotification);
    }
}

void MonitorModule::resized()
{
    // 簡単モードはハモリのお手本を出さない（DESIGN 2）
    harmStrip->setVisible (state().mode != project::Mode::easy);

    int visible = 0;
    for (auto* st : strips) visible += st->isVisible() ? 1 : 0;

    auto r = content();
    const auto w = r.getWidth() / juce::jmax (1, visible);
    for (auto* st : strips)
        if (st->isVisible())
            st->setBounds (r.removeFromLeft (w).reduced (3, 0));
}

//==============================================================================
RecordModule::RecordModule (UiSession& u)
    : RackModule (tr ("rack.record"), tr ("rack.record.sub")), SessionView (u),
      recMode ({ tr ("record.delivery"), tr ("record.practice") }, (int) u->recMode, colours::rec)
{
    recMode.onChange = [this] (int i) { session.setRecMode ((project::RecMode) i); };
    addAndMakeVisible (recMode);
}

void RecordModule::onSessionChanged (juce::uint32 changes)
{
    if (changes & (change::practice | change::transport | change::mode))
    {
        recMode.setSelected ((int) state().recMode, juce::dontSendNotification);
        repaint();
    }
}

void RecordModule::resized()
{
    auto r = content();
    recMode.setBounds (r.removeFromTop (34));
    r.removeFromTop (10);
    targetArea = r.removeFromTop (52);
    r.removeFromTop (8);
    lockArea = r.removeFromTop (16);
}

void RecordModule::paint (juce::Graphics& g)
{
    RackModule::paint (g);
    const auto& s = state();
    const bool delivery = s.recMode == project::RecMode::delivery;
    const bool rec = s.isRecording;

    auto t = targetArea.toFloat();
    paint::inset (g, t);
    if (rec)
    {
        g.setColour (colours::rec.withAlpha (0.08f));
        g.fillRoundedRectangle (t.reduced (1.0f), metrics::windowRadius);
    }

    auto inner = t.reduced (10.0f, 6.0f);
    auto top = inner.removeFromTop (14.0f);
    paint::led (g, { top.getX() + 3.0f, top.getCentreY() }, 2.6f, colours::rec, rec);
    paint::microLabel (g, top.withTrimmedLeft (11.0f), rec ? tr ("record.recordingTo") : tr ("record.target"),
                       rec ? colours::rec : colours::textMute);

    g.setColour (colours::text);
    g.setFont (sans (13.5f, Weight::semibold));
    const auto target = ! delivery ? tr ("record.target.practice")
                      : (s.mode == project::Mode::easy ? tr ("record.target.deliveryWhole") : tr ("record.target.delivery"));
    g.drawText (target, inner.removeFromTop (18.0f), juce::Justification::centredLeft, true);

    g.setColour (colours::textDim);
    g.setFont (mono (10.5f));
    g.drawText (tr ("record.format", formatKhz (s.sampleRate()), s.project.bitDepthExport,
                    formatTime (s.project.lengthSamples, s.sampleRate(), false)),
                inner, juce::Justification::centredLeft, true);

    auto l = lockArea.toFloat();
    const auto lc = session.deliveryLocked() ? colours::warn : colours::textMute;
    drawIcon (g, Icon::lock, l.removeFromLeft (14.0f).withSizeKeepingCentre (11.0f, 11.0f), lc);
    l.removeFromLeft (6.0f);
    g.setColour (lc);
    g.setFont (sans (10.5f));
    g.drawText (tr ("record.lockNote"), l, juce::Justification::centredLeft, true);
}

//==============================================================================
Rack::Rack (UiSession& u, Actions& a)
    : SessionView (u), input (u, a), practice (u, a), monitor (u), record (u)
{
    addAndMakeVisible (input);
    addAndMakeVisible (practice);
    addAndMakeVisible (monitor);
    addAndMakeVisible (record);
}

void Rack::resized()
{
    // 既定の高さ（合計 756）と最小の高さ。足りない分は (既定 - 最小) の比で各段から削る
    constexpr int desired[] = { 156, 186, 246, 168 };
    constexpr int minimum[] = { 140, 164, 186, 146 };
    constexpr int desiredSum = 156 + 186 + 246 + 168, slackSum = 16 + 22 + 60 + 22;

    auto r = getLocalBounds().withTrimmedLeft (1);
    const auto deficit = juce::jlimit (0, slackSum, desiredSum - r.getHeight());
    int h[4];
    for (int i = 0; i < 4; ++i)
        h[i] = desired[i] - deficit * (desired[i] - minimum[i]) / slackSum;

    input.setBounds (r.removeFromTop (h[0]));
    practice.setBounds (r.removeFromTop (h[1]));
    record.setBounds (r.removeFromBottom (h[3]));
    monitor.setBounds (r);   // 余りはすべてモニター
}

void Rack::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    paint::vline (g, 0.0f, 0.0f, (float) getHeight(), state().isRecording ? colours::rec : colours::line);

    for (auto* m : std::initializer_list<juce::Component*> { &practice, &monitor, &record })
        paint::hline (g, (float) m->getY(), 1.0f, (float) getWidth());
}
} // namespace vb
