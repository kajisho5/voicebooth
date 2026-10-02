#include "PitchLane.h"
#include "SongMarks.h"

namespace vb
{
namespace
{
    constexpr int rulerH = 24;
    constexpr int footerH = 38;
    constexpr float minConfidence = 0.5f;
    constexpr double maxGapSeconds = 0.015;   // 15 ms 以上空いたら線を切る（嘘でつながない）
    constexpr float harmonyOffset = 4.0f;  // ダミーのハモリ（長 3 度上）

    bool isBlackKey (int midi)
    {
        const auto pc = ((midi % 12) + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    /** 信頼できる点だけを、途切れごとの区間に分ける */
    template <typename Fn>
    void forEachRun (const std::vector<dummy::PitchPoint>& pts, int64 from, int64 to, double sampleRate, Fn&& fn)
    {
        std::vector<const dummy::PitchPoint*> run;
        int64 last = 0;
        const auto maxGapSamples = (int64) (maxGapSeconds * sampleRate);

        auto flush = [&] { if (run.size() > 1) fn (run); run.clear(); };

        for (auto& p : pts)
        {
            if (p.sample < from || p.sample > to) continue;
            if (p.confidence < minConfidence) { flush(); continue; }
            if (! run.empty() && p.sample - last > maxGapSamples) flush();
            run.push_back (&p);
            last = p.sample;
        }
        flush();
    }
}

PitchLane::PitchLane (UiSession& u, Actions& a)
    : SessionView (u), actions (a),
      octaveAlign (tr ("pitch.octaveAlign")),
      octaveUp (tr ("pitch.octaveUp")),
      fullRange (tr ("pitch.fullRange"))
{
    for (auto* b : { &octaveAlign, &octaveUp, &fullRange })
    {
        b->withLed().withToggle (false).withFont (sans (11.5f, Weight::medium));
        addAndMakeVisible (b);
    }
    octaveAlign.setTooltip (tr ("pitch.octaveAlign.tooltip"));
    octaveUp.setTooltip (tr ("pitch.octaveUp.tooltip"));
    fullRange.setTooltip (tr ("pitch.fullRange.tooltip"));

    octaveAlign.onClick = [this] { session.setOctaveAlign (! state().octaveAlign); };
    octaveUp.onClick    = [this] { session.setOctaveUp (! state().octaveUp); };
    fullRange.onClick   = [this] { session.setFullRange (! state().fullRange); };

    onSessionChanged (change::all);
}

void PitchLane::onSessionChanged (juce::uint32 changes)
{
    const auto& s = state();
    octaveAlign.setToggleState (s.octaveAlign, juce::dontSendNotification);
    octaveUp.setToggleState (s.octaveUp, juce::dontSendNotification);
    fullRange.setToggleState (s.fullRange, juce::dontSendNotification);

    if (changes & change::mode)
        resized();

    repaint();
}

void PitchLane::resized()
{
    auto r = getLocalBounds();
    rulerArea = r.removeFromTop (rulerH);
    footerArea = r.removeFromBottom (footerH);
    gutterArea = r.removeFromLeft (metrics::gutter);
    plotArea = r;

    auto f = footerArea.reduced (metrics::pad, 0);
    for (auto* b : { &fullRange, &octaveUp, &octaveAlign })
    {
        b->setSize (10, 26);
        const auto w = b->idealWidth();
        b->setBounds (f.removeFromRight (w).withSizeKeepingCentre (w, 26));
        f.removeFromRight (6);
    }
    f.removeFromRight (10);
    analysisArea = f.removeFromRight (state().mode == project::Mode::pro ? 300 : (state().mode == project::Mode::standard ? 140 : 0));
    legendArea = f.withTrimmedLeft (metrics::gutter - metrics::pad);
}

TimeMap PitchLane::map() const
{
    return lane::makeMap (state(), plotArea.toFloat());
}

float PitchLane::yForMidi (float midi) const
{
    const auto& s = state();
    const auto p = plotArea.toFloat();
    const auto k = (midi - ((float) s.lowMidi - 0.5f)) / (float) (s.highMidi - s.lowMidi + 1);
    return p.getBottom() - k * p.getHeight();
}

juce::Colour PitchLane::colourForCents (float cents) const
{
    const auto a = std::abs (cents);
    if (a <= state().pitchToleranceCents) return colours::signal;
    if (a <= 50.0f)                       return colours::warn;
    return colours::bad;
}

juce::Colour PitchLane::colourFor (const dummy::PitchPoint& p) const
{
    // お手本がまだ無い（B9 の前）：合っている・外れているは言えないので中立の色
    return p.judged ? colourForCents (p.centsOff) : (juce::Colour) colours::text;
}

float PitchLane::refOffset() const  { return state().isHarmonySelected() ? harmonyOffset : 0.0f; }
float PitchLane::mineOffset() const { return refOffset() + (state().octaveUp ? 12.0f : 0.0f); }

//==============================================================================
int PitchLane::tagAt (juce::Point<float> p) const
{
    for (auto& t : lane::sectionTags (state(), map(), rulerArea.withTrimmedLeft (metrics::gutter).toFloat()))
        if (t.area.expanded (0.0f, 2.0f).contains (p))
            return t.index;
    return -1;
}

void PitchLane::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (tagAt (e.position) >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void PitchLane::mouseDown (const juce::MouseEvent& e)
{
    draggingTag = -1;
    draggingRuler = false;

    if (rulerArea.contains (e.getPosition()))
    {
        const auto tag = tagAt (e.position);
        if (e.mods.isPopupMenu())
        {
            const auto screen = juce::Rectangle<int> (e.getScreenX(), e.getScreenY(), 1, 1);
            if (tag >= 0)
                marks::showSectionMenu (session, actions, tag, screen);
            else if (e.x >= plotArea.getX())
                marks::showRulerMenu (session, actions, map().sampleAt (e.position.x), ! e.mods.isAltDown(), screen);
            return;
        }

        if (tag >= 0)
        {
            draggingTag = tag;
            tagMoved = false;
            tagGrabOffset = e.position.x - map().x (state().project.sections[(size_t) tag].startSample);
            session.selectSection (tag);
            return;
        }

        session.selectSection (-1);
        draggingRuler = true;
        session.seek (map().sampleAt ((float) juce::jmax (plotArea.getX(), e.x)));
    }
    else if (plotArea.contains (e.getPosition()))
        gesture.down (session, map(), e.position.x);
}

void PitchLane::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingTag >= 0)
    {
        if (! tagMoved && e.getDistanceFromDragStartX() * e.getDistanceFromDragStartX() < 9)
            return;   // 3px 未満はクリック扱い
        tagMoved = true;
        const auto x = juce::jlimit ((float) plotArea.getX(), (float) plotArea.getRight(), e.position.x - tagGrabOffset);
        draggingTag = session.moveSection (draggingTag, map().sampleAt (x), ! e.mods.isAltDown());
        return;
    }

    if (draggingRuler)
        session.seek (map().sampleAt ((float) juce::jlimit (plotArea.getX(), plotArea.getRight(), e.x)));
    else if (plotArea.contains (e.getMouseDownPosition()))
        gesture.drag (session, map(), e.position.x);
}

void PitchLane::mouseUp (const juce::MouseEvent& e)
{
    if (draggingTag >= 0)
    {
        if (! tagMoved)
            session.goToSection (draggingTag);   // 札をクリック：その区間の頭へ（「サビへ」）
        draggingTag = -1;
        return;
    }

    if (! draggingRuler && plotArea.contains (e.getMouseDownPosition()))
        gesture.up (session, map(), e.position.x);
    draggingRuler = false;
}

void PitchLane::mouseDoubleClick (const juce::MouseEvent& e)
{
    // 札をダブルクリック：名前を選ぶ（一覧 / 自由入力）
    const auto tag = tagAt (e.position);
    if (tag >= 0)
        marks::showNameMenu (session, actions, tag, { e.getScreenX(), e.getScreenY(), 1, 1 });
}

//==============================================================================
void PitchLane::paint (juce::Graphics& g)
{
    const auto& s = state();
    const auto plot = plotArea.toFloat();
    const auto m = map();

    g.setColour (colours::panel);
    g.fillRect (rulerArea.withWidth (metrics::gutter));
    paint::hline (g, (float) rulerArea.getBottom() - 1.0f, 0.0f, (float) metrics::gutter);
    lane::drawRuler (g, s, m, rulerArea.withTrimmedLeft (metrics::gutter).toFloat());

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plotArea);

