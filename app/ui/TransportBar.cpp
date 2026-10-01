#include "TransportBar.h"

namespace vb
{
void OverviewSeek::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    paint::inset (g, r);

    const auto inner = r.reduced (8.0f, 5.0f);
    TimeMap map { 0, session.project.lengthSamples, inner.getX(), inner.getRight() };
    const auto playX = map.x (session.playhead);

    // ループ範囲
    {
        const auto x0 = map.x (session.rangeIn), x1 = map.x (session.rangeOut);
        g.setColour (colours::signal.withAlpha (session.loopOn ? 0.10f : 0.04f));
        g.fillRect (juce::Rectangle<float> (x0, r.getY() + 2.0f, x1 - x0, r.getHeight() - 4.0f));
        g.setColour (colours::signal.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (x0, r.getBottom() - 4.0f, x1 - x0, 2.0f));
    }

    // オフボ概形（下から伸びるバー：波形レーンと見え方を変える）
    for (float px = inner.getX(); px < inner.getRight(); px += 2.0f)
    {
        const auto s0 = map.sampleAt (px), s1 = map.sampleAt (px + 2.0f);
        float a = 0.0f;
        for (int k = 0; k < 4; ++k)
            a = juce::jmax (a, dummy::backingAmplitude (session, s0 + (s1 - s0) * k / 4));

        const auto h = juce::jmax (1.0f, a * inner.getHeight());
        g.setColour (px < playX ? colours::text.withAlpha (0.55f) : colours::textMute.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (px, inner.getBottom() - h, 1.0f, h));
    }

    // 表示中ウィンドウ
    {
        const auto x0 = map.x (session.viewStart), x1 = map.x (session.viewEnd);
        const auto w = juce::Rectangle<float> (x0, r.getY() + 2.0f, x1 - x0, r.getHeight() - 4.0f);
        g.setColour (colours::text.withAlpha (0.06f));
        g.fillRect (w);
        g.setColour (colours::text.withAlpha (0.35f));
        g.drawRect (w, 1.0f);
    }

    // 再生ヘッド
    g.setColour (colours::signal);
    g.fillRect (juce::Rectangle<float> (playX - 0.75f, r.getY() + 2.0f, 1.5f, r.getHeight() - 4.0f));
}

//==============================================================================
TransportBar::TransportBar (const dummy::Session& s)
    : session (s),
      rangeIn ("IN " + formatTime (s.rangeIn, s.sampleRate(), true)),
      rangeOut ("OUT " + formatTime (s.rangeOut, s.sampleRate(), true)),
      seek (s),
      countIn ({ "Off", "1", "2" }, s.countInBars),
      click (jp ("クリック"))
{
    toStart.withIcon (Icon::toStart);
    toStart.setTooltip (jp ("先頭へ"));
    play.withIcon (s.isPlaying ? Icon::pause : Icon::play);
    play.setTooltip (jp ("再生 / 停止（Space）"));
    if (s.isPlaying)
        play.withIconColour (colours::signal);
    stop.withIcon (Icon::stop);
    stop.setTooltip (jp ("停止"));
    rec.withIcon (Icon::rec);
    rec.setTooltip (jp ("録音（R）"));
    rec.setToggleState (s.isRecording, juce::dontSendNotification);   // Phase A: 見た目トグルのみ

    time.setValue (formatTime (s.playhead, s.sampleRate(), true), "/ " + formatTime (s.project.lengthSamples, s.sampleRate(), false));
    time.setMainSize (21.0f);
    {
        const auto beatLen = (int64) std::llround (60.0 / s.bpm() * s.sampleRate());
        const auto beats = s.playhead / beatLen;
        beat.setValue (juce::String (beats / s.beatsPerBar + 1) + "." + juce::String (beats % s.beatsPerBar + 1));
        beat.setMainSize (21.0f);
    }

    loop.setButtonText (jp ("ループ"));
    loop.withIcon (Icon::loop).withLed();
    loop.setToggleState (s.loopOn, juce::dontSendNotification);
    loop.setTooltip (jp ("範囲リピート（L）"));

    for (auto* b : { &rangeIn, &rangeOut })
        b->withFont (mono (11.5f, Weight::medium));
    rangeIn.withIcon (Icon::rangeIn);
    rangeOut.withIcon (Icon::rangeOut);
    rangeIn.setTooltip (jp ("範囲の開始を現在位置に（[）"));
    rangeOut.setTooltip (jp ("範囲の終了を現在位置に（]）"));
    clearRange.withIcon (Icon::close);
    clearRange.setTooltip (jp ("範囲を解除"));

    countIn.setFont (mono (11.5f, Weight::medium));
    click.withIcon (Icon::metronome).withLed();
    click.setToggleState (s.clickOn, juce::dontSendNotification);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &toStart, &play, &stop, &rec, &time, &beat, &loop, &rangeIn, &rangeOut, &clearRange, &seek, &countIn, &click })
        addAndMakeVisible (c);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);
    auto centreH = [] (juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre (a.getWidth(), h); };
    constexpr int keyH = 34;

    toStart.setBounds (centreH (r.removeFromLeft (keyH), keyH));
    r.removeFromLeft (4);
    play.setBounds (centreH (r.removeFromLeft (44), keyH));
    r.removeFromLeft (4);
    stop.setBounds (centreH (r.removeFromLeft (keyH), keyH));
    r.removeFromLeft (10);
    rec.setBounds (centreH (r.removeFromLeft (44), keyH));
    r.removeFromLeft (16);

    time.setBounds (centreH (r.removeFromLeft (172), 46));
    r.removeFromLeft (6);
    beat.setBounds (centreH (r.removeFromLeft (84), 46));
    r.removeFromLeft (16);

    for (auto* b : { &loop, &rangeIn, &rangeOut })
    {
        b->setSize (10, 30);
        b->setBounds (centreH (r.removeFromLeft (b->idealWidth()), 30));
        r.removeFromLeft (4);
    }
    clearRange.setBounds (centreH (r.removeFromLeft (30), 30));
    r.removeFromLeft (14);

    click.setSize (10, 30);
    click.setBounds (centreH (r.removeFromRight (click.idealWidth()), 30));
    r.removeFromRight (8);
    countIn.setBounds (centreH (r.removeFromRight (132), 32));
    countLabel = r.removeFromRight (52);
    r.removeFromRight (6);

    seek.setBounds (centreH (r, 40));
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    paint::hline (g, (float) getHeight() - 1.0f, 0.0f, (float) getWidth());
    paint::microLabel (g, countLabel.toFloat(), "COUNT", colours::textMute, juce::Justification::centredRight);
}
} // namespace vb
