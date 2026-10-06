#include "TrackTabs.h"
#include "WaveLane.h"
#include "audio/PlaybackCore.h"

namespace vb
{
TrackCard::TrackCard (UiSession& u, int i)
    : SessionView (u), index (i)
{
    arm.withIcon (Icon::rec).withToggle (false);
    arm.setTooltip (tr ("track.arm.tooltip"));
    arm.onClick = [this] { session.armTrack (index); };

    mute.withLatch (colours::warn).withToggle (false).withFont (mono (10.5f, Weight::semibold));
    solo.withLatch (colours::signal).withToggle (false).withFont (mono (10.5f, Weight::semibold));
    mute.setTooltip (tr ("monitor.mute"));
    solo.setTooltip (tr ("monitor.solo"));
    mute.onClick = [this] { session.setMute (index, ! track().mute); };
    solo.onClick = [this] { session.setSolo (index, ! track().solo); };

    addAndMakeVisible (arm);
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    onSessionChanged (change::all);
}

// モニター量の線（B12）：ドラッグで 0..1（0.75 = 0 dB）、ダブルクリックで 0 dB。線の外をクリックしたら選択
void TrackCard::setGainFromX (int x)
{
    const auto g = gainArea;
    auto v = juce::jlimit (0.0f, 1.0f, (float) (x - g.getX()) / (float) juce::jmax (1, g.getWidth()));
    if (std::abs (v - 0.75f) < 0.015f)
        v = 0.75f;   // 0 dB に吸い付く
    session.setTrackGain (index, v);
}

void TrackCard::mouseDown (const juce::MouseEvent& e)
{
    draggingGain = gainHitArea().contains (e.getPosition());
    if (draggingGain)
        setGainFromX (e.x);
}

void TrackCard::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingGain)
        setGainFromX (e.x);
}

void TrackCard::mouseUp (const juce::MouseEvent&)
{
    if (draggingGain)
    {
        draggingGain = false;
        repaint();
        return;
    }
    session.selectTrack (index);
}

void TrackCard::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (gainHitArea().contains (e.getPosition()))
        session.setTrackGain (index, 0.75f);
}

void TrackCard::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (gainHitArea().contains (e.getPosition()) ? juce::MouseCursor::LeftRightResizeCursor
                                                             : juce::MouseCursor::PointingHandCursor);
}

void TrackCard::onSessionChanged (juce::uint32 changes)
{
    if (changes & change::prefs)
        repaint();   // 選ぶキーを変えた
    if ((changes & (change::tracks | change::mode)) == 0)
        return;

    arm.setToggleState (track().armed, juce::dontSendNotification);
    mute.setToggleState (track().mute, juce::dontSendNotification);
    solo.setToggleState (track().solo, juce::dontSendNotification);
    resized();
    repaint();
}

void TrackCard::resized()
{
    auto r = getLocalBounds().reduced (10, 0).withTrimmedLeft (isSelected() ? 3 : 0);
    arm.setBounds (r.removeFromLeft (30).withSizeKeepingCentre (30, 28));
    r.removeFromLeft (10);

    auto right = r.removeFromRight (52);
    solo.setBounds (right.removeFromRight (24).withSizeKeepingCentre (24, 22));
    right.removeFromRight (4);
    mute.setBounds (right.removeFromRight (24).withSizeKeepingCentre (24, 22));
    r.removeFromRight (8);

    gainArea = r.removeFromBottom (12).withTrimmedBottom (7);
    textArea = r.withTrimmedTop (5);
}