        drawBackground (g, m);
        lane::drawRange (g, s, m, plot);
        if (s.isHarmonySelected())
            drawMainGhost (g, m);
        drawReference (g, m);
        drawMine (g, m);

        // 開いたばかりの曲：お手本ピッチはまだない（B9 で解析）
        if (s.refPitch.empty())
        {
            g.setColour (colours::textMute);
            g.setFont (sans (13.0f));
            g.drawText (tr ("pitch.notAnalyzed"), plot.reduced (24.0f), juce::Justification::centred, false);
        }
        lane::drawPlayhead (g, s, m, plot);
        drawCurrent (g, m);
    }

    drawNoteGutter (g);
    drawFooter (g);
}

void PitchLane::drawBackground (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    const auto plot = plotArea.toFloat();
    g.setColour (colours::bgDeep);
    g.fillRect (plot);

    for (int n = s.lowMidi; n <= s.highMidi; ++n)
    {
        const auto y0 = yForMidi ((float) n + 0.5f), y1 = yForMidi ((float) n - 0.5f);
        if (! isBlackKey (n))
        {
            g.setColour (colours::highlight (0.012f));
            g.fillRect (juce::Rectangle<float> (plot.getX(), y0, plot.getWidth(), y1 - y0));
        }
        if (n % 12 == 0)
            paint::hline (g, std::round (y1), plot.getX(), plot.getRight(), colours::line.withAlpha (0.8f));
    }

    lane::drawTimeGrid (g, s, m, plot);
}

