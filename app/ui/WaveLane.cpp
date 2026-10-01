#include "WaveLane.h"

namespace vb
{
namespace
{
    constexpr float topPad = 6.0f;
    constexpr float compH = 18.0f;
    constexpr float bigH = 56.0f;
    constexpr float easyH = 40.0f;
    constexpr float thinH = 14.0f;
    constexpr float gap = 4.0f;
}

juce::String trackName (project::TrackType t)
{
    using project::TrackType;
    switch (t)
    {
        case TrackType::backing:     return tr ("track.backing");
        case TrackType::guide:       return tr ("track.guide");
        case TrackType::main:        return tr ("track.main");
        case TrackType::doubleTrack: return tr ("track.double");
        case TrackType::harm1:       return tr ("track.harm1");
        case TrackType::harm2:       return tr ("track.harm2");
    }
    return {};
}

int WaveLane::preferredHeight (project::Mode m)
{
    switch (m)
    {
        case project::Mode::easy:     return 86;    // 最小（Main とオフボ）
        case project::Mode::standard: return 142;
        case project::Mode::pro:      return 160;   // Harm 2 の行が増える
    }
    return 142;
}

WaveLane::WaveLane (UiSession& u) : SessionView (u)
{
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

TimeMap WaveLane::map() const
{
    return lane::makeMap (state(), plot());
}

std::vector<WaveLane::Row> WaveLane::layoutRows() const
{
    const auto& s = state();
    const bool easy = s.mode == project::Mode::easy;
    std::vector<Row> rows;
    auto r = plot();
    r.removeFromTop (topPad + (easy ? 0.0f : compH + 2.0f));

    const auto& cur = s.currentTrack();
    rows.push_back ({ cur.type, s.selectedTrack, r.removeFromTop (easy ? easyH : bigH), true });
    r.removeFromTop (gap + 2.0f);

    for (size_t i = 0; i < s.trackUi.size(); ++i)
    {
        const auto& t = s.trackUi[i];
        if ((int) i == s.selectedTrack || ! session.isTrackVisible (t.type))
            continue;

        rows.push_back ({ t.type, (int) i, r.removeFromTop (thinH), false });
        r.removeFromTop (gap);
    }

    rows.push_back ({ project::TrackType::backing, -1, r.removeFromTop (thinH), false });
    return rows;
}

//==============================================================================
void WaveLane::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < metrics::gutter) return;
    gesture.down (session, map(), e.position.x);
}

void WaveLane::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getMouseDownX() < metrics::gutter) return;
    gesture.drag (session, map(), e.position.x);
}

void WaveLane::mouseUp (const juce::MouseEvent& e)
{
    // ガターの細い行（トラック名）をクリック → そのトラックを選択
    if (e.getMouseDownX() < metrics::gutter)
    {
        for (auto& row : layoutRows())
            if (! row.current && row.trackIndex >= 0
                && juce::isPositiveAndBelow (e.position.y - (row.area.getY() - gap * 0.5f), row.area.getHeight() + gap))
                session.selectTrack (row.trackIndex);
        return;
    }
    gesture.up (session, map(), e.position.x);
}

