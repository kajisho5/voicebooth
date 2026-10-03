#include "LaneCommon.h"
#include "SongMarks.h"

namespace vb::lane
{
namespace
{
    /** 目盛りの線を順に渡す：fn (位置, 小節線か, 添える文字)
        テンポが分かっていれば 1 小節目の位置から拍と小節（狭ければ拍線を省き、小節番号を間引く）。
        分からなければ 1 秒と 5 秒（m:ss） */
    template <typename Fn>
    void forEachTick (const dummy::Session& s, const TimeMap& map, Fn&& fn)
    {
        const auto sr = s.sampleRate();
        const auto pxPerSample = (double) (map.x1 - map.x0) / (double) juce::jmax ((int64) 1, map.end - map.start);

        if (! s.tempoKnown())
        {
            const int64 sec = sr;
            for (auto b = (s.viewStart / sec) * sec; b <= s.viewEnd; b += sec)
                if (b >= s.viewStart)
                {
                    const bool five = (b / sec) % 5 == 0;
                    fn (b, five, five ? formatTime (b, sr, false) : juce::String());
                }
            return;
        }

        const auto& t = s.project.tempo;
        const auto spb = t.samplesPerBeat (sr);
        const auto perBar = s.beatsPerBar();
        const bool beatLines = spb * pxPerSample >= 5.0;
        int labelEvery = 1;
        while (spb * perBar * labelEvery * pxPerSample < 30.0 && labelEvery < 256)
            labelEvery *= 2;

        for (auto k = song::beatIndexAt (t, s.viewStart, sr);; ++k)
        {
            const auto b = song::beatSample (t, k, sr);
            if (b > s.viewEnd)
                break;
            if (b < s.viewStart)
                continue;

            const auto bb = song::barBeatAt (t, b, sr);
            const bool barLine = bb.beat == 1;
            if (! barLine && ! beatLines)
                continue;
            const bool labelled = barLine && ((bb.bar - 1) % labelEvery + labelEvery) % labelEvery == 0;
            fn (b, barLine, labelled ? juce::String (bb.bar) : juce::String());
        }
    }

    /** 区間の札の色（選んでいる / 確定 / 推定） */
    struct TagLook { juce::Colour fill, outline, text, line; };

    TagLook tagLook (const dummy::Session& s, int index)
    {
        const auto& sec = s.project.sections[(size_t) index];
        if (index == s.selectedSection)
            return { colours::signal, colours::signal, colours::onFill (colours::signal), colours::signal };
        if (sec.source == song::Source::confirmed)
            return { colours::raisedHi, colours::lineHi, colours::text, colours::textDim };
        // 推定は薄く（DESIGN 7.5：触ったら確定）
        return { colours::bgDeep, colours::line, colours::textDim, colours::textMute };
    }
}

juce::Colour playheadColour (const dummy::Session& s)
{
    return s.isRecording ? colours::rec : colours::signal;
}

void wheel (UiSession& session, const TimeMap& map, const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        // ホイール 1 刻み（deltaY 0.25 前後）で約 1.4 倍
        if (! juce::approximatelyEqual (w.deltaY, 0.0f))
            session.zoomView (map.sampleAt (e.position.x), std::pow (2.0, -(double) w.deltaY * 2.0));
        return;
    }

    // レーンは縦に送る物が無いので、縦ホイールも横送りにする（下へ回す＝先へ）
    const auto dx = ! juce::approximatelyEqual (w.deltaX, 0.0f) ? -w.deltaX : -w.deltaY;
    if (! juce::approximatelyEqual (dx, 0.0f))
        session.scrollView ((double) dx * 0.4);
}

void magnify (UiSession& session, const TimeMap& map, const juce::MouseEvent& e, float scaleFactor)
{
    if (scaleFactor > 0.0f)
        session.zoomView (map.sampleAt (e.position.x), 1.0 / (double) scaleFactor);
}

