#include "TransportBar.h"

namespace vb
{
void OverviewSeek::seekTo (float x)
{
    const auto r = inner();
    const TimeMap map { 0, state().project.lengthSamples, r.getX(), r.getRight() };
    session.seek (map.sampleAt (juce::jlimit (r.getX(), r.getRight(), x)));
}

void OverviewSeek::paint (juce::Graphics& g)
{
    const auto& s = state();
    const auto r = getLocalBounds().toFloat();
    paint::inset (g, r);

    const auto in = inner();
    const TimeMap map { 0, s.project.lengthSamples, in.getX(), in.getRight() };
    const auto playX = map.x (s.playhead);

    // ループ範囲
    if (s.hasRange())
    {
        const auto x0 = map.x (s.rangeIn), x1 = map.x (s.rangeOut);
        g.setColour (colours::signal.withAlpha (s.loopOn ? 0.10f : 0.04f));
        g.fillRect (juce::Rectangle<float> (x0, r.getY() + 2.0f, x1 - x0, r.getHeight() - 4.0f));
        g.setColour (colours::signal.withAlpha (s.loopOn ? 0.6f : 0.25f));
        g.fillRect (juce::Rectangle<float> (x0, r.getBottom() - 4.0f, x1 - x0, 2.0f));
    }

    // オフボ概形（下から伸びるバー）
    for (float px = in.getX(); px < in.getRight(); px += 2.0f)
    {
        const auto s0 = map.sampleAt (px), s1 = map.sampleAt (px + 2.0f);
        float a = 0.0f;
        for (int k = 0; k < 4; ++k)
            a = juce::jmax (a, dummy::backingAmplitude (s, s0 + (s1 - s0) * k / 4));

        const auto h = juce::jmax (1.0f, a * in.getHeight());
        g.setColour (px < playX ? colours::text.withAlpha (0.55f) : colours::textMute.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (px, in.getBottom() - h, 1.0f, h));
    }

    // 表示中ウィンドウ
    {
        const auto x0 = map.x (s.viewStart), x1 = map.x (s.viewEnd);
        const auto w = juce::Rectangle<float> (x0, r.getY() + 2.0f, x1 - x0, r.getHeight() - 4.0f);
        g.setColour (colours::text.withAlpha (0.06f));
        g.fillRect (w);
        g.setColour (colours::text.withAlpha (0.35f));
        g.drawRect (w, 1.0f);
    }

    // 再生ヘッド
    g.setColour (s.isRecording ? colours::rec : colours::signal);
    g.fillRect (juce::Rectangle<float> (playX - 0.75f, r.getY() + 2.0f, 1.5f, r.getHeight() - 4.0f));
}

//==============================================================================
TransportBar::TransportBar (UiSession& u, Actions& a)
    : SessionView (u), actions (a),
      time (tr ("transport.time")), beat (tr ("transport.barBeat")),
      seek (u),
      countIn ({ tr ("transport.countIn.off"), "1", "2" }, u->countInBars),
      click (tr ("transport.click"))
{
    toStart.withIcon (Icon::toStart);
    toStart.setTooltip (tr ("transport.toStart.tooltip"));
    toStart.onClick = [this] { session.goToStart(); };

    play.setTooltip (tr ("transport.play.tooltip"));
    play.onClick = [this] { session.setPlaying (! state().isPlaying); };

    stop.withIcon (Icon::stop);
    stop.setTooltip (tr ("transport.stop.tooltip"));
    stop.onClick = [this] { session.stop(); };

    rec.withIcon (Icon::rec).withToggle (false);
    rec.setTooltip (tr ("transport.rec.tooltip"));
    rec.onClick = [this] { if (actions.toggleRecord) actions.toggleRecord(); };

    time.setMainSize (21.0f);
    beat.setMainSize (21.0f);

    loop.setButtonText (tr ("transport.loop"));
    loop.withIcon (Icon::loop).withLed().withToggle (false);
    loop.setTooltip (tr ("transport.loop.tooltip"));
    loop.onClick = [this] { session.setLoop (! state().loopOn); };

    for (auto* b : { &rangeIn, &rangeOut })
        b->withFont (mono (11.5f, Weight::medium));
    rangeIn.withIcon (Icon::rangeIn);
    rangeOut.withIcon (Icon::rangeOut);
    rangeIn.setTooltip (tr ("transport.rangeIn.tooltip"));
    rangeOut.setTooltip (tr ("transport.rangeOut.tooltip"));
    rangeIn.onClick = [this] { session.setRangeInAtPlayhead(); };
    rangeOut.onClick = [this] { session.setRangeOutAtPlayhead(); };

    clearRange.withIcon (Icon::close);
    clearRange.setTooltip (tr ("transport.clearRange.tooltip"));
    clearRange.onClick = [this] { session.clearRange(); };

    countIn.setFont (mono (11.5f, Weight::medium));
    countIn.onChange = [this] (int i) { session.setCountIn (i); };

    click.withIcon (Icon::metronome).withLed().withToggle (false);
    click.onClick = [this] { session.setClick (! state().clickOn); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &toStart, &play, &stop, &rec, &time, &beat, &loop, &rangeIn, &rangeOut, &clearRange, &seek, &countIn, &click })
        addAndMakeVisible (c);

    onSessionChanged (change::all);
}

void TransportBar::onSessionChanged (juce::uint32 changes)
{
    const auto& s = state();

    if (changes & change::transport)
    {
        play.withIcon (s.isPlaying ? Icon::pause : Icon::play);
        play.withIconColour (s.isPlaying ? colours::signal : colours::text);
        rec.setToggleState (s.isRecording, juce::dontSendNotification);
        loop.setToggleState (s.loopOn, juce::dontSendNotification);
        click.setToggleState (s.clickOn, juce::dontSendNotification);
        countIn.setSelected (s.countInBars, juce::dontSendNotification);
    }

    if (changes & change::playhead)
    {
        time.setValue (formatTime (s.playhead, s.sampleRate(), true), "/ " + formatTime (s.project.lengthSamples, s.sampleRate(), false));
        const auto beatLen = (int64) std::llround (60.0 / s.bpm() * s.sampleRate());
        const auto beats = s.playhead / beatLen;
        beat.setValue (juce::String (beats / s.beatsPerBar + 1) + "." + juce::String (beats % s.beatsPerBar + 1));
    }

    if (changes & change::range)
    {
        const auto none = juce::String ("-:--.---");
        rangeIn.setButtonText (tr ("transport.in") + " " + (s.hasRange() ? formatTime (s.rangeIn, s.sampleRate(), true) : none));
        rangeOut.setButtonText (tr ("transport.out") + " " + (s.hasRange() ? formatTime (s.rangeOut, s.sampleRate(), true) : none));
        clearRange.setEnabled (s.hasRange());
        loop.setEnabled (s.hasRange());
        resized();
    }
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
    countIn.setBounds (centreH (r.removeFromRight (juce::jmax (132, countIn.idealWidth())), 32));
    countLabel = r.removeFromRight (52);
    r.removeFromRight (6);

    seek.setBounds (centreH (r, 40));
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    paint::hline (g, (float) getHeight() - 1.0f, 0.0f, (float) getWidth());
    paint::microLabel (g, countLabel.toFloat(), tr ("label.count"), colours::textMute, juce::Justification::centredRight);
}
} // namespace vb
