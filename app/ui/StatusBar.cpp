#include "StatusBar.h"

namespace vb
{
namespace
{
    constexpr int labelW = 42;
}

StatusBar::StatusBar (const dummy::Session& s) : session (s)
{
    mini.setLevels (s.inputPeakDb, s.inputRmsDb, s.inputPeakHoldDb, false);
    addAndMakeVisible (mini);
}

void StatusBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);
    r.removeFromLeft (labelW);
    meterArea = r.removeFromLeft (120);
    mini.setBounds (meterArea.withSizeKeepingCentre (meterArea.getWidth(), 8));
}

void StatusBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgDeep);
    paint::hline (g, 0.0f, 0.0f, (float) getWidth());

    auto r = getLocalBounds().reduced (metrics::pad, 0).toFloat();
    paint::microLabel (g, r.removeFromLeft ((float) labelW), "INPUT", colours::textMute);
    r.removeFromLeft ((float) meterArea.getWidth() + 10.0f);

    const auto vf = mono (11.0f, Weight::medium);
    auto item = [&] (const juce::String& label, const juce::String& value, juce::Colour vc)
    {
        if (label.isNotEmpty())
        {
            const auto lw = textWidth (mono (9.5f, Weight::medium, 0.12f), label) + 8.0f;
            paint::microLabel (g, r.removeFromLeft (lw), label, colours::textMute);
        }
        g.setColour (vc);
        g.setFont (vf);
        g.drawText (value, r.removeFromLeft (textWidth (vf, value) + 4.0f), juce::Justification::centredLeft, false);
        r.removeFromLeft (12.0f);
        paint::vline (g, r.getX(), r.getY() + 8.0f, r.getBottom() - 8.0f);
        r.removeFromLeft (13.0f);
    };

    const auto latencyMs = (double) session.latencySamples * 1000.0 / session.sampleRate();

    item ({}, juce::String (session.inputPeakDb, 1) + " dBFS", colours::text);
    item ("LATENCY", juce::String (latencyMs, 1) + " ms", colours::text);
    item ("REC TO", session.recMode == project::RecMode::delivery ? "DRY / DELIVERY" : "PRACTICE",
          session.isRecording ? colours::rec : colours::text);
    item ("EXPORT", juce::String (session.sampleRate() / 1000) + "kHz 24bit MONO", colours::text);
    item ("DRIVER", session.driver + " " + juce::String (session.bufferSize), colours::textDim);

    // Phase A であることを明示（音は出ない）
    const auto mf = mono (10.0f, Weight::semibold, 0.1f);
    const auto txt = juce::String ("UI MOCK");
    const auto w = textWidth (mf, txt) + 22.0f;
    const auto chip = r.removeFromRight (w).withSizeKeepingCentre (w, 18.0f);
    g.setColour (colours::warn.withAlpha (0.14f));
    g.fillRoundedRectangle (chip, 3.0f);
    paint::led (g, { chip.getX() + 8.0f, chip.getCentreY() }, 2.3f, colours::warn, true);
    g.setColour (colours::warn);
    g.setFont (mf);
    g.drawText (txt, chip.withTrimmedLeft (15.0f), juce::Justification::centredLeft, false);

    g.setColour (colours::textMute);
    g.setFont (sans (10.5f));
    g.drawText (jp ("音声デバイス未接続"), r.withTrimmedRight (8.0f), juce::Justification::centredRight, false);
}
} // namespace vb
