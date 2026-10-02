#include "LyricsLane.h"

namespace vb
{
namespace
{
    /** 小さなキーの形（「Enter」など）。幅を返す */
    float drawKeyHint (juce::Graphics& g, juce::Point<float> leftCentre, const juce::String& key)
    {
        const auto f = mono (10.0f, Weight::semibold);
        const auto w = textWidth (f, key) + 12.0f;
        const auto r = juce::Rectangle<float> (leftCentre.x, leftCentre.y - 9.0f, w, 18.0f);
        paint::keycap (g, r, {}, 3.0f);
        g.setColour (colours::text);
        g.setFont (f);
        g.drawText (key, r, juce::Justification::centred, false);
        return w;
    }
}

LyricsLane::LyricsLane (UiSession& u, Actions& a) : SessionView (u), actions (a), syncKey (tr ("lyrics.sync")), autoKey (tr ("lyrics.auto"))
{
    editButton.setButtonText (tr ("lyrics.pad"));
    editButton.withIcon (Icon::edit).withFont (sans (11.5f, Weight::medium));
    editButton.setTooltip (tr ("lyrics.pad.tooltip"));
    editButton.onClick = [this] { if (actions.openLyrics) actions.openLyrics(); };
    addAndMakeVisible (editButton);

    // タップで合わせる（DESIGN 7.5.3）：押している間 LED が点く
    syncKey.withLed().withToggle (false).withFont (sans (11.5f, Weight::medium));
    syncKey.setTooltip (tr ("lyrics.sync.tooltip"));
    syncKey.withShortcut ("Enter");
    syncKey.onClick = [this] { session.setLyricSyncing (! state().lyricSyncing); };
    addChildComponent (syncKey);

    // 自動で合わせる（B17）：お手本の声を認識して行の時刻を推定する。動いている間 LED が点き、押すと止める
    autoKey.withLed().withToggle (false).withFont (sans (11.5f, Weight::medium));
    autoKey.setTooltip (tr ("lyrics.auto.tooltip"));
    autoKey.onClick = [this]
    {
        if (state().lyricsAligning) session.stopLyricsAlign();
        else                        session.alignLyricsAuto();
    };
    addChildComponent (autoKey);

    refreshKeys();
}

void LyricsLane::onSessionChanged (juce::uint32 c)
{
    if (c & change::songInfo)
    {
        refreshKeys();
        repaint();
        return;
    }
    if (c & change::view)
        refreshKeys();
    if (c & (change::playhead | change::transport))
        repaint (textArea);
}

void LyricsLane::refreshKeys()
{
    const auto& s = state();
    const bool had = syncKey.isVisible(), hadAuto = autoKey.isVisible();
    syncKey.setVisible (! s.project.lyrics.empty());
    syncKey.setToggleState (s.lyricSyncing, juce::dontSendNotification);
    autoKey.setVisible (! s.project.lyrics.empty() && s.engineAttached);   // 見本（UI_MOCK）では出さない
    autoKey.setToggleState (s.lyricsAligning, juce::dontSendNotification);
    const auto label = s.lyricsAligning ? tr ("lyrics.auto.running", juce::roundToInt (s.lyricsAlignProgress * 100.0f)) : tr ("lyrics.auto");
    if (autoKey.getButtonText() != label)
    {
        autoKey.setButtonText (label);
        if (autoKey.isVisible()) resized();
    }
    if (had != syncKey.isVisible() || hadAuto != autoKey.isVisible())
        resized();
}

void LyricsLane::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (metrics::gutter);
    r.removeFromRight (metrics::pad);

    editButton.setSize (10, 28);
    const auto w = editButton.idealWidth();
    editButton.setBounds (r.removeFromRight (w).withSizeKeepingCentre (w, 28));

    if (syncKey.isVisible())
    {
        r.removeFromRight (6);
        syncKey.setSize (10, 28);
        const auto sw = syncKey.idealWidth();
        syncKey.setBounds (r.removeFromRight (sw).withSizeKeepingCentre (sw, 28));
    }
    if (autoKey.isVisible())
    {
        r.removeFromRight (6);
        autoKey.setSize (10, 28);
        const auto aw = autoKey.idealWidth();
        autoKey.setBounds (r.removeFromRight (aw).withSizeKeepingCentre (aw, 28));
    }
    r.removeFromRight (metrics::pad);
    textArea = r.withTrimmedLeft (22);
}