void TrackCard::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const bool over = isMouseOver (true);
    const bool selected = isSelected();
    const auto& t = track();

    if (selected)
    {
        paint::keycap (g, b, { over, false, true, true });
        g.setColour (colours::signal);
        g.fillRoundedRectangle (b.reduced (1.5f).withWidth (3.0f), 1.5f);
    }
    else
    {
        g.setColour (over ? colours::raised.withAlpha (0.6f) : colours::panel);
        g.fillRoundedRectangle (b.reduced (0.5f), metrics::keyRadius);
        g.setColour (colours::line);
        g.drawRoundedRectangle (b.reduced (0.5f), metrics::keyRadius, 1.0f);
    }

    // 名前 + ホットキー
    auto tx = textArea;
    auto top = tx.removeFromTop (tx.getHeight() / 2 + 2);
    const auto nf = sans (13.5f, Weight::semibold);
    const auto name = trackName (t.type);
    g.setColour (selected ? colours::text : colours::text.withAlpha (0.82f));
    g.setFont (nf);
    const auto nw = juce::jmin (top.getWidth() - 24, (int) textWidth (nf, name) + 2);
    g.drawText (name, top.removeFromLeft (nw), juce::Justification::bottomLeft, true);

    top.removeFromLeft (7);
    // 選ぶキー（#28 で変えられる・なしにできる。前は 1〜4 の決め打ちで、変えても表示が変わらなかった。バグチェック 2026-10-05）
    const auto keyText = index < 4 ? state().shortcuts.keyName ((shortcuts::Action) ((int) shortcuts::Action::track1 + index)) : juce::String();
    if (keyText.isNotEmpty())
    {
        const auto kf = mono (9.5f, Weight::semibold);
        const auto key = top.removeFromLeft (juce::jmax (15, (int) textWidth (kf, keyText) + 6)).withTrimmedTop (top.getHeight() - 15).toFloat();
        paint::inset (g, key, 2.0f);
        g.setColour (colours::textDim);
        g.setFont (kf);
        g.drawText (keyText, key, juce::Justification::centred, false);
    }

    // テイク情報
    const auto* tr_ = state().project.findTrack (t.type);
    const auto takes = tr_ != nullptr ? (int) tr_->takes.size() : 0;
    const auto segs  = tr_ != nullptr ? (int) tr_->comp.size() : 0;

    g.setFont (sans (11.0f));
    g.setColour (takes == 0 ? colours::textMute : colours::textDim);
    g.drawText (takes == 0 ? tr ("track.unrecorded") : tr ("track.takes", takes, segs), tx, juce::Justification::topLeft, true);

    // 動かしている間は dB を出す
    if (draggingGain)
    {
        const auto db = juce::Decibels::gainToDecibels (audio::PlaybackCore::faderToGain (t.monitorGain), -100.0f);
        g.setColour (colours::text);
        g.setFont (mono (10.5f, Weight::medium));
        g.drawText (db <= -99.0f ? juce::String ("-inf dB") : (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB",
                    tx, juce::Justification::topRight, false);
    }

    // モニター量
    const auto gr = gainArea.toFloat().withSizeKeepingCentre ((float) gainArea.getWidth(), 3.0f);
    g.setColour (colours::bgDeep);
    g.fillRoundedRectangle (gr, 1.5f);
    g.setColour ((selected ? colours::signal : colours::textDim).withAlpha (0.85f));
    g.fillRoundedRectangle (gr.withWidth (gr.getWidth() * t.monitorGain), 1.5f);
}

//==============================================================================
TrackTabs::TrackTabs (UiSession& u, Actions& a)
    : SessionView (u), actions (a), compare (tr ("track.compare"))
{
    for (int i = 0; i < (int) u->trackUi.size(); ++i)
        addChildComponent (cards.add (new TrackCard (u, i)));

    // LED は比べている間だけ点く（パネルは右に出るので、キーは見えたまま）
    compare.withIcon (Icon::compare).withLed (colours::signal).withToggle (false);
    compare.onClick = [this] { if (actions.openTakeCompare) actions.openTakeCompare (0, 0); };   // IN / OUT があればその範囲、無ければ曲全体
    addAndMakeVisible (compare);

    onSessionChanged (change::all);
}

void TrackTabs::onSessionChanged (juce::uint32 changes)
{
    if ((changes & (change::mode | change::tracks | change::takes | change::transport | change::song)) == 0)
        return;

    for (auto* c : cards)
        c->setVisible (session.isTrackVisible (state().trackUi[(size_t) c->trackIndex()].type));

    // テイク比較は標準以上（DESIGN 2）。比べるテイクが無い・録音中は押せない（理由はツールチップ）
    const bool can = session.canCompareTakes();
    compare.setVisible (state().mode != project::Mode::easy);
    compare.setEnabled (can || state().compare.active);
    compare.setToggleState (state().compare.active, juce::dontSendNotification);
    compare.setTooltip (can || state().compare.active ? tr ("track.compare.tooltip")
                        : state().isRecording        ? tr ("compare.disabled.recording")
                                                     : tr ("compare.disabled.noTakes"));
    resized();
}

void TrackTabs::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 9);
    r.removeFromLeft (metrics::gutter - metrics::pad);

    if (compare.isVisible())
    {
        compare.setSize (10, 34);
        compare.setBounds (r.removeFromRight (compare.idealWidth()).withSizeKeepingCentre (compare.idealWidth(), 34));
        r.removeFromRight (16);
    }

    int visible = 0;
    for (auto* c : cards) visible += c->isVisible() ? 1 : 0;

    const auto w = juce::jmin (264, (r.getWidth() - 8 * (juce::jmax (1, visible) - 1)) / juce::jmax (1, visible));
    for (auto* c : cards)
    {
        if (! c->isVisible()) continue;
        c->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (8);
    }
}

void TrackTabs::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    paint::microLabel (g, getLocalBounds().withWidth (metrics::gutter).toFloat().withTrimmedLeft ((float) metrics::pad),
                       tr ("label.track"), colours::textMute);
}
} // namespace vb
