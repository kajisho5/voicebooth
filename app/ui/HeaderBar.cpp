#include "HeaderBar.h"

namespace vb
{
HeaderBar::HeaderBar (const dummy::Session& s)
    : session (s),
      mode ({ jp ("簡単"), jp ("標準"), jp ("プロ") }, (int) s.mode),
      device (s.inputDevice, colours::text)
{
    device.setLeadingIcon (Icon::mic);
    device.setClickingTogglesState (false);
    device.setFontSize (12.0f);

    addAndMakeVisible (mode);
    addAndMakeVisible (device);
    addAndMakeVisible (settings);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);

    logoArea = r.removeFromLeft (150);
    r.removeFromLeft (12);

    settings.setBounds (r.removeFromRight (34).withSizeKeepingCentre (34, 34));
    r.removeFromRight (10);
    stateArea = r.removeFromRight (104).withSizeKeepingCentre (104, 28);
    r.removeFromRight (10);
    device.setBounds (r.removeFromRight (juce::jmin (260, device.idealWidth())).withSizeKeepingCentre (juce::jmin (260, device.idealWidth()), 32));
    r.removeFromRight (20);

    const auto modeW = mode.idealWidth() + 24;
    mode.setBounds (r.removeFromRight (modeW).withSizeKeepingCentre (modeW, 32));
    modeLabelArea = r.removeFromRight (52);

    songArea = r;
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::border);
    g.fillRect (getLocalBounds().removeFromBottom (1));

    // --- ロゴ ---
    {
        auto r = logoArea;
        const auto mark = r.removeFromLeft (28).withSizeKeepingCentre (28, 28).toFloat();
        g.setGradientFill (juce::ColourGradient (colours::accent, mark.getTopLeft(), colours::refPitch, mark.getBottomRight(), false));
        g.fillRoundedRectangle (mark, 7.0f);

        // マイク前の波形をイメージしたバー
        g.setColour (colours::bg0);
        const float heights[] = { 0.28f, 0.55f, 0.85f, 0.55f, 0.28f };
        for (int i = 0; i < 5; ++i)
        {
            const auto h = mark.getHeight() * heights[i];
            g.fillRoundedRectangle (juce::Rectangle<float> (2.6f, h).withCentre ({ mark.getX() + 7.0f + 3.5f * (float) i, mark.getCentreY() }), 1.3f);
        }

        r.removeFromLeft (10);
        g.setColour (colours::text);
        g.setFont (font (17.0f, FontWeight::bold));
        g.drawText ("VoiceBooth", r, juce::Justification::centredLeft, false);
    }

    // --- 区切り + 曲名 ---
    {
        g.setColour (colours::border);
        g.fillRect (songArea.getX() - 10, 14, 1, getHeight() - 28);

        auto r = songArea.reduced (6, 0);
        auto title = font (15.0f, FontWeight::bold);
        g.setColour (colours::text);
        g.setFont (title);
        const auto tw = juce::jmin ((float) r.getWidth() - 160.0f, textWidth (title, session.songName));
        g.drawText (session.songName, r.removeFromLeft ((int) tw + 2), juce::Justification::centredLeft, true);

        r.removeFromLeft (10);
        const auto meta = juce::String (session.sampleRate() / 1000) + " kHz  ·  "
                        + formatTime (session.project.lengthSamples, session.sampleRate(), false)
                        + "  ·  " + jp ("原キー ") + juce::String (session.project.keyOriginal)
                        + "  ·  " + juce::String (juce::roundToInt (session.bpm())) + " BPM";
        g.setColour (colours::textDim);
        g.setFont (font (12.0f));
        g.drawText (meta, r, juce::Justification::centredLeft, true);
    }

    // --- モード見出し ---
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, FontWeight::bold));
    g.drawText (jp ("モード"), modeLabelArea, juce::Justification::centredRight, false);

    // --- 状態ピル（準備中 / 再生中 / 録音中） ---
    {
        const auto r = stateArea.toFloat();
        juce::Colour c = colours::textDim;
        juce::String label = jp ("準備中");

        if (session.isRecording)      { c = colours::rec;    label = jp ("録音中"); }
        else if (session.isPlaying)   { c = colours::accent; label = jp ("再生中"); }

        g.setColour (c.withAlpha (session.isRecording ? 0.9f : 0.12f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (c.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);

        const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 16.0f, r.getCentreY() });
        g.setColour (session.isRecording ? colours::text : c);
        g.fillEllipse (dot);

        g.setFont (font (12.0f, FontWeight::bold));
        g.drawText (label, r.withTrimmedLeft (26.0f), juce::Justification::centredLeft, false);
    }
}
} // namespace vb