//==============================================================================
void WaveLane::paint (juce::Graphics& g)
{
    const auto& s = state();
    const auto bounds = getLocalBounds().toFloat();
    const auto pl = plot();
    const auto m = map();
    const auto rows = layoutRows();

    g.setColour (colours::bgDeep);
    g.fillRect (pl);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (pl.getSmallestIntegerContainer());

        lane::drawTimeGrid (g, s, m, pl);
        lane::drawRange (g, s, m, pl);
        if (s.mode != project::Mode::easy)
            drawCompBar (g, m, pl.withTop (pl.getY() + topPad).withHeight (compH), rows.front().type);

        for (auto& row : rows)
            drawWave (g, m, row);

        drawRecording (g, m, rows.front());
        lane::drawPlayhead (g, s, m, pl);
    }

    // ガター（トラック名）
    const auto gut = bounds.withWidth ((float) metrics::gutter);
    g.setColour (colours::panel);
    g.fillRect (gut);
    paint::vline (g, gut.getRight() - 1.0f, gut.getY(), gut.getBottom());
    paint::hline (g, bounds.getBottom() - 1.0f, 0.0f, bounds.getRight());

    if (s.mode != project::Mode::easy)
        paint::microLabel (g, juce::Rectangle<float> (gut.getX() + (float) metrics::pad, pl.getY() + topPad, gut.getWidth(), compH),
                           tr ("label.take"), colours::textMute);

    for (auto& row : rows)
    {
        auto label = juce::Rectangle<float> (gut.getX() + (float) metrics::pad, row.area.getY(),
                                             gut.getWidth() - (float) metrics::pad - 6.0f, row.area.getHeight());
        const auto name = trackName (row.type);

        if (row.current)
        {
            g.setColour (colours::text);
            g.setFont (sans (13.0f, Weight::semibold));
            g.drawFittedText (name, label.removeFromTop (row.area.getHeight() * 0.5f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.8f);

            if (row.trackIndex >= 0 && s.trackUi[(size_t) row.trackIndex].armed)
            {
                paint::led (g, { label.getX() + 3.0f, label.getY() + 9.0f }, 2.6f, colours::rec, true);
                paint::microLabel (g, label.withTrimmedLeft (10.0f).withHeight (18.0f), tr ("label.arm"), colours::rec);
            }
        }
        else
        {
            g.setColour (colours::textDim);
            g.setFont (sans (10.5f));
            g.drawFittedText (name, label.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        }
    }
}

void WaveLane::drawCompBar (juce::Graphics& g, const TimeMap& m, juce::Rectangle<float> bar, project::TrackType type)
{
    const auto* track = state().project.findTrack (type);
    if (track == nullptr)
        return;

    if (track->comp.empty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (10.5f));
        g.drawText (tr ("wave.noTake"), bar.withTrimmedLeft (8.0f), juce::Justification::centredLeft, false);
        return;
    }

    for (auto& c : track->comp)
    {
        const auto x0 = m.x (c.startSample), x1 = m.x (c.endSample);
        if (x1 < bar.getX() || x0 > bar.getRight())
            continue;

        bool clipped = false;
        for (auto& t : track->takes)
            if (t.id == c.takeId)
                clipped = t.clip;

        const auto seg = juce::Rectangle<float> (x0, bar.getY(), x1 - x0, bar.getHeight()).reduced (1.0f, 0.0f);
        g.setColour (colours::raised);
        g.fillRoundedRectangle (seg, 2.0f);
        g.setColour (colours::signal);
        g.fillRect (seg.withWidth (2.0f));

        auto label = seg.withLeft (juce::jmax (seg.getX(), bar.getX()) + 8.0f);
        const auto lf = mono (10.5f, Weight::medium);
        const auto name = c.takeId.toUpperCase();
        g.setColour (colours::text.withAlpha (0.9f));
        g.setFont (lf);
        g.drawText (name, label, juce::Justification::centredLeft, false);

        if (clipped)
        {
            label.removeFromLeft (textWidth (lf, name) + 10.0f);
            const auto tf = mono (9.5f, Weight::semibold, 0.08f);
            const auto tw = textWidth (tf, tr ("wave.clip")) + 14.0f;
            const auto tag = label.removeFromLeft (tw).reduced (0.0f, 3.0f);
            g.setColour (colours::bad);
            g.fillRoundedRectangle (tag, 2.0f);
            g.setColour (colours::bgDeep);
            g.setFont (tf);
            g.drawText (tr ("wave.clip"), tag, juce::Justification::centred, false);
        }
    }

    // つなぎ目（クロスフェード）
    for (size_t i = 1; i < track->comp.size(); ++i)
    {
        const auto x = m.x (track->comp[i].startSample);
        if (x < bar.getX() || x > bar.getRight())
            continue;

        const auto cy = bar.getCentreY();
        juce::Path d;
        d.addQuadrilateral (x, cy - 5.0f, x + 5.0f, cy, x, cy + 5.0f, x - 5.0f, cy);
        g.setColour (colours::text);
        g.fillPath (d);
    }
}

