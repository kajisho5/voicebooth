#include "StatusBar.h"

namespace vb
{
StatusBar::StatusBar (UiSession& u) : SessionView (u)
{
    mini.setLevels (u->inputPeakDb, u->inputRmsDb, u->inputPeakHoldDb, false);
    addAndMakeVisible (mini);
}

void StatusBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);
    r.removeFromLeft ((int) textWidth (mono (9.5f, Weight::medium, 0.12f), tr ("label.input")) + 10);
    meterArea = r.removeFromLeft (120);
    mini.setBounds (meterArea.withSizeKeepingCentre (meterArea.getWidth(), 8));
}

void StatusBar::paint (juce::Graphics& g)
{
    const auto& s = state();
    g.fillAll (colours::bgDeep);
    paint::hline (g, 0.0f, 0.0f, (float) getWidth());

    auto r = getLocalBounds().reduced (metrics::pad, 0).toFloat();
    const auto lf = mono (9.5f, Weight::medium, 0.12f);
    paint::microLabel (g, r.removeFromLeft (textWidth (lf, tr ("label.input")) + 10.0f), tr ("label.input"), colours::textMute);
    r.removeFromLeft ((float) meterArea.getWidth() + 10.0f);

    const auto vf = mono (11.0f, Weight::medium);
    auto item = [&] (const juce::String& label, const juce::String& value, juce::Colour vc)
    {
        if (label.isNotEmpty())
            paint::microLabel (g, r.removeFromLeft (textWidth (lf, label) + 8.0f), label, colours::textMute);
        g.setColour (vc);
        g.setFont (vf);
        g.drawText (value, r.removeFromLeft (textWidth (vf, value) + 4.0f), juce::Justification::centredLeft, false);
        r.removeFromLeft (12.0f);
        paint::vline (g, r.getX(), r.getY() + 8.0f, r.getBottom() - 8.0f);
        r.removeFromLeft (13.0f);
    };

    const auto latencyMs = (double) s.latencySamples * 1000.0 / s.sampleRate();

    item ({}, juce::String (s.inputPeakDb, 1) + " dBFS", colours::text);
    item (tr ("status.latency"), juce::String (latencyMs, 1) + " ms", colours::text);
    item (tr ("status.recTo"), s.recMode == project::RecMode::delivery ? tr ("status.recTo.delivery") : tr ("status.recTo.practice"),
          s.isRecording ? colours::rec : colours::text);
    item (tr ("status.export"), tr ("status.export.value", formatKhz (s.sampleRate()), s.project.bitDepthExport), colours::text);
    if (s.mode != project::Mode::easy)
        item (tr ("status.driver"), s.driver + " " + juce::String (s.bufferSize), colours::textDim);

    // Phase A であることを明示（音は出ない）
    const auto mf = mono (10.0f, Weight::semibold, 0.1f);
    const auto txt = tr ("status.uiMock");
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
    g.drawText (tr ("status.noAudio"), r.withTrimmedRight (8.0f), juce::Justification::centredRight, true);
}
} // namespace vb