void LyricsLane::paint (juce::Graphics& g)
{
    const auto& s = state();
    g.fillAll (colours::bg0);
    paint::hline (g, (float) getHeight() - 1.0f, 0.0f, (float) getWidth());
    paint::microLabel (g, getLocalBounds().withWidth (metrics::gutter).toFloat().withTrimmedLeft ((float) metrics::pad),
                       tr ("label.lyric"), colours::textMute);

    const auto& ly = s.project.lyrics;
    if (ly.empty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (13.0f));
        g.drawText (tr ("lyrics.empty"), textArea, juce::Justification::centredLeft, true);
        return;
    }

    auto r = textArea.toFloat();
    if (s.lyricSyncing)
        paintSyncing (g, r);
    else if (ly.numTimed() == 0)
        paintManual (g, r);    // 時刻が無いままでも表示する（今の行は ↑ ↓ で手送り）
    else
        paintTimed (g, r);
}

void LyricsLane::paintNext (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, float soon)
{
    const auto lf = mono (9.5f, Weight::medium, 0.12f);
    const auto lw = textWidth (lf, tr ("label.next")) + 10.0f;
    paint::microLabel (g, r.removeFromLeft (lw), tr ("label.next"), colours::textMute);
    g.setColour (colours::textDim.interpolatedWith (colours::text, juce::jlimit (0.0f, 1.0f, soon) * 0.7f));
    g.setFont (sansFor (text, 15.0f));
    g.drawText (text, r, juce::Justification::centredLeft, true);
}

void LyricsLane::paintTimed (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& s = state();
    const auto* cur = s.lyricAt (s.playhead);
    const auto* next = s.lyricAfter (s.playhead);

    if (cur != nullptr)
    {
        const auto f = sansFor (cur->text, 22.0f, Weight::semibold);
        const auto w = juce::jmin (r.getWidth() * 0.62f, textWidth (f, cur->text));
        const auto line = r.removeFromLeft (w + 2.0f).withSizeKeepingCentre (w + 2.0f, 30.0f).translated (0.0f, -3.0f);

        const auto progress = juce::jlimit (0.0f, 1.0f, (float) (s.playhead - cur->startSample)
                                                          / (float) juce::jmax ((int64) 1, cur->endSample - cur->startSample));
        const auto split = line.getX() + line.getWidth() * progress;
        const auto sung = s.isRecording ? colours::rec : colours::signal;

        g.setFont (f);
        g.setColour (colours::text);
        g.drawText (cur->text, line, juce::Justification::centredLeft, true);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (line.withRight (split).getSmallestIntegerContainer());
            g.setColour (sung);
            g.drawText (cur->text, line, juce::Justification::centredLeft, true);
        }

        const auto uy = line.getBottom() + 3.0f;
        paint::hline (g, uy, line.getX(), line.getRight(), colours::line);
        g.setColour (sung);
        g.fillRect (juce::Rectangle<float> (line.getX(), uy - 0.5f, split - line.getX(), 2.0f));

        r.removeFromLeft (36.0f);
    }
    else if (next != nullptr)
    {
        // フレーズの合間：次を大きめに待たせる
        const auto f = sansFor (next->text, 18.0f, Weight::medium);
        g.setColour (colours::textDim);
        g.setFont (f);
        const auto w = juce::jmin (r.getWidth() * 0.62f, textWidth (f, next->text));
        g.drawText (next->text, r.removeFromLeft (w + 2.0f), juce::Justification::centredLeft, true);
        r.removeFromLeft (36.0f);
        next = s.lyricAfter (next->startSample);
    }

    if (next != nullptr)
    {
        // 次の行の 1.5 秒前から少しずつ明るく（いまの行を歌い終える前に目を移せる）
        const auto lead = (double) (next->startSample - s.playhead) / juce::jmax (1, s.sampleRate());
        paintNext (g, r, next->text, cur != nullptr ? (float) (1.0 - lead / 1.5) : 0.0f);
    }
}

