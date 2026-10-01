#include "Rack.h"

namespace vb
{
void RackModule::paint (juce::Graphics& g)
{
    paint::sectionHeader (g, getLocalBounds().reduced (metrics::pad, 0).withHeight (headerH).withTrimmedTop (4), en, ja);
}

//==============================================================================
InputModule::InputModule (const dummy::Session& s)
    : RackModule ("INPUT", jp ("入力")), session (s)
{
    meter.setLevels (s.inputPeakDb, s.inputRmsDb, s.inputPeakHoldDb, false);
    addAndMakeVisible (meter);
}

void InputModule::resized()
{
    auto r = content();
    deviceArea = r.removeFromTop (20);
    r.removeFromTop (6);
    meter.setBounds (r.removeFromTop (38));
    r.removeFromTop (4);
    readoutArea = r;
}

void InputModule::paint (juce::Graphics& g)
{
    RackModule::paint (g);

    // デバイス
    {
        auto r = deviceArea.toFloat();
        drawIcon (g, Icon::mic, r.removeFromLeft (16.0f).withSizeKeepingCentre (14.0f, 14.0f), colours::textDim);
        r.removeFromLeft (6.0f);
        const auto drv = session.driver + " " + juce::String (session.bufferSize);
        const auto df = mono (10.0f);
        const auto dw = textWidth (df, drv);
        g.setColour (colours::textMute);
        g.setFont (df);
        g.drawText (drv, r.removeFromRight (dw + 2.0f), juce::Justification::centredRight, false);
        g.setColour (colours::text.withAlpha (0.9f));
        g.setFont (sans (12.0f));
        g.drawText (session.inputDevice, r, juce::Justification::centredLeft, true);
    }

    // 数値：PEAK / RMS / 判定 / レイテンシ
    {
        auto r = readoutArea;
        auto row = r.removeFromTop (22).toFloat();

        auto value = [&] (const juce::String& label, const juce::String& v)
        {
            paint::microLabel (g, row.removeFromLeft (textWidth (mono (9.5f, Weight::medium, 0.12f), label) + 6.0f), label, colours::textMute);
            g.setColour (colours::text);
            g.setFont (mono (13.0f, Weight::semibold));
            const auto w = textWidth (mono (13.0f, Weight::semibold), v);
            g.drawText (v, row.removeFromLeft (w + 2.0f), juce::Justification::centredLeft, false);
            row.removeFromLeft (14.0f);
        };
        value ("PEAK", juce::String (session.inputPeakDb, 1));
        value ("RMS", juce::String (session.inputRmsDb, 1));

        const bool inTarget = session.inputPeakDb >= LedMeter::targetLow && session.inputPeakDb <= LedMeter::targetHigh;
        const auto chip = row.removeFromRight (56.0f).withSizeKeepingCentre (56.0f, 20.0f);
        const auto c = inTarget ? colours::signal : colours::warn;
        g.setColour (c.withAlpha (0.14f));
        g.fillRoundedRectangle (chip, 3.0f);
        paint::led (g, { chip.getX() + 9.0f, chip.getCentreY() }, 2.4f, c, true);
        g.setColour (c);
        g.setFont (sans (11.0f, Weight::medium));
        g.drawText (inTarget ? jp ("適正") : jp ("調整"), chip.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);

        r.removeFromTop (4);
        const auto ms = (double) session.latencySamples * 1000.0 / session.sampleRate();
        auto lat = r.removeFromTop (16).toFloat();
        paint::microLabel (g, lat.removeFromLeft (62.0f), "LATENCY", colours::textMute);
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        g.drawText (juce::String (ms, 1) + " ms  /  " + juce::String (session.latencySamples) + " smp", lat, juce::Justification::centredLeft, false);
    }
}

//==============================================================================
PracticeModule::PracticeModule (const dummy::Session& s)
    : RackModule ("PRACTICE", jp ("練習（耳とお手本だけ変わる）")),
      tempo (jp ("テンポ"), 50.0, 150.0, s.tempoPercent, 1.0,
             [] (double v) { return juce::String (juce::roundToInt (v)); }, "%"),
      key (jp ("キー"), -6.0, 6.0, s.keyShift, 1.0,
           [] (double v) { const auto k = juce::roundToInt (v); return (k > 0 ? "+" : "") + juce::String (k); },
           {}, true, colours::ref)
{
    tempo.setCaption (jp ("原速 ") + juce::String (juce::roundToInt (s.bpm())) + " BPM");
    key.setCaption (jp ("原キー"));

    const bool lock = s.isRecording && s.recMode == project::RecMode::delivery;
    tempo.setLocked (lock);
    key.setLocked (lock);

    addAndMakeVisible (tempo);
    addAndMakeVisible (key);
}

void PracticeModule::resized()
{
    auto r = content();
    tempo.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (4, 0));
    key.setBounds (r.reduced (4, 0));
}

