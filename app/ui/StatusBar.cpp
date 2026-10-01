#include "StatusBar.h"

namespace vb
{
StatusBar::StatusBar (const dummy::Session& s) : session (s)
{
    mini.setLevels (s.inputPeakDb, s.inputRmsDb, s.inputPeakHoldDb, false);
    addAndMakeVisible (mini);
}

void StatusBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);
    r.removeFromLeft (36);
    meterArea = r.removeFromLeft (110);
    mini.setBounds (meterArea.withSizeKeepingCentre (meterArea.getWidth(), 12));
}

void StatusBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgDeep);
    g.setColour (colours::border);
    g.fillRect (getLocalBounds().withHeight (1));

    auto r = getLocalBounds().reduced (metrics::pad, 0);
    const auto f = font (11.0f);
    const auto fb = font (11.0f, FontWeight::bold);

    g.setFont (fb);
    g.setColour (colours::textDim);
    g.drawText (jp ("入力"), r.removeFromLeft (36), juce::Justification::centredLeft, false);
    r.removeFromLeft (meterArea.getWidth() + 8);

    auto item = [&] (const juce::String& label, const juce::String& value, juce::Colour vc)
    {
        g.setFont (f);
        g.setColour (colours::textDim);
        const auto lw = (int) textWidth (f, label) + 6;
        g.drawText (label, r.removeFromLeft (lw), juce::Justification::centredLeft, false);
        g.setFont (fb);
        g.setColour (vc);
        const auto vw = (int) textWidth (fb, value) + 4;
        g.drawText (value, r.removeFromLeft (vw), juce::Justification::centredLeft, false);

        r.removeFromLeft (12);
        g.setColour (colours::border);
        g.fillRect (r.removeFromLeft (1).reduced (0, 8));
        r.removeFromLeft (12);
    };

    const auto latencyMs = (double) session.latencySamples * 1000.0 / session.sampleRate();

    item ("", juce::String (session.inputPeakDb, 1) + " dBFS", colours::text);
    item (jp ("レイテンシ補正"), juce::String (latencyMs, 1) + " ms (" + juce::String (session.latencySamples) + " smp)", colours::text);
    item (jp ("録音先"), session.recMode == project::RecMode::delivery ? jp ("Dry / 納品") : jp ("練習"),
          session.isRecording ? colours::rec : colours::text);
    item (jp ("書き出し"), juce::String (session.sampleRate() / 1000) + " kHz / " + juce::String (session.project.bitDepthExport) + jp (" bit / モノラル"), colours::text);
    item (session.driver, juce::String (session.bufferSize) + " smp", colours::textDim);

    // Phase A であることを明示（音は出ない）
    g.setFont (fb);
    g.setColour (colours::warn);
    g.drawText (jp ("UI MOCK — 音声デバイス未接続"), r, juce::Justification::centredRight, false);
}
} // namespace vb