void PitchLane::drawNoteGutter (juce::Graphics& g)
{
    const auto& s = state();
    const auto r = gutterArea.toFloat();
    g.setColour (colours::panel);
    g.fillRect (r);
    paint::vline (g, r.getRight() - 1.0f, r.getY(), r.getBottom());

    int current = -1;
    if (auto* p = dummy::myPitchAt (s, s.playhead))
        current = (int) std::lround (p->midi + mineOffset());

    for (int n = s.lowMidi; n <= s.highMidi; ++n)
    {
        const auto yc = yForMidi ((float) n);

        g.setColour (isBlackKey (n) ? colours::line : colours::lineHi);
        const auto len = n % 12 == 0 ? 10.0f : (isBlackKey (n) ? 3.0f : 6.0f);
        g.fillRect (juce::Rectangle<float> (r.getRight() - 1.0f - len, std::round (yc), len, 1.0f));

        if (n == current)
        {
            const auto pill = juce::Rectangle<float> (r.getX() + 8.0f, yc - 8.5f, r.getWidth() - 22.0f, 17.0f);
            g.setColour (colours::signal);
            g.fillRoundedRectangle (pill, 3.0f);
            g.setColour (colours::onFill (colours::signal));
            g.setFont (mono (11.0f, Weight::semibold));
            g.drawText (dummy::noteName ((float) n), pill, juce::Justification::centred, false);
        }
        else if (n % 12 == 0)
        {
            g.setColour (colours::textDim);
            g.setFont (mono (10.5f, Weight::medium));
            g.drawText (dummy::noteName ((float) n), juce::Rectangle<float> (r.getX() + 8.0f, yc - 7.0f, r.getWidth() - 22.0f, 14.0f),
                        juce::Justification::centredLeft, false);
        }
    }
}

