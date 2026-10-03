#include "TransportBar.h"
#include "SongMarks.h"

namespace vb
{
void OverviewSeek::seekTo (float x, bool snapToSections)
{
    const auto r = inner();
    const TimeMap map { 0, state().project.lengthSamples, r.getX(), r.getRight() };
    x = juce::jlimit (r.getX(), r.getRight(), x);
    auto target = map.sampleAt (x);

    // 区間の頭（サビなど）の近くでは頭に吸い付く
    if (snapToSections)
    {
        float best = 5.0f;
        for (auto& sec : state().project.sections)
        {
            const auto d = std::abs (map.x (sec.startSample) - x);
            if (d <= best)
            {
                best = d;
                target = sec.startSample;
            }
        }
    }
    session.seek (target);
}

void OverviewSeek::drawSections (juce::Graphics& g, const TimeMap& map, juce::Rectangle<float> r)
{
    const auto& s = state();
    const auto& list = s.project.sections;
    const auto f = sans (9.5f, Weight::medium);

    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto x0 = std::round (map.x (list[(size_t) i].startSample));
        const auto x1 = std::round (map.x (song::sectionEnd (list, i, s.project.lengthSamples)));

        // 帯：交互にわずかに明るく（どこからどこまでが 1 つの区間か）
        if (i % 2 == 0)
        {
            g.setColour (colours::highlight (0.035f));
            g.fillRect (juce::Rectangle<float> (x0, r.getY() + 2.0f, x1 - x0, r.getHeight() - 4.0f));
        }
        g.setColour (i == s.selectedSection ? colours::signal.withAlpha (0.8f) : colours::lineHi);
        g.fillRect (juce::Rectangle<float> (x0, r.getY() + 2.0f, 1.0f, r.getHeight() - 4.0f));

        // 名前（入る幅があれば）
        const auto name = marks::sectionName (list, i);
        const auto w = juce::jmin (textWidth (f, name) + 8.0f, x1 - x0 - 3.0f);
        if (w >= 14.0f)
        {
            const auto tag = juce::Rectangle<float> (x0 + 2.0f, r.getY() + 3.0f, w, 12.0f);
            g.setColour (colours::bgDeep.withAlpha (0.75f));
            g.fillRoundedRectangle (tag, 2.0f);
            g.setColour (list[(size_t) i].source == song::Source::confirmed ? colours::textDim : colours::textMute);
            g.setFont (f);
            g.drawText (name, tag.reduced (4.0f, 0.0f), juce::Justification::centredLeft, true);
        }
    }
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

    // オフボ概形（下から伸びるバー）：薄いのがピーク、濃いのが RMS（曲の起伏）
    for (float px = in.getX(); px < in.getRight(); px += 2.0f)
    {
        const auto s0 = map.sampleAt (px), s1 = map.sampleAt (px + 2.0f);
        const auto peak = juce::jmin (1.0f, dummy::backingPeak (s, s0, s1));
        const auto rms = juce::jmin (peak, dummy::backingRms (s, s0, s1));
        const bool played = px < playX;

        const auto hp = juce::jmax (1.0f, peak * in.getHeight());
        g.setColour ((played ? colours::text : colours::textMute).withAlpha (0.22f));
        g.fillRect (juce::Rectangle<float> (px, in.getBottom() - hp, 1.0f, hp));

        const auto hr = juce::jmax (1.0f, rms * in.getHeight());
        g.setColour ((played ? colours::text : colours::textDim).withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (px, in.getBottom() - hr, 1.0f, hr));
    }

    drawSections (g, map, r);

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
    play.withShortcut ("Space");
    play.onClick = [this] { session.setPlaying (! state().isPlaying); };

    stop.withIcon (Icon::stop);
    stop.setTooltip (tr ("transport.stop.tooltip"));
    stop.onClick = [this] { session.stop(); };

    rec.withIcon (Icon::rec).withToggle (false).withBreathing();   // REC 中は ● がゆっくり呼吸
    rec.setTooltip (tr ("transport.rec.tooltip"));
    rec.withShortcut ("R");
    rec.onClick = [this] { if (actions.toggleRecord) actions.toggleRecord(); };

    time.setMainSize (21.0f);
    beat.setMainSize (21.0f);

    loop.setButtonText (tr ("transport.loop"));
    loop.withIcon (Icon::loop).withLed().withToggle (false);
    loop.setTooltip (tr ("transport.loop.tooltip"));
    loop.withShortcut ("L");
    loop.onClick = [this] { session.setLoop (! state().loopOn); };

    for (auto* b : { &rangeIn, &rangeOut })
        b->withFont (mono (11.5f, Weight::medium));
    rangeIn.withIcon (Icon::rangeIn);
    rangeOut.withIcon (Icon::rangeOut);
    rangeIn.setTooltip (tr ("transport.rangeIn.tooltip"));
    rangeOut.setTooltip (tr ("transport.rangeOut.tooltip"));
    rangeIn.withShortcut ("[");
    rangeOut.withShortcut ("]");
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

    if (changes & (change::transport | change::songInfo))
    {
        // クリック・カウントインは曲のテンポで鳴る（2026-10-02）。テンポが分からない間は、なぜ鳴らないかをツールチップで
        click.setTooltip (s.tempoKnown() ? tr ("transport.click.tooltip") : tr ("transport.click.noTempo"));
        countIn.setTooltip (s.tempoKnown() ? tr ("transport.countIn.tooltip") : tr ("transport.countIn.noTempo"));
    }

    if (changes & (change::playhead | change::songInfo | change::transport))
    {
        time.setValue (formatTime (s.playhead, s.sampleRate(), true), "/ " + formatTime (s.project.lengthSamples, s.sampleRate(), false));
        if (s.tempoKnown())
        {
            // 1 小節目の位置から（それより前は 0、-1 …）。カウントイン中は数えている拍（曲の位置はまだ動かない）
            const auto bb = s.barBeatAt (s.countingIn ? s.countInPosition : s.playhead);
            beat.setValue (juce::String (bb.bar) + "." + juce::String (bb.beat));
        }
        else
        {
            beat.setValue ("-.-");   // テンポがまだ分からない（推定前・手入力前）
        }
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
    beat.setBounds (centreH (r.removeFromLeft (104), 46));
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
    if (! countLabel.isEmpty())
        paint::microLabel (g, countLabel.toFloat(), tr ("label.count"), colours::textMute, juce::Justification::centredRight);
}
} // namespace vb
