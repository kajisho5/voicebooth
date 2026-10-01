#include "TransportBar.h"

namespace vb
{
void SeekBar::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (colours::bgDeep);
    g.fillRoundedRectangle (r, 5.0f);

    const auto inner = r.reduced (6.0f, 4.0f);
    TimeMap map { 0, session.project.lengthSamples, inner.getX(), inner.getRight() };

    // 範囲（ループ）
    {
        const auto x0 = map.x (session.rangeIn), x1 = map.x (session.rangeOut);
        g.setColour (colours::accent.withAlpha (session.loopOn ? 0.14f : 0.06f));
        g.fillRect (juce::Rectangle<float> (x0, r.getY(), x1 - x0, r.getHeight()));
    }

    // オフボ概形
    const auto cy = inner.getCentreY();
    const auto playX = map.x (session.playhead);
    for (int px = (int) inner.getX(); px < (int) inner.getRight(); px += 2)
    {
        const auto s0 = map.sampleAt ((float) px), s1 = map.sampleAt ((float) px + 2.0f);
        float a = 0.0f;
        for (int k = 0; k < 4; ++k)
            a = juce::jmax (a, dummy::backingAmplitude (session, s0 + (s1 - s0) * k / 4));

        const auto h = juce::jmax (1.0f, a * inner.getHeight());
        g.setColour ((float) px < playX ? colours::accent.withAlpha (0.55f) : colours::textDim.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> ((float) px, cy - h * 0.5f, 1.2f, h));
    }

    // 表示中ウィンドウ（ピッチ/波形レーンのズーム範囲）
    {
        const auto x0 = map.x (session.viewStart), x1 = map.x (session.viewEnd);
        g.setColour (colours::text.withAlpha (0.35f));
        g.drawRoundedRectangle (juce::Rectangle<float> (x0, r.getY() + 1.0f, x1 - x0, r.getHeight() - 2.0f), 3.0f, 1.0f);
    }

    // 範囲の境界
    g.setColour (colours::accent.withAlpha (0.8f));
    g.fillRect (juce::Rectangle<float> (map.x (session.rangeIn), r.getY(), 1.0f, r.getHeight()));
    g.fillRect (juce::Rectangle<float> (map.x (session.rangeOut) - 1.0f, r.getY(), 1.0f, r.getHeight()));

    // 再生ヘッド
    g.setColour (colours::accent);
    g.fillRect (juce::Rectangle<float> (playX - 1.0f, r.getY(), 2.0f, r.getHeight()));
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ playX, r.getY() + 2.0f }));
}