void WaveLane::drawWave (juce::Graphics& g, const TimeMap& m, const Row& row)
{
    const auto& s = state();
    const auto a = row.area;
    const auto cy = a.getCentreY();
    const bool backing = row.type == project::TrackType::backing;

    // 未録音は斜線
    float runStart = -1.0f;
    for (float px = a.getX(); px <= a.getRight(); px += 1.0f)
    {
        const bool recorded = px < a.getRight() && dummy::isRecorded (s, row.type, m.sampleAt (px));
        if (! recorded && runStart < 0.0f) runStart = px;
        if ((recorded || px >= a.getRight()) && runStart >= 0.0f)
        {
            const auto hatch = juce::Rectangle<float> (runStart, a.getY(), px - runStart, a.getHeight());
            lane::drawHatch (g, hatch, colours::line.withAlpha (0.9f));
            if (hatch.getWidth() > 120.0f)
            {
                g.setColour (colours::textMute);
                g.setFont (sans (row.current ? 12.0f : 10.0f));
                g.drawText (tr ("wave.unrecorded"), hatch.reduced (8.0f, 0.0f), juce::Justification::centredLeft, false);
            }
            runStart = -1.0f;
        }
    }

    paint::hline (g, std::round (cy), a.getX(), a.getRight(), colours::line.withAlpha (0.5f));

    const auto base = backing ? colours::textMute.withAlpha (0.55f)
                              : (row.current ? colours::text.withAlpha (0.62f) : colours::textDim.withAlpha (0.45f));

    float clipX = -1.0f;
    for (float px = a.getX(); px < a.getRight(); px += 1.0f)
    {
        const auto s0 = m.sampleAt (px), s1 = m.sampleAt (px + 1.0f);
        float amp = 0.0f;
        if (backing)
        {
            amp = dummy::backingPeak (s, s0, s1);
        }
        else
        {
            for (int k = 0; k < 6; ++k)
                amp = juce::jmax (amp, dummy::vocalAmplitude (s, row.type, s0 + (s1 - s0) * k / 6));
        }
        if (amp <= 0.0f)
            continue;

        const bool clip = amp > 1.0f;
        const auto h = juce::jmax (1.0f, juce::jmin (1.0f, amp) * (a.getHeight() - 2.0f));
        g.setColour (clip ? colours::bad : base);
        g.fillRect (juce::Rectangle<float> (px, cy - h * 0.5f, 1.0f, h));

        if (clip && row.current && clipX < 0.0f)
            clipX = px;
    }

    if (clipX >= 0.0f)
    {
        juce::Path tri;
        tri.addTriangle (clipX - 5.0f, a.getY() - 2.0f, clipX + 5.0f, a.getY() - 2.0f, clipX, a.getY() + 5.0f);
        g.setColour (colours::bad);
        g.fillPath (tri);
    }
}

void WaveLane::drawRecording (juce::Graphics& g, const TimeMap& m, const Row& row)
{
    const auto& s = state();
    if (! s.isRecording || s.playhead <= s.recordStart)
        return;

    // 今回の録音（新しいテイク）を現在トラックの上に重ねて見せる
    const auto x0 = juce::jmax (row.area.getX(), m.x (s.recordStart));
    const auto x1 = juce::jmin (row.area.getRight(), m.x (s.playhead));
    if (x1 <= x0)
        return;

    const auto r = juce::Rectangle<float> (x0, row.area.getY(), x1 - x0, row.area.getHeight());
    g.setColour (colours::rec.withAlpha (0.12f));
    g.fillRect (r);
    g.setColour (colours::rec);
    g.fillRect (r.withHeight (2.0f));
    paint::microLabel (g, r.withHeight (16.0f).translated (6.0f, 3.0f), tr ("wave.newTake"), colours::rec);
}
} // namespace vb