void PitchLane::drawMainGhost (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    forEachRun (s.refPitch, s.viewStart - 4800, s.viewEnd + 4800, s.sampleRate(), [&] (const std::vector<const dummy::PitchPoint*>& run)
    {
        juce::Path p;
        for (size_t i = 0; i < run.size(); ++i)
        {
            const juce::Point<float> pt { m.x (run[i]->sample), yForMidi (run[i]->midi) };
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (colours::text.withAlpha (0.28f));
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    });
}

void PitchLane::drawReference (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    const auto halfBand = s.pitchToleranceCents / 100.0f;
    const auto off = refOffset();

    forEachRun (s.refPitch, s.viewStart - 4800, s.viewEnd + 4800, s.sampleRate(), [&] (const std::vector<const dummy::PitchPoint*>& run)
    {
        // 許容帯：上辺を左→右、下辺を右→左でつないだ多角形
        juce::Path band, centre;
        for (size_t i = 0; i < run.size(); ++i)
        {
            const auto x = m.x (run[i]->sample);
            const auto y = yForMidi (run[i]->midi + off + halfBand);
            if (i == 0) band.startNewSubPath (x, y); else band.lineTo (x, y);
        }
        for (size_t i = run.size(); i-- > 0;)
            band.lineTo (m.x (run[i]->sample), yForMidi (run[i]->midi + off - halfBand));
        band.closeSubPath();

        for (size_t i = 0; i < run.size(); ++i)
        {
            const juce::Point<float> pt { m.x (run[i]->sample), yForMidi (run[i]->midi + off) };
            if (i == 0) centre.startNewSubPath (pt); else centre.lineTo (pt);
        }

        g.setColour (colours::ref.withAlpha (0.24f));
        g.fillPath (band);
        g.setColour (colours::ref.withAlpha (0.9f));
        g.strokePath (centre, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    });
}

void PitchLane::drawMine (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    const auto off = mineOffset();
    const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };

    // 自分の線は「歌ったところ」＝再生ヘッドまで
    forEachRun (s.myPitch, s.viewStart - 4800, s.playhead, s.sampleRate(), [&] (const std::vector<const dummy::PitchPoint*>& run)
    {
        // 同じ色の連続ごとに描く（境界点は両側で共有して途切れなく見せる）
        juce::Path path;
        auto colour = colourFor (*run.front());

        auto flush = [&]
        {
            g.setColour (colour.withAlpha (0.16f));
            g.strokePath (path, stroke (7.0f));
            g.setColour (colour);
            g.strokePath (path, stroke (2.6f));
            path.clear();
        };

        for (size_t i = 0; i < run.size(); ++i)
        {
            const juce::Point<float> pt { m.x (run[i]->sample), yForMidi (run[i]->midi + off) };
            const auto c = colourFor (*run[i]);

            if (i == 0)       { path.startNewSubPath (pt); continue; }
            path.lineTo (pt);
            if (c != colour)  { flush(); colour = c; path.startNewSubPath (pt); }
        }
        flush();
    });
}

void PitchLane::drawCurrent (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    const auto* p = dummy::myPitchAt (s, s.playhead);
    if (p == nullptr)
        return;   // 無音・子音では出さない

    const juce::Point<float> c { m.x (s.playhead), yForMidi (p->midi + mineOffset()) };
    const auto col = colourFor (*p);

    g.setColour (col.withAlpha (0.16f));
    g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (c));
    g.setColour (colours::bgDeep);
    g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c));
    g.setColour (col);
    g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c), 2.5f);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));

    // セント値はプロのみ（DESIGN 4.3）。お手本が無ければ出さない
    if (s.mode == project::Mode::pro && p->judged)
    {
        const auto cents = juce::roundToInt (p->centsOff);
        g.setFont (mono (11.0f, Weight::semibold));
        g.setColour (col);
        g.drawText (tr ("pitch.cents", (cents >= 0 ? "+" : "") + juce::String (cents)),
                    juce::Rectangle<float> (c.x + 14.0f, c.y - 22.0f, 90.0f, 16.0f), juce::Justification::centredLeft, false);
    }
}