//==============================================================================
TransportBar::TransportBar (const dummy::Session& s)
    : session (s),
      rangeIn ("IN  " + formatTime (s.rangeIn, s.sampleRate(), true)),
      rangeOut ("OUT  " + formatTime (s.rangeOut, s.sampleRate(), true)),
      seek (s),
      countIn ({ "Off", jp ("1小節"), jp ("2小節") }, s.countInBars)
{
    play.setIcon (s.isPlaying ? Icon::pause : Icon::play);
    play.setToggleState (s.isPlaying, juce::dontSendNotification);
    play.setRound (true);

    rec.setRound (true);
    rec.setIconColour (colours::rec);
    rec.setFilled (true);
    rec.setClickingTogglesState (true);   // Phase A: 見た目トグルのみ
    rec.onClick = [this] { rec.setIconColour (rec.getToggleState() ? colours::text : colours::rec); };

    loop.setLeadingIcon (Icon::loop);
    loop.setToggleState (s.loopOn, juce::dontSendNotification);

    for (auto* b : { &rangeIn, &rangeOut })
    {
        b->setClickingTogglesState (false);
        b->setFontSize (12.0f);
    }
    rangeIn.setLeadingIcon (Icon::rangeIn);
    rangeOut.setLeadingIcon (Icon::rangeOut);
    rangeIn.setTooltip (jp ("範囲開始 ["));
    rangeOut.setTooltip (jp ("範囲終了 ]"));

    click.setLeadingIcon (Icon::metronome);
    click.setToggleState (s.clickOn, juce::dontSendNotification);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &toStart, &play, &stop, &rec, &loop, &rangeIn, &rangeOut, &clearRange, &seek, &countIn, &click })
        addAndMakeVisible (c);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);
    auto centreH = [] (juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre (a.getWidth(), h); };

    toStart.setBounds (centreH (r.removeFromLeft (34), 34));
    r.removeFromLeft (6);
    play.setBounds (centreH (r.removeFromLeft (40), 40));
    r.removeFromLeft (6);
    stop.setBounds (centreH (r.removeFromLeft (34), 34));
    r.removeFromLeft (10);
    rec.setBounds (centreH (r.removeFromLeft (40), 40));
    r.removeFromLeft (18);

    timeArea = r.removeFromLeft (190);
    r.removeFromLeft (4);
    beatArea = r.removeFromLeft (76);
    r.removeFromLeft (14);

    loop.setBounds (centreH (r.removeFromLeft (loop.idealWidth()), 30));
    r.removeFromLeft (6);
    rangeIn.setBounds (centreH (r.removeFromLeft (rangeIn.idealWidth()), 30));
    r.removeFromLeft (4);
    rangeOut.setBounds (centreH (r.removeFromLeft (rangeOut.idealWidth()), 30));
    r.removeFromLeft (4);
    clearRange.setBounds (centreH (r.removeFromLeft (30), 30));

    click.setBounds (centreH (r.removeFromRight (click.idealWidth()), 30));
    r.removeFromRight (8);
    const auto ciW = countIn.idealWidth();
    countIn.setBounds (centreH (r.removeFromRight (ciW), 30));
    countInLabel = r.removeFromRight (70);
    r.removeFromRight (8);

    r.removeFromLeft (14);
    seek.setBounds (centreH (r, 30));
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    g.setColour (colours::border);
    g.fillRect (getLocalBounds().removeFromBottom (1));

    // 現在時間 / 総時間
    {
        auto r = timeArea;
        const auto cur = formatTime (session.playhead, session.sampleRate(), true);
        const auto tot = " / " + formatTime (session.project.lengthSamples, session.sampleRate(), false);
        const auto big = font (24.0f, FontWeight::bold);
        g.setColour (colours::text);
        g.setFont (big);
        const auto w = textWidth (big, cur);
        g.drawText (cur, r.removeFromLeft ((int) w + 2), juce::Justification::centredLeft, false);
        g.setColour (colours::textDim);
        g.setFont (font (14.0f));
        g.drawText (tot, r, juce::Justification::centredLeft, false);
    }

    // 小節.拍
    {
        const auto beatLen = (int64) std::llround (60.0 / session.bpm() * session.sampleRate());
        const auto beats = session.playhead / beatLen;
        const auto bar = beats / session.beatsPerBar + 1;
        const auto beat = beats % session.beatsPerBar + 1;

        auto r = beatArea.toFloat().withSizeKeepingCentre ((float) beatArea.getWidth(), 34.0f);
        g.setColour (colours::panelHi);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (colours::text);
        g.setFont (font (15.0f, FontWeight::bold));
        g.drawText (juce::String (bar) + "." + juce::String (beat), r.removeFromTop (20.0f).translated (0, 1.0f),
                    juce::Justification::centred, false);
        g.setColour (colours::textDim);
        g.setFont (font (10.0f));
        g.drawText (jp ("小節.拍"), r.translated (0, -2.0f), juce::Justification::centred, false);
    }

    g.setColour (colours::textDim);
    g.setFont (font (11.0f, FontWeight::bold));
    g.drawText (jp ("カウントイン"), countInLabel, juce::Justification::centredRight, false);
}
} // namespace vb