void LyricsLane::paintManual (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& s = state();
    const auto& lines = s.project.lyrics.lines;
    const auto i = juce::jlimit (0, (int) lines.size() - 1, s.lyricCursor);
    const auto& cur = lines[(size_t) i].text;

    const auto f = sansFor (cur, 22.0f, Weight::semibold);
    const auto w = juce::jmin (r.getWidth() * 0.55f, textWidth (f, cur));
    g.setColour (colours::text);
    g.setFont (f);
    g.drawText (cur, r.removeFromLeft (w + 2.0f).translated (0.0f, -3.0f), juce::Justification::centredLeft, true);
    r.removeFromLeft (30.0f);

    // 右端：↑ ↓ で送る（時刻はまだ）
    {
        const auto hf = sans (11.0f);
        const auto hint = tr ("lyrics.manualHint");
        auto hr = r.removeFromRight (textWidth (hf, hint) + 2.0f + 2 * 26.0f);
        auto x = hr.getX();
        x += drawKeyHint (g, { x, hr.getCentreY() }, juce::String::charToString (0x2191)) + 4.0f;   // ↑
        x += drawKeyHint (g, { x, hr.getCentreY() }, juce::String::charToString (0x2193)) + 6.0f;   // ↓
        g.setColour (colours::textMute);
        g.setFont (hf);
        g.drawText (hint, hr.withLeft (x), juce::Justification::centredLeft, false);
        r.removeFromRight (16.0f);
    }

    if (i + 1 < (int) lines.size())
        paintNext (g, r, lines[(size_t) i + 1].text);
}

void LyricsLane::paintSyncing (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& s = state();
    const auto& lines = s.project.lyrics.lines;
    const auto i = juce::jlimit (0, (int) lines.size() - 1, s.lyricCursor);

    // 左：TAP と進み具合（3 / 24）
    {
        const auto lf = mono (9.5f, Weight::medium, 0.12f);
        const auto label = tr ("lyrics.sync.micro");
        auto left = r.removeFromLeft (textWidth (lf, label) + 18.0f + 44.0f);
        paint::led (g, { left.getX() + 4.0f, left.getCentreY() - 7.0f }, 2.8f, colours::signal, true);
        paint::microLabel (g, left.withTrimmedLeft (12.0f).withHeight (left.getHeight() / 2.0f).translated (0.0f, 2.0f), label, colours::signal);
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f, Weight::medium));
        g.drawText (juce::String (i + 1) + " / " + juce::String ((int) lines.size()),
                    left.withTrimmedTop (left.getHeight() / 2.0f).withTrimmedLeft (12.0f).translated (0.0f, -3.0f),
                    juce::Justification::centredLeft, false);
        r.removeFromLeft (8.0f);
    }

    // 右：Enter 歌い出し / Backspace 1 行戻す / Esc 終わる
    {
        const auto hf = sans (11.0f);
        const juce::String keys[] = { "Enter", "Backspace", "Esc" };
        const juce::String texts[] = { tr ("lyrics.sync.hintTap"), tr ("lyrics.sync.hintBack"), tr ("lyrics.sync.hintEnd") };
        float total = 0.0f;
        for (int k = 0; k < 3; ++k)
            total += textWidth (mono (10.0f, Weight::semibold), keys[k]) + 12.0f + 5.0f + textWidth (hf, texts[k]) + 14.0f;
        auto hr = r.removeFromRight (juce::jmin (total, r.getWidth() * 0.5f));
        auto x = hr.getX();
        for (int k = 0; k < 3 && x < hr.getRight(); ++k)
        {
            x += drawKeyHint (g, { x, hr.getCentreY() }, keys[k]) + 5.0f;
            const auto tw = textWidth (hf, texts[k]);
            g.setColour (colours::textMute);
            g.setFont (hf);
            g.drawText (texts[k], juce::Rectangle<float> (x, hr.getY(), tw + 2.0f, hr.getHeight()), juce::Justification::centredLeft, false);
            x += tw + 14.0f;
        }
        r.removeFromRight (12.0f);
    }

    // 次に叩く行（大きく）と、その次（薄く）
    const auto& cur = lines[(size_t) i].text;
    const auto f = sansFor (cur, 22.0f, Weight::semibold);
    const auto w = juce::jmin (r.getWidth() * 0.62f, textWidth (f, cur));
    const auto line = r.removeFromLeft (w + 2.0f).withSizeKeepingCentre (w + 2.0f, 30.0f).translated (0.0f, -3.0f);
    g.setColour (colours::text);
    g.setFont (f);
    g.drawText (cur, line, juce::Justification::centredLeft, true);
    paint::hline (g, line.getBottom() + 3.0f, line.getX(), line.getRight(), colours::signal.withAlpha (0.6f));
    r.removeFromLeft (28.0f);

    if (i + 1 < (int) lines.size())
        paintNext (g, r, lines[(size_t) i + 1].text);
}
} // namespace vb
