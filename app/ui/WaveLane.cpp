#include "WaveLane.h"
#include "LaneCommon.h"

namespace vb
{
namespace
{
    constexpr float topPad = 6.0f;
    constexpr float compH = 18.0f;
    constexpr float bigH = 56.0f;
    constexpr float thinH = 14.0f;
    constexpr float gap = 4.0f;
}

WaveLane::WaveLane (const dummy::Session& s) : session (s) {}

std::vector<WaveLane::Row> WaveLane::layoutRows() const
{
    std::vector<Row> rows;
    auto r = getLocalBounds().toFloat().withTrimmedLeft ((float) metrics::gutter);
    r.removeFromTop (topPad + compH + 2.0f);

    const auto& cur = session.currentTrack();
    rows.push_back ({ cur.type, cur.name, r.removeFromTop (bigH), true });
    r.removeFromTop (gap + 2.0f);

    for (auto& t : session.trackUi)
    {
        if (t.type == cur.type)
            continue;
        if (t.type == project::TrackType::harm2 && session.mode != project::Mode::pro)
            continue;   // Harm 2 はプロのみ表示（データは消さない）

        rows.push_back ({ t.type, t.name, r.removeFromTop (thinH), false });
        r.removeFromTop (gap);
    }

    rows.push_back ({ project::TrackType::backing, jp ("オフボ"), r.removeFromTop (thinH), false });
    return rows;
}

void WaveLane::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = bounds.withTrimmedLeft ((float) metrics::gutter);
    const auto map = lane::makeMap (session, plot);
    const auto rows = layoutRows();

    g.setColour (colours::bgDeep);
    g.fillRect (plot);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.getSmallestIntegerContainer());

        lane::drawTimeGrid (g, session, map, plot);
        lane::drawRange (g, session, map, plot);
        drawCompBar (g, map, plot.withTop (plot.getY() + topPad).withHeight (compH), rows.front().type);

        for (auto& row : rows)
            drawWave (g, map, row);

        lane::drawPlayhead (g, session, map, plot);
    }

    // ガター（トラック名）
    const auto gut = bounds.withWidth ((float) metrics::gutter);
    g.setColour (colours::panel);
    g.fillRect (gut);
    paint::vline (g, gut.getRight() - 1.0f, gut.getY(), gut.getBottom());
    paint::hline (g, bounds.getBottom() - 1.0f, 0.0f, bounds.getRight());

    paint::microLabel (g, juce::Rectangle<float> (gut.getX() + (float) metrics::pad, plot.getY() + topPad, gut.getWidth(), compH),
                       "TAKE", colours::textMute);

    for (auto& row : rows)
    {
        auto label = juce::Rectangle<float> (gut.getX() + (float) metrics::pad, row.area.getY(), gut.getWidth() - (float) metrics::pad - 8.0f, row.area.getHeight());

        if (row.current)
        {
            g.setColour (colours::text);
            g.setFont (sans (13.0f, Weight::semibold));
            g.drawText (row.name, label.removeFromTop (row.area.getHeight() * 0.5f), juce::Justification::bottomLeft, false);

            if (session.currentTrack().armed)
            {
                paint::led (g, { label.getX() + 3.0f, label.getY() + 9.0f }, 2.6f, colours::rec, true);
                paint::microLabel (g, label.withTrimmedLeft (10.0f).withHeight (18.0f), "ARM", colours::rec);
            }
        }
        else
        {
            g.setColour (colours::textDim);
            g.setFont (sans (10.5f));
            g.drawText (row.name, label, juce::Justification::centredLeft, false);
        }
    }
}

void WaveLane::drawCompBar (juce::Graphics& g, const TimeMap& map, juce::Rectangle<float> bar, project::TrackType type)
{
    const auto* track = session.project.findTrack (type);
    if (track == nullptr)
        return;

    for (auto& c : track->comp)
    {
        const auto x0 = map.x (c.startSample), x1 = map.x (c.endSample);
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
            const auto tag = label.removeFromLeft (44.0f).reduced (0.0f, 3.0f);
            g.setColour (colours::bad);
            g.fillRoundedRectangle (tag, 2.0f);
            g.setColour (colours::bgDeep);
            g.setFont (mono (9.5f, Weight::semibold, 0.08f));
            g.drawText ("CLIP", tag, juce::Justification::centred, false);
        }
    }

    // つなぎ目（クロスフェード）
    for (size_t i = 1; i < track->comp.size(); ++i)
    {
        const auto x = map.x (track->comp[i].startSample);
        if (x < bar.getX() || x > bar.getRight())
            continue;

        const auto cy = bar.getCentreY();
        juce::Path d;
        d.addQuadrilateral (x, cy - 5.0f, x + 5.0f, cy, x, cy + 5.0f, x - 5.0f, cy);
        g.setColour (colours::text);
        g.fillPath (d);
    }
}

void WaveLane::drawWave (juce::Graphics& g, const TimeMap& map, const Row& row)
{
    const auto a = row.area;
    const auto cy = a.getCentreY();
    const bool backing = row.type == project::TrackType::backing;

    // 未録音は斜線
    float runStart = -1.0f;
    for (float px = a.getX(); px <= a.getRight(); px += 1.0f)
    {
        const bool recorded = px < a.getRight() && dummy::isRecorded (session, row.type, map.sampleAt (px));
        if (! recorded && runStart < 0.0f) runStart = px;
        if ((recorded || px >= a.getRight()) && runStart >= 0.0f)
        {
            const auto hatch = juce::Rectangle<float> (runStart, a.getY(), px - runStart, a.getHeight());
            lane::drawHatch (g, hatch, colours::line.withAlpha (0.9f));
            if (hatch.getWidth() > 120.0f && ! row.current)
            {
                g.setColour (colours::textMute);
                g.setFont (sans (10.0f));
                g.drawText (jp ("未録音"), hatch.reduced (8.0f, 0.0f), juce::Justification::centredLeft, false);
            }
            runStart = -1.0f;
        }
    }

    // 中心線
    paint::hline (g, std::round (cy), a.getX(), a.getRight(), colours::line.withAlpha (0.5f));

    const auto base = backing ? colours::textMute.withAlpha (0.55f)
                              : (row.current ? colours::text.withAlpha (0.62f) : colours::textDim.withAlpha (0.45f));

    float clipX = -1.0f;
    for (float px = a.getX(); px < a.getRight(); px += 1.0f)
    {
        const auto s0 = map.sampleAt (px), s1 = map.sampleAt (px + 1.0f);
        float amp = 0.0f;
        for (int k = 0; k < 6; ++k)
        {
            const auto smp = s0 + (s1 - s0) * k / 6;
            amp = juce::jmax (amp, backing ? dummy::backingAmplitude (session, smp)
                                           : dummy::vocalAmplitude (session, row.type, smp));
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

    // クリップ位置の目印（画面全体は赤くしない）
    if (clipX >= 0.0f)
    {
        juce::Path tri;
        tri.addTriangle (clipX - 5.0f, a.getY() - 2.0f, clipX + 5.0f, a.getY() - 2.0f, clipX, a.getY() + 5.0f);
        g.setColour (colours::bad);
        g.fillPath (tri);
    }
}
} // namespace vb
