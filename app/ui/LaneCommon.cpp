#include "LaneCommon.h"

namespace vb::lane
{
namespace
{
    int64 beatLength (const dummy::Session& s)
    {
        return (int64) std::llround (60.0 / s.bpm() * s.sampleRate());
    }
}

TimeMap makeMap (const dummy::Session& s, juce::Rectangle<float> plot)
{
    return { s.viewStart, s.viewEnd, plot.getX(), plot.getRight() };
}

void drawTimeGrid (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> area)
{
    const auto beat = beatLength (s);
    for (auto b = (s.viewStart / beat) * beat; b <= s.viewEnd; b += beat)
    {
        if (b < s.viewStart)
            continue;

        const auto x = map.x (b);
        const bool barLine = (b / beat) % s.beatsPerBar == 0;
        g.setColour (barLine ? colours::grid.brighter (0.15f) : colours::grid.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (std::round (x), area.getY(), 1.0f, area.getHeight()));
    }
}

void drawRange (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> area)
{
    const auto x0 = juce::jmax (area.getX(), map.x (s.rangeIn));
    const auto x1 = juce::jmin (area.getRight(), map.x (s.rangeOut));
    if (x1 <= x0)
        return;

    g.setColour (colours::accent.withAlpha (s.loopOn ? 0.06f : 0.03f));
    g.fillRect (juce::Rectangle<float> (x0, area.getY(), x1 - x0, area.getHeight()));

    const float dashes[] = { 4.0f, 4.0f };
    g.setColour (colours::accent.withAlpha (0.55f));
    for (auto smp : { s.rangeIn, s.rangeOut })
    {
        const auto x = map.x (smp);
        if (x >= area.getX() && x <= area.getRight())
            g.drawDashedLine ({ x, area.getY(), x, area.getBottom() }, dashes, 2, 1.0f);
    }
}

void drawPlayhead (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> area)
{
    const auto x = map.x (s.playhead);
    const auto c = s.isRecording ? colours::rec : colours::accent;
    g.setColour (c.withAlpha (0.18f));
    g.fillRect (juce::Rectangle<float> (x - 3.0f, area.getY(), 6.0f, area.getHeight()));
    g.setColour (c);
    g.fillRect (juce::Rectangle<float> (x - 1.0f, area.getY(), 2.0f, area.getHeight()));
}

void drawRuler (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> r)
{
    g.setColour (colours::panel);
    g.fillRect (r);
    g.setColour (colours::border);
    g.fillRect (r.withTop (r.getBottom() - 1.0f));

    // ループ範囲バー
    {
        const auto x0 = juce::jmax (r.getX(), map.x (s.rangeIn));
        const auto x1 = juce::jmin (r.getRight(), map.x (s.rangeOut));
        if (x1 > x0)
        {
            g.setColour (colours::accent.withAlpha (s.loopOn ? 0.35f : 0.15f));
            g.fillRect (juce::Rectangle<float> (x0, r.getY(), x1 - x0, 4.0f));
        }
    }

    const auto beat = beatLength (s);
    g.setFont (font (11.0f, FontWeight::bold));

    for (auto b = (s.viewStart / beat) * beat; b <= s.viewEnd; b += beat)
    {
        if (b < s.viewStart)
            continue;

        const auto x = std::round (map.x (b));
        const auto beatIndex = b / beat;
        const bool barLine = beatIndex % s.beatsPerBar == 0;

        g.setColour (barLine ? colours::textDim : colours::textMute);
        const auto tickH = barLine ? 8.0f : 4.0f;
        g.fillRect (juce::Rectangle<float> (x, r.getBottom() - tickH, 1.0f, tickH));

        if (barLine)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (beatIndex / s.beatsPerBar + 1), juce::Rectangle<float> (x + 4.0f, r.getY() + 3.0f, 40.0f, r.getHeight() - 6.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    // マーカー（サビ）
    for (auto& m : s.project.markers)
    {
        const auto x = map.x (m.sample);
        if (x < r.getX() || x > r.getRight())
            continue;

        const auto f = font (11.0f, FontWeight::bold);
        const auto w = textWidth (f, m.name) + 14.0f;
        const auto tag = juce::Rectangle<float> (x + 26.0f, r.getY() + 3.0f, w, r.getHeight() - 6.0f);
        g.setColour (colours::accent.withAlpha (0.16f));
        g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (colours::accent);
        g.setFont (f);
        g.drawText (m.name, tag, juce::Justification::centred, false);
    }

    // 再生ヘッドの頭
    const auto px = map.x (s.playhead);
    juce::Path head;
    head.addTriangle (px - 6.0f, r.getY() + 2.0f, px + 6.0f, r.getY() + 2.0f, px, r.getBottom() - 2.0f);
    g.setColour (s.isRecording ? colours::rec : colours::accent);
    g.fillPath (head);
}

void drawHatch (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.getSmallestIntegerContainer());
    g.setColour (c);
    const auto step = 7.0f;
    for (auto x = area.getX() - area.getHeight(); x < area.getRight(); x += step)
        g.drawLine (x, area.getBottom(), x + area.getHeight(), area.getY(), 1.0f);
}
} // namespace vb::lane
