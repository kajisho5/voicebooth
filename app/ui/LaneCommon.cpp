#include "LaneCommon.h"

namespace vb::lane
{
namespace
{
    /** 目盛りの単位。テンポが分かっていれば拍と小節、分からなければ 1 秒と 5 秒 */
    int64 beatLength (const dummy::Session& s)
    {
        if (! s.tempoKnown)
            return s.sampleRate();
        return (int64) std::llround (60.0 / s.bpm() * s.sampleRate());
    }

    int beatsPerBar (const dummy::Session& s)
    {
        return s.tempoKnown ? s.beatsPerBar : 5;
    }

    template <typename Fn>
    void forEachBeat (const dummy::Session& s, Fn&& fn)
    {
        const auto beat = beatLength (s);
        const auto perBar = beatsPerBar (s);
        for (auto b = (s.viewStart / beat) * beat; b <= s.viewEnd; b += beat)
            if (b >= s.viewStart)
                fn (b, (b / beat) % perBar == 0, b / beat);
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
    if (! s.hasRange())
        return;

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
    // 位置は丸めない（小数の座標でなめらかに進む。DESIGN 4.10.1 PH）
    const auto x = map.x (s.playhead);
    const auto c = playheadColour (s);

    // REC 中は短い赤い尾
    if (s.isRecording)
    {
        constexpr float tail = 70.0f;
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.0f), x - tail, 0.0f, c.withAlpha (0.16f), x, 0.0f, false));
        g.fillRect (juce::Rectangle<float> (x - tail, area.getY(), tail, area.getHeight()).getIntersection (area));
    }

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
    if (s.hasRange())
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
            g.drawText (s.tempoKnown ? juce::String (beatIndex / s.beatsPerBar + 1)
                                     : formatTime (b, s.sampleRate(), false),
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

        const auto name = m.kind == project::MarkerKind::chorus ? tr ("marker.chorus") : m.name;
        const auto f = sans (10.5f, Weight::semibold);
        const auto w = textWidth (f, name) + 12.0f;
        const auto tag = juce::Rectangle<float> (x + 26.0f, r.getY() + 4.0f, w, r.getHeight() - 11.0f);
        g.setColour (colours::signal);
        g.fillRoundedRectangle (tag, 2.0f);
        g.setColour (colours::onFill (colours::signal));
        g.setFont (f);
        g.drawText (name, tag, juce::Justification::centred, false);
    }

    // 再生ヘッドの頭
    const auto px = map.x (s.playhead);
    juce::Path head;
    head.addTriangle (px - 5.5f, r.getY() + 3.0f, px + 5.5f, r.getY() + 3.0f, px, r.getBottom() - 3.0f);
    g.setColour (playheadColour (s));
    g.fillPath (head);
}

void RangeGesture::down (UiSession&, const TimeMap& map, float x)
{
    startX = x;
    startSample = map.sampleAt (x);
    dragging = false;
}

void RangeGesture::drag (UiSession& session, const TimeMap& map, float x)
{
    if (! dragging && std::abs (x - startX) < 4.0f)
        return;   // 4px 未満はクリック扱い

    dragging = true;
    session.setRange (startSample, map.sampleAt (juce::jlimit (map.x0, map.x1, x)));
}

void RangeGesture::up (UiSession& session, const TimeMap& map, float x)
{
    if (! dragging)
        session.seek (map.sampleAt (x));
    dragging = false;
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
