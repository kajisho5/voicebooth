#include "WaveLane.h"
#include "LaneCommon.h"

namespace vb
{
namespace
{
    constexpr float compH = 20.0f;
    constexpr float bigH = 54.0f;
    constexpr float thinH = 14.0f;
    constexpr float gap = 4.0f;
}

WaveLane::WaveLane (const dummy::Session& s) : session (s) {}

std::vector<WaveLane::Row> WaveLane::layoutRows() const
{
    std::vector<Row> rows;
    auto r = getLocalBounds().toFloat().withTrimmedLeft ((float) metrics::gutter);
    r.removeFromTop (6.0f + compH + 2.0f);

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

    rows.push_back ({ project::TrackType::backing, "Backing", r.removeFromTop (thinH), false });
    return rows;
}

void WaveLane::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);

    const auto bounds = getLocalBounds().toFloat();
    const auto plot = bounds.withTrimmedLeft ((float) metrics::gutter);
    const auto map = lane::makeMap (session, plot);
    const auto rows = layoutRows();

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.getSmallestIntegerContainer());

        lane::drawTimeGrid (g, session, map, plot);
        lane::drawRange (g, session, map, plot);

        drawCompBar (g, map, plot.withTop (plot.getY() + 6.0f).withHeight (compH), rows.front().type);

        for (auto& row : rows)
            drawWave (g, map, row);

        lane::drawPlayhead (g, session, map, plot);
    }

    // ガター（トラック名）
    const auto gut = bounds.withWidth ((float) metrics::gutter);
    g.setColour (colours::panel);
    g.fillRect (gut);
    g.setColour (colours::border);
    g.fillRect (gut.withLeft (gut.getRight() - 1.0f));
    g.fillRect (bounds.withTop (bounds.getBottom() - 1.0f));

    for (auto& row : rows)
    {
        auto label = juce::Rectangle<float> (gut.getX() + 8.0f, row.area.getY(), gut.getWidth() - 16.0f, row.area.getHeight());
        g.setFont (font (row.current ? 13.0f : 10.0f, row.current ? FontWeight::bold : FontWeight::regular));
        g.setColour (row.current ? colours::text : colours::textDim);

        if (row.current)
        {
            g.drawText (row.name, label.removeFromTop (row.area.getHeight() * 0.5f), juce::Justification::bottomRight, false);
            if (session.currentTrack().armed)
            {
                g.setColour (colours::rec);
                g.setFont (font (10.0f, FontWeight::bold));
                g.drawText (jp ("● ARM"), label, juce::Justification::topRight, false);
            }
        }
        else
        {
            g.drawText (row.name, label, juce::Justification::centredRight, false);
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
        g.setColour (colours::accent.withAlpha (0.10f));
        g.fillRoundedRectangle (seg, 3.0f);
        g.setColour (colours::accent.withAlpha (0.35f));
        g.drawRoundedRectangle (seg.reduced (0.5f), 3.0f, 1.0f);

        // 見えている範囲の左端にラベル
        auto label = seg.withLeft (juce::jmax (seg.getX(), bar.getX()) + 8.0f);
        g.setFont (font (11.0f, FontWeight::bold));
        g.setColour (colours::accent);
        const auto name = jp ("採用 ") + c.takeId;
        g.drawText (name, label, juce::Justification::centredLeft, false);

        if (clipped)
        {
            label.removeFromLeft (textWidth (font (11.0f, FontWeight::bold), name) + 10.0f);
            const auto tag = label.removeFromLeft (64.0f).reduced (0.0f, 3.0f);
            g.setColour (colours::bad.withAlpha (0.18f));
            g.fillRoundedRectangle (tag, 3.0f);
            g.setColour (colours::bad);
            g.setFont (font (10.0f, FontWeight::bold));
            g.drawText (jp ("クリップあり"), tag, juce::Justification::centred, false);
        }
    }

    // つなぎ目（クロスフェード 8 ms）
    for (size_t i = 1; i < track->comp.size(); ++i)
    {
        const auto x = map.x (track->comp[i].startSample);
        if (x < bar.getX() || x > bar.getRight())
            continue;

        juce::Path d;
        const auto cy = bar.getCentreY();
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

    g.setColour (row.current ? colours::bgDeep : colours::bgDeep.withAlpha (0.7f));
    g.fillRoundedRectangle (a, 3.0f);

    // 未録音は斜線
    float runStart = -1.0f;
    for (float px = a.getX(); px <= a.getRight(); px += 1.0f)
    {
        const bool rec = px < a.getRight() && dummy::isRecorded (session, row.type, map.sampleAt (px));
        if (! rec && runStart < 0.0f) runStart = px;
        if ((rec || px >= a.getRight()) && runStart >= 0.0f)
        {
            const auto hatch = juce::Rectangle<float> (runStart, a.getY(), px - runStart, a.getHeight());
            lane::drawHatch (g, hatch, colours::textMute.withAlpha (0.35f));
            if (hatch.getWidth() > 120.0f && ! row.current)
            {
                g.setColour (colours::textMute);
                g.setFont (font (10.0f));
                g.drawText (jp ("未録音"), hatch.reduced (8.0f, 0.0f), juce::Justification::centredLeft, false);
            }
            runStart = -1.0f;
        }
    }

    const auto base = backing ? colours::textDim.withAlpha (0.45f)
                              : (row.current ? colours::accent.withAlpha (0.78f) : colours::accent.withAlpha (0.35f));

    std::vector<float> clipXs;
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

        if (clip && row.current)
            clipXs.push_back (px);
    }

    if (! clipXs.empty())
    {
        // クリップ位置の目印（画面全体は赤くしない）
        const auto x = clipXs.front();
        juce::Path tri;
        tri.addTriangle (x - 5.0f, a.getY() - 1.0f, x + 5.0f, a.getY() - 1.0f, x, a.getY() + 6.0f);
        g.setColour (colours::bad);
        g.fillPath (tri);
    }

    if (row.current)
    {
        g.setColour (colours::accent.withAlpha (0.25f));
        g.drawRoundedRectangle (a.reduced (0.5f), 3.0f, 1.0f);
    }
}
} // namespace vb
