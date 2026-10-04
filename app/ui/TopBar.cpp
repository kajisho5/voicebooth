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
    g.setColour (colours::panel);
    g.fillEllipse (juce::Rectangle<float> (s * 0.36f, s * 0.36f).withCentre (c));
    paint::led (g, c, s * 0.11f, recording ? colours::rec : colours::signal, true);
}

//==============================================================================
TopBar::TopBar (UiSession& u, Actions& a)
    : SessionView (u), actions (a),
      mode ({ tr ("mode.easy"), tr ("mode.standard"), tr ("mode.pro") }, (int) u->mode),
      exportKey (tr ("topbar.export"))
{
    mode.onChange = [this] (int i) { session.setMode ((project::Mode) i); };
    mode.setTitle (tr ("settings.mode"));   // 読み上げの名前（#28）

    device.setButtonText (inputDisplayName (state()));
    device.withIcon (Icon::mic).withFont (sans (12.0f));
    device.setTooltip (tr ("topbar.device.tooltip"));
    device.onClick = [this] { if (actions.openSetup) actions.openSetup(); };

    songInfo.withIcon (Icon::note).withFont (mono (11.0f));
    songInfo.setTooltip (tr ("topbar.songInfo.tooltip"));
    songInfo.onClick = [this] { if (actions.openSongInfo) actions.openSongInfo(); };

    exportKey.withIcon (Icon::exportFile);
    exportKey.setTooltip (tr ("topbar.export.tooltip"));
    exportKey.withShortcut (commandKeyName ('E'));
    exportKey.onClick = [this] { if (actions.openExport) actions.openExport(); };

    settings.withIcon (Icon::gear);
    settings.setTooltip (tr ("topbar.settings"));
    settings.withShortcut (commandKeyName (','));
    settings.onClick = [this] { if (actions.openSettings) actions.openSettings(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &mode, &device, &songInfo, &tally, &exportKey, &settings })
        addAndMakeVisible (c);

    onSessionChanged (change::all);
}

void TopBar::onSessionChanged (juce::uint32 changes)
{
    if (changes & change::song)
        repaint (songArea);   // 曲名・長さ・SR

    // KEY / BPM（分からなければ「-」。B4b で手入力、B9b で推定）
    if (changes & (change::song | change::songInfo))
    {
        const auto& s = state();
        songInfo.setButtonText (tr ("topbar.songInfo",
                                    s.keyKnown() ? s.project.key.shortName() : juce::String ("-"),
                                    s.tempoKnown() ? song::formatBpm (s.bpm()) : juce::String ("-")));
        songInfo.setTitle (songInfo.getButtonText().isEmpty() ? tr ("topbar.songInfo.tooltip") : juce::String());   // 読み上げ：表示中の KEY / BPM（#28）
        resized();
        repaint (songArea);
    }

    // 入力デバイス名（無ければ警告色。DESIGN 4.1）
    if (changes & change::device)
    {
        const auto& s = state();
        const bool missing = s.engineAttached && ! s.input.open;
        device.setButtonText (inputDisplayName (s));
        device.withIconColour (missing ? colours::warn : colours::text);
        resized();
        repaint();
    }

    if ((changes & (change::transport | change::mode)) == 0)
        return;

    const auto& s = state();
    tally.setState (s.isRecording ? TallyLamp::State::rec
                                  : (s.isPlaying ? TallyLamp::State::play : TallyLamp::State::standby));
    mode.setSelected ((int) s.mode, juce::dontSendNotification);
    repaint (logoArea);
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);
    auto centreH = [] (juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre (a.getWidth(), h); };

    // ロゴの幅は名前の文字から測る（OS のフォントで描く言語で「VoiceBo」と切れていた。#19）
    logoArea = r.removeFromLeft (juce::jmax (136, 36 + juce::roundToInt (textWidth (sans (16.5f, Weight::semibold), tr ("app.name"))) + 4));
    r.removeFromLeft (14);

    settings.setBounds (centreH (r.removeFromRight (32), 32));
    r.removeFromRight (8);
    exportKey.setSize (10, 32);
    exportKey.setBounds (centreH (r.removeFromRight (exportKey.idealWidth()), 32));
    r.removeFromRight (12);
    tally.setBounds (centreH (r.removeFromRight (TallyLamp::idealWidth()), 28));
    r.removeFromRight (10);

    device.setSize (10, 32);
    const auto dw = juce::jmin (240, device.idealWidth());
    device.setBounds (centreH (r.removeFromRight (dw), 32));
    r.removeFromRight (14);

    const auto mw = mode.idealWidth();
    mode.setBounds (centreH (r.removeFromRight (mw), 32));
    modeLabelArea = r.removeFromRight (48);
    r.removeFromRight (6);

    songInfo.setSize (10, 30);
    songInfo.setBounds (centreH (r.removeFromRight (songInfo.idealWidth()), 30));
    r.removeFromRight (6);

    songArea = r;
}

void TopBar::mouseUp (const juce::MouseEvent& e)
{
    // ロゴ → 起動画面（曲の読み込み / 最近のプロジェクト）
    if (logoArea.contains (e.getPosition()) && actions.openStart)
        actions.openStart();
}

void TopBar::paint (juce::Graphics& g)
{
    const auto& s = state();
    g.fillAll (colours::panel);
    paint::hline (g, (float) getHeight() - 1.0f, 0.0f, (float) getWidth());

    // ロゴ
    {
        auto r = logoArea.toFloat();
        drawBoothMark (g, r.removeFromLeft (26.0f).withSizeKeepingCentre (26.0f, 26.0f), s.isRecording);
        r.removeFromLeft (10.0f);
        g.setColour (colours::text);
        g.setFont (sans (16.5f, Weight::semibold));
        g.drawText (tr ("app.name"), r, juce::Justification::centredLeft, false);
    }

    // 区切り + 曲名 + 情報
    {
        paint::vline (g, (float) songArea.getX() - 1.0f, 14.0f, (float) getHeight() - 14.0f);

        auto r = songArea.withTrimmedLeft (14).toFloat();
        const auto tf = sansFor (s.songName, 14.0f, Weight::medium);
        const auto tw = juce::jmin (r.getWidth() * 0.5f, textWidth (tf, s.songName));
        g.setColour (colours::text);
        g.setFont (tf);
        g.drawText (s.songName, r.removeFromLeft (tw + 2.0f), juce::Justification::centredLeft, true);

        r.removeFromLeft (14.0f);
        // 44.1 kHz は小数で（44 と出さない）。キーとテンポは右のキー（songInfo）に出す
        const auto sr = s.sampleRate();
        const auto meta = tr ("topbar.meta", formatKhz (sr), formatTime (s.project.lengthSamples, sr, false));
        g.setColour (colours::textMute);
        g.setFont (mono (11.0f));
        g.drawText (meta, r, juce::Justification::centredLeft, true);
    }

    paint::microLabel (g, modeLabelArea.toFloat().withTrimmedRight (8.0f), tr ("label.mode"), colours::textMute, juce::Justification::centredRight);
}
} // namespace vb