void PitchLane::drawFooter (juce::Graphics& g)
{
    const auto& s = state();
    g.setColour (colours::panel);
    g.fillRect (footerArea);
    paint::hline (g, (float) footerArea.getY(), 0.0f, (float) getWidth());

    paint::microLabel (g, footerArea.withWidth (metrics::gutter).toFloat().withTrimmedLeft ((float) metrics::pad),
                       tr ("label.pitch"), colours::textMute);

    auto r = legendArea.toFloat();
    const auto lf = sans (11.0f);

    auto label = [&] (const juce::String& text, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (lf);
        const auto w = juce::jmin (r.getWidth(), textWidth (lf, text) + 2.0f);
        g.drawText (text, r.removeFromLeft (w), juce::Justification::centredLeft, true);
        r.removeFromLeft (16.0f);
    };

    // お手本：帯
    {
        auto sw = r.removeFromLeft (26.0f).withSizeKeepingCentre (26.0f, 8.0f);
        g.setColour (colours::ref.withAlpha (0.22f));
        g.fillRect (sw);
        g.setColour (colours::ref.withAlpha (0.85f));
        g.fillRect (sw.withSizeKeepingCentre (sw.getWidth(), 1.4f));
        r.removeFromLeft (6.0f);
        label (s.isHarmonySelected() ? tr ("pitch.legend.refHarmony", (int) s.pitchToleranceCents)
                                     : tr ("pitch.legend.ref", (int) s.pitchToleranceCents), colours::textDim);
    }

    // 自分：3 状態
    {
        const juce::Colour cs[] = { colours::signal, colours::warn, colours::bad };
        for (auto c : cs)
        {
            g.setColour (c);
            g.fillRoundedRectangle (r.removeFromLeft (12.0f).withSizeKeepingCentre (12.0f, 3.0f), 1.5f);
            r.removeFromLeft (2.0f);
        }
        r.removeFromLeft (6.0f);
        label (tr ("pitch.legend.mine"), colours::textDim);
    }

    if (s.isHarmonySelected() && r.getWidth() > 60.0f)
    {
        g.setColour (colours::text.withAlpha (0.35f));
        g.fillRoundedRectangle (r.removeFromLeft (18.0f).withSizeKeepingCentre (18.0f, 2.0f), 1.0f);
        r.removeFromLeft (6.0f);
        label (tr ("pitch.legend.mainGhost"), colours::textDim);
    }

    // 入りタイミング（標準以上）/ 解析（プロ）— ダミー値。歌っていない（自分のピッチが無い）曲では出さない
    // 入りの早さ・ビブラートはまだ見本の値（解析は B18）。実際の声（お手本と比べていない点）では出さない
    if (! analysisArea.isEmpty() && ! s.myPitch.empty() && s.myPitch.front().judged)
    {
        auto a = analysisArea.toFloat();
        auto chip = [&] (const juce::String& name, const juce::String& value, juce::Colour c)
        {
            const auto lf2 = mono (9.5f, Weight::medium, 0.12f);
            const auto vf = mono (11.0f, Weight::semibold);
            const auto w = textWidth (lf2, name) + textWidth (vf, value) + 22.0f;
            auto box = a.removeFromLeft (w).withSizeKeepingCentre (w, 24.0f);
            a.removeFromLeft (6.0f);
            paint::inset (g, box);
            box.reduce (8.0f, 0.0f);
            paint::microLabel (g, box.removeFromLeft (textWidth (lf2, name) + 6.0f), name, colours::textMute);
            g.setColour (c);
            g.setFont (vf);
            g.drawText (value, box, juce::Justification::centredLeft, false);
        };

        chip (tr ("analysis.onset"), tr ("analysis.onset.value", "+40"), colours::warn);
        if (s.mode == project::Mode::pro)
            chip (tr ("analysis.vibrato"), tr ("analysis.vibrato.value", "5.5", "28"), colours::text);
    }
}
} // namespace vb
