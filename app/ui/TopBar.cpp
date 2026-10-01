#include "TopBar.h"

namespace vb
{
void drawBoothMark (juce::Graphics& g, juce::Rectangle<float> r, bool recording)
{
    r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
    const auto s = r.getWidth();

    // ブースの窓（角丸の枠）
    g.setColour (colours::text);
    g.drawRoundedRectangle (r.reduced (s * 0.06f), s * 0.22f, s * 0.085f);

    // マイクのカプセル
    const auto cap = juce::Rectangle<float> (s * 0.30f, s * 0.44f).withCentre ({ r.getCentreX(), r.getY() + s * 0.47f });
    g.fillRoundedRectangle (cap, s * 0.15f);
    g.fillRect (juce::Rectangle<float> (s * 0.07f, s * 0.14f).withCentre ({ r.getCentreX(), r.getY() + s * 0.76f }));

    // タリー（右上）
    const auto c = juce::Point<float> (r.getRight() - s * 0.06f, r.getY() + s * 0.06f);
    g.setColour (colours::bg0);
    g.fillEllipse (juce::Rectangle<float> (s * 0.36f, s * 0.36f).withCentre (c));
    paint::led (g, c, s * 0.11f, recording ? colours::rec : colours::signal, true);
}

//==============================================================================
TopBar::TopBar (const dummy::Session& s)
    : session (s),
      mode ({ jp ("簡単"), jp ("標準"), jp ("プロ") }, (int) s.mode),
      device (s.inputDevice, KeyButton::Kind::ghost)
{
    device.withIcon (Icon::mic).withFont (sans (12.0f));
    device.setTooltip (jp ("入力デバイス（クリックで入力セットアップ）"));

    tally.setState (s.isRecording ? TallyLamp::State::rec
                                  : (s.isPlaying ? TallyLamp::State::play : TallyLamp::State::standby));

    settings.withIcon (Icon::gear);
    settings.setTooltip (jp ("設定"));

    addAndMakeVisible (mode);
    addAndMakeVisible (device);
    addAndMakeVisible (tally);
    addAndMakeVisible (settings);
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);
    auto centreH = [] (juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre (a.getWidth(), h); };

    logoArea = r.removeFromLeft (136);
    r.removeFromLeft (14);

    settings.setBounds (centreH (r.removeFromRight (32), 32));
    r.removeFromRight (10);
    tally.setBounds (centreH (r.removeFromRight (TallyLamp::idealWidth()), 28));
    r.removeFromRight (10);

    device.setSize (10, 32);
    const auto dw = juce::jmin (260, device.idealWidth());
    device.setBounds (centreH (r.removeFromRight (dw), 32));
    r.removeFromRight (18);

    const auto mw = mode.idealWidth();
    mode.setBounds (centreH (r.removeFromRight (mw), 32));
    modeLabelArea = r.removeFromRight (48);

    songArea = r;
}

void TopBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    paint::hline (g, (float) getHeight() - 1.0f, 0.0f, (float) getWidth());

    // ロゴ
    {
        auto r = logoArea.toFloat();
        drawBoothMark (g, r.removeFromLeft (26.0f).withSizeKeepingCentre (26.0f, 26.0f), session.isRecording);
        r.removeFromLeft (10.0f);
        g.setColour (colours::text);
        g.setFont (sans (16.5f, Weight::semibold));
        g.drawText ("VoiceBooth", r, juce::Justification::centredLeft, false);
    }

    // 区切り + 曲名 + 情報
    {
        paint::vline (g, (float) songArea.getX() - 1.0f, 14.0f, (float) getHeight() - 14.0f);

        auto r = songArea.withTrimmedLeft (14).toFloat();
        const auto tf = sans (14.0f, Weight::medium);
        const auto tw = juce::jmin (r.getWidth() - 220.0f, textWidth (tf, session.songName));
        g.setColour (colours::text);
        g.setFont (tf);
        g.drawText (session.songName, r.removeFromLeft (tw + 2.0f), juce::Justification::centredLeft, true);

        r.removeFromLeft (14.0f);
        const auto meta = juce::String (session.sampleRate() / 1000) + "kHz  "
                        + formatTime (session.project.lengthSamples, session.sampleRate(), false)
                        + "  KEY " + juce::String (session.project.keyOriginal)
                        + "  " + juce::String (juce::roundToInt (session.bpm())) + "BPM";
        g.setColour (colours::textMute);
        g.setFont (mono (11.0f));
        g.drawText (meta, r, juce::Justification::centredLeft, true);
    }

    paint::microLabel (g, modeLabelArea.toFloat().withTrimmedRight (8.0f), "MODE", colours::textMute, juce::Justification::centredRight);
}
} // namespace vb