TimeMap makeMap (const dummy::Session& s, juce::Rectangle<float> plot)
{
    return { s.viewStart, s.viewEnd, plot.getX(), plot.getRight() };
}

void drawTimeGrid (juce::Graphics& g, const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> area)
{
    forEachTick (s, map, [&] (int64 b, bool barLine, const juce::String&)
    {
        const auto x = std::round (map.x (b));
        g.setColour (barLine ? colours::line.withAlpha (0.75f) : colours::grid.withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (x, area.getY(), 1.0f, area.getHeight()));
    });

    // 区間の頭（ピッチ・波形を縦に通す）
    const auto& list = s.project.sections;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto x = std::round (map.x (list[(size_t) i].startSample));
        if (x < area.getX() || x > area.getRight())
            continue;
        g.setColour (i == s.selectedSection ? colours::signal.withAlpha (0.5f) : colours::lineHi.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (x, area.getY(), 1.0f, area.getHeight()));
    }
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
    forEachTick (s, map, [&] (int64 b, bool barLine, const juce::String& label)
    {
        const auto x = std::round (map.x (b));
        const auto tickH = barLine ? 9.0f : 4.0f;
        g.setColour (barLine ? colours::textMute : colours::line);
        g.fillRect (juce::Rectangle<float> (x, r.getBottom() - 1.0f - tickH, 1.0f, tickH));

        if (label.isNotEmpty())
        {
            g.setColour (colours::textDim);
            g.drawText (label, juce::Rectangle<float> (x + 5.0f, r.getY() + 2.0f, 40.0f, r.getHeight() - 8.0f),
                        juce::Justification::centredLeft, false);
        }
    });

    // 区間の札（イントロ・Aメロ・サビ…）。小節番号の上に重ねる
    const auto f = sans (10.5f, Weight::semibold);
    for (auto& tag : sectionTags (s, map, r))
    {
        const auto look = tagLook (s, tag.index);
        const auto x = std::round (map.x (s.project.sections[(size_t) tag.index].startSample));
        g.setColour (look.line);
        g.fillRect (juce::Rectangle<float> (x, r.getY(), 1.0f, r.getHeight() - 1.0f));

        g.setColour (look.fill);
        g.fillRoundedRectangle (tag.area, 2.0f);
        g.setColour (look.outline);
        g.drawRoundedRectangle (tag.area.reduced (0.5f), 2.0f, 1.0f);
        g.setColour (look.text);
        g.setFont (f);
        g.drawText (marks::sectionName (s.project.sections, tag.index), tag.area.reduced (5.0f, 0.0f),
                    juce::Justification::centredLeft, true);
    }

    // 再生ヘッドの頭
    const auto px = map.x (s.playhead);
    juce::Path head;
    head.addTriangle (px - 5.5f, r.getY() + 3.0f, px + 5.5f, r.getY() + 3.0f, px, r.getBottom() - 3.0f);
    g.setColour (playheadColour (s));
    g.fillPath (head);
}

std::vector<SectionTag> sectionTags (const dummy::Session& s, const TimeMap& map, juce::Rectangle<float> r)
{
    std::vector<SectionTag> tags;
    const auto& list = s.project.sections;
    const auto f = sans (10.5f, Weight::semibold);

    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto x = std::round (map.x (list[(size_t) i].startSample));
        if (x < r.getX() - 1.0f || x > r.getRight())
            continue;

        // 次の頭の手前まで（狭ければ名前を省略する）
        auto right = r.getRight();
        if (i + 1 < (int) list.size())
            right = juce::jmin (right, std::round (map.x (list[(size_t) i + 1].startSample)) - 2.0f);

        const auto w = juce::jmin (textWidth (f, marks::sectionName (list, i)) + 12.0f, right - (x + 1.0f));
        if (w < 10.0f)
            continue;
        tags.push_back ({ i, juce::Rectangle<float> (x + 1.0f, r.getY() + 3.0f, w, r.getHeight() - 8.0f) });
    }
    return tags;
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