//==============================================================================
MonitorModule::MonitorModule (const dummy::Session& s)
    : RackModule ("MONITOR", jp ("モニター（録音には入らない）"))
{
    strips.add (new ChannelStrip (jp ("オフボ"), s.offVocalGain, 0.62f));
    strips.add (new ChannelStrip (jp ("お手本\nMain"), s.mainGain, 0.48f, colours::ref));
    strips.add (new ChannelStrip (jp ("お手本\nHarm"), s.harmonyGain, 0.22f, colours::ref));
    strips.add (new ChannelStrip (jp ("自分"), s.monitorGain, 0.70f));
    strips.add (new ChannelStrip (jp ("リバーブ"), s.monitorReverb, -1.0f, colours::textDim, false, jp ("耳のみ")));

    for (auto* st : strips)
        addAndMakeVisible (st);
}

void MonitorModule::resized()
{
    auto r = content();
    const auto w = r.getWidth() / juce::jmax (1, strips.size());
    for (auto* st : strips)
        st->setBounds (r.removeFromLeft (w).reduced (3, 0));
}

//==============================================================================
RecordModule::RecordModule (const dummy::Session& s)
    : RackModule ("RECORD", jp ("録音")), session (s),
      recMode ({ jp ("納品"), jp ("練習") }, (int) s.recMode, colours::rec)
{
    addAndMakeVisible (recMode);
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

    const bool delivery = session.recMode == project::RecMode::delivery;
    const bool rec = session.isRecording;

    auto t = targetArea.toFloat();
    paint::inset (g, t);
    auto inner = t.reduced (10.0f, 6.0f);
    auto top = inner.removeFromTop (14.0f);
    paint::led (g, { top.getX() + 3.0f, top.getCentreY() }, 2.6f, colours::rec, rec);
    paint::microLabel (g, top.withTrimmedLeft (11.0f), rec ? "RECORDING TO" : "TARGET", rec ? colours::rec : colours::textMute);

    g.setColour (colours::text);
    g.setFont (sans (13.5f, Weight::semibold));
    g.drawText (delivery ? jp ("Dry Vocal ／ 納品") : jp ("練習テイク（納品に入らない）"), inner.removeFromTop (18.0f), juce::Justification::centredLeft, true);

    g.setColour (colours::textDim);
    g.setFont (mono (10.5f));
    g.drawText (juce::String (session.sampleRate() / 1000) + "kHz  " + juce::String (session.project.bitDepthExport) + "bit  MONO  0:00-"
                    + formatTime (session.project.lengthSamples, session.sampleRate(), false),
                inner, juce::Justification::centredLeft, true);

    auto l = lockArea.toFloat();
    drawIcon (g, Icon::lock, l.removeFromLeft (14.0f).withSizeKeepingCentre (11.0f, 11.0f), colours::textMute);
    l.removeFromLeft (6.0f);
    g.setColour (colours::textMute);
    g.setFont (sans (10.5f));
    g.drawText (jp ("納品REC中はテンポ100%・キー0に固定"), l, juce::Justification::centredLeft, true);
}

//==============================================================================
Rack::Rack (const dummy::Session& s)
    : input (s), practice (s), monitor (s), record (s)
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
    paint::vline (g, 0.0f, 0.0f, (float) getHeight());

    for (auto* m : std::initializer_list<juce::Component*> { &practice, &monitor, &record })
        paint::hline (g, (float) m->getY(), 1.0f, (float) getWidth());
}
} // namespace vb
