#include "LaneCommon.h"

namespace vb::lane
{
namespace
{
    int64 beatLength (const dummy::Session& s)
    {
        return (int64) std::llround (60.0 / s.bpm() * s.sampleRate());
    }

    template <typename Fn>
    void forEachBeat (const dummy::Session& s, Fn&& fn)
    {
        const auto beat = beatLength (s);
        for (auto b = (s.viewStart / beat) * beat; b <= s.viewEnd; b += beat)
            if (b >= s.viewStart)
                fn (b, (b / beat) % s.beatsPerBar == 0, b / beat);
    }
}

juce::Colour playheadColour (const dummy::Session& s)
{
    return s.isRecording ? colours::rec : colours::signal;
}

TimeMap makeMap (const dummy::Session& s, juce::Rectangle<float> plot)
{
    return { s.viewStart, s.viewEnd, plot.getX(), plot.getRight() };
}

void drawTimeGrid (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> area)
{
    forEachBeat (s, [&] (int64 b, bool barLine, int64)
    {
        const auto x = std::round (map.x (b));
        g.setColour (barLine ? colours::line.withAlpha (0.75f) : colours::grid.withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (x, area.getY(), 1.0f, area.getHeight()));
    });
}

void drawRange (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> area)
{
    const auto x0 = juce::jmax (area.getX(), map.x (s.rangeIn));
    const auto x1 = juce::jmin (area.getRight(), map.x (s.rangeOut));
    if (x1 <= x0)
        return;

    g.setColour (colours::signal.withAlpha (s.loopOn ? 0.045f : 0.02f));
    g.fillRect (juce::Rectangle<float> (x0, area.getY(), x1 - x0, area.getHeight()));

    const float dashes[] = { 3.0f, 4.0f };
    g.setColour (colours::signal.withAlpha (0.45f));
    for (auto smp : { s.rangeIn, s.rangeOut })
    {
        const auto x = std::round (map.x (smp)) + 0.5f;
        if (x >= area.getX() && x <= area.getRight())
            g.drawDashedLine ({ x, area.getY(), x, area.getBottom() }, dashes, 2, 1.0f);
    }
}

void drawPlayhead (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> area)
{
    const auto x = map.x (s.playhead);
    const auto c = playheadColour (s);
    g.setColour (c.withAlpha (0.12f));
    g.fillRect (juce::Rectangle<float> (x - 3.0f, area.getY(), 6.0f, area.getHeight()));
    g.setColour (c);
    g.fillRect (juce::Rectangle<float> (x - 0.75f, area.getY(), 1.5f, area.getHeight()));
}

void drawRuler (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> r)
{
    g.setColour (colours::panel);
    g.fillRect (r);
    paint::hline (g, r.getBottom() - 1.0f, r.getX(), r.getRight());

    // ループ範囲（ルーラー下端の帯）
    {
        const auto x0 = juce::jmax (r.getX(), map.x (s.rangeIn));
        const auto x1 = juce::jmin (r.getRight(), map.x (s.rangeOut));
        if (x1 > x0)
        {
            g.setColour (colours::signal.withAlpha (s.loopOn ? 0.55f : 0.2f));
            g.fillRect (juce::Rectangle<float> (x0, r.getBottom() - 4.0f, x1 - x0, 3.0f));
        }
    }

    g.setFont (mono (10.5f, Weight::medium));
    forEachBeat (s, [&] (int64 b, bool barLine, int64 beatIndex)
    {
        const auto x = std::round (map.x (b));
        const auto tickH = barLine ? 9.0f : 4.0f;
        g.setColour (barLine ? colours::textMute : colours::line);
        g.fillRect (juce::Rectangle<float> (x, r.getBottom() - 1.0f - tickH, 1.0f, tickH));

        if (barLine)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (beatIndex / s.beatsPerBar + 1),
                        juce::Rectangle<float> (x + 5.0f, r.getY() + 2.0f, 40.0f, r.getHeight() - 8.0f),
                        juce::Justification::centredLeft, false);
        }
    });

    // マーカー（サビ等）
    for (auto& m : s.project.markers)
    {
        const auto x = map.x (m.sample);
        if (x < r.getX() || x > r.getRight())
            continue;

        const auto f = sans (10.5f, Weight::semibold);
        const auto w = textWidth (f, m.name) + 12.0f;
        const auto tag = juce::Rectangle<float> (x + 26.0f, r.getY() + 4.0f, w, r.getHeight() - 11.0f);
        g.setColour (colours::signal);
        g.fillRoundedRectangle (tag, 2.0f);
        g.setColour (colours::bgDeep);
        g.setFont (f);
        g.drawText (m.name, tag, juce::Justification::centred, false);
    }

    // 再生ヘッドの頭
    const auto px = map.x (s.playhead);
    juce::Path head;
    head.addTriangle (px - 5.5f, r.getY() + 3.0f, px + 5.5f, r.getY() + 3.0f, px, r.getBottom() - 3.0f);
    g.setColour (playheadColour (s));
    g.fillPath (head);
}

void drawHatch (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.getSmallestIntegerContainer());
    g.setColour (c);
    for (auto x = area.getX() - area.getHeight(); x < area.getRight(); x += 6.0f)
        g.drawLine (x, area.getBottom(), x + area.getHeight(), area.getY(), 1.0f);
}
} // namespace vb::lane
