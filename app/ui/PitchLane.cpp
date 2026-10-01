#include "PitchLane.h"
#include "LaneCommon.h"

namespace vb
{
namespace
{
    constexpr int rulerH = 24;
    constexpr int footerH = 38;
    constexpr float minConfidence = 0.5f;
    constexpr int64 maxGapSamples = 720;   // 15 ms 以上空いたら線を切る（嘘でつながない）

    bool isBlackKey (int midi)
    {
        const auto pc = ((midi % 12) + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    /** 信頼できる点だけを、途切れごとの区間に分ける */
    template <typename Fn>
    void forEachRun (const std::vector<dummy::PitchPoint>& pts, int64 from, int64 to, Fn&& fn)
    {
        std::vector<const dummy::PitchPoint*> run;
        int64 last = 0;

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

PitchLane::PitchLane (const dummy::Session& s) : session (s)
{
    octaveAlign.setToggleState (s.octaveAlign, juce::dontSendNotification);
    fullRange.setToggleState (s.fullRange, juce::dontSendNotification);

    for (auto* b : { &octaveAlign, &octaveUp, &fullRange })
    {
        b->withLed().withFont (sans (11.5f, Weight::medium));
        addAndMakeVisible (b);
    }
    octaveUp.setTooltip (jp ("自分の声を1オクターブ上げて重ねる"));
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
    legendArea = f.withTrimmedLeft (metrics::gutter - metrics::pad);
}

float PitchLane::yForMidi (float midi) const
{
    const auto p = plotArea.toFloat();
    const auto k = (midi - ((float) session.lowMidi - 0.5f)) / (float) (session.highMidi - session.lowMidi + 1);
    return p.getBottom() - k * p.getHeight();
}

juce::Colour PitchLane::colourForCents (float cents) const
{
    const auto a = std::abs (cents);
    if (a <= session.pitchToleranceCents) return colours::signal;
    if (a <= 50.0f)                       return colours::warn;
    return colours::bad;
}

void PitchLane::paint (juce::Graphics& g)
{
    const auto plot = plotArea.toFloat();
    const auto map = lane::makeMap (session, plot);

    g.setColour (colours::panel);
    g.fillRect (rulerArea.withWidth (metrics::gutter));
    paint::hline (g, (float) rulerArea.getBottom() - 1.0f, 0.0f, (float) metrics::gutter);
    lane::drawRuler (g, session, map, rulerArea.withTrimmedLeft (metrics::gutter).toFloat());

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plotArea);

        drawBackground (g, map);
        lane::drawRange (g, session, map, plot);
        drawReference (g, map);
        drawMine (g, map);
        lane::drawPlayhead (g, session, map, plot);
        drawCurrent (g, map);
    }

    drawNoteGutter (g);
    drawFooter (g);
}

void PitchLane::drawBackground (juce::Graphics& g, const TimeMap& map)
{
    const auto plot = plotArea.toFloat();
    g.setColour (colours::bgDeep);
    g.fillRect (plot);

    for (int m = session.lowMidi; m <= session.highMidi; ++m)
    {
        const auto y0 = yForMidi ((float) m + 0.5f), y1 = yForMidi ((float) m - 0.5f);
        if (! isBlackKey (m))
        {
            g.setColour (juce::Colours::white.withAlpha (0.012f));
            g.fillRect (juce::Rectangle<float> (plot.getX(), y0, plot.getWidth(), y1 - y0));
        }
        if (m % 12 == 0)
            paint::hline (g, std::round (y1), plot.getX(), plot.getRight(), colours::line.withAlpha (0.8f));
    }

    lane::drawTimeGrid (g, session, map, plot);
}

void PitchLane::drawNoteGutter (juce::Graphics& g)
{
    const auto r = gutterArea.toFloat();
    g.setColour (colours::panel);
    g.fillRect (r);
    paint::vline (g, r.getRight() - 1.0f, r.getY(), r.getBottom());

    const int current = session.myPitch.empty() ? -1 : (int) std::lround (session.myPitch.back().midi);

    for (int m = session.lowMidi; m <= session.highMidi; ++m)
    {
        const auto yc = yForMidi ((float) m);
        const auto rowH = yForMidi ((float) m - 0.5f) - yForMidi ((float) m + 0.5f);

        // 音高の目盛り（黒鍵は短く）
        g.setColour (isBlackKey (m) ? colours::line : colours::lineHi);
        const auto len = m % 12 == 0 ? 10.0f : (isBlackKey (m) ? 3.0f : 6.0f);
        g.fillRect (juce::Rectangle<float> (r.getRight() - 1.0f - len, std::round (yc), len, 1.0f));

        if (m == current)
        {
            const auto pill = juce::Rectangle<float> (r.getX() + 8.0f, yc - 8.5f, r.getWidth() - 22.0f, 17.0f);
            g.setColour (colours::signal);
            g.fillRoundedRectangle (pill, 3.0f);
            g.setColour (colours::bgDeep);
            g.setFont (mono (11.0f, Weight::semibold));
            g.drawText (dummy::noteName ((float) m), pill, juce::Justification::centred, false);
        }
        else if (m % 12 == 0 && rowH > 0.0f)
        {
            g.setColour (colours::textDim);
            g.setFont (mono (10.5f, Weight::medium));
            g.drawText (dummy::noteName ((float) m), juce::Rectangle<float> (r.getX() + 8.0f, yc - 7.0f, r.getWidth() - 22.0f, 14.0f),
                        juce::Justification::centredLeft, false);
        }
    }
}

void PitchLane::drawReference (juce::Graphics& g, const TimeMap& map)
{
    const auto halfBand = session.pitchToleranceCents / 100.0f;

    forEachRun (session.refPitch, session.viewStart - 4800, session.viewEnd + 4800,
                [&] (const std::vector<const dummy::PitchPoint*>& run)
    {
        // 許容帯：上辺を左→右、下辺を右→左でつないだ多角形
        juce::Path band, centre;
        for (size_t i = 0; i < run.size(); ++i)
        {
            const auto x = map.x (run[i]->sample);
            const auto y = yForMidi (run[i]->midi + halfBand);
            if (i == 0) band.startNewSubPath (x, y); else band.lineTo (x, y);
        }
        for (size_t i = run.size(); i-- > 0;)
            band.lineTo (map.x (run[i]->sample), yForMidi (run[i]->midi - halfBand));
        band.closeSubPath();

        for (size_t i = 0; i < run.size(); ++i)
        {
            const juce::Point<float> pt { map.x (run[i]->sample), yForMidi (run[i]->midi) };
            if (i == 0) centre.startNewSubPath (pt); else centre.lineTo (pt);
        }

        g.setColour (colours::ref.withAlpha (0.24f));
        g.fillPath (band);
        g.setColour (colours::ref.withAlpha (0.9f));
        g.strokePath (centre, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    });
}

void PitchLane::drawMine (juce::Graphics& g, const TimeMap& map)
{
    const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };

    forEachRun (session.myPitch, session.viewStart - 4800, session.viewEnd,
                [&] (const std::vector<const dummy::PitchPoint*>& run)
    {
        // 同じ色の連続ごとに描く（境界点は両側で共有して途切れなく見せる）
        juce::Path path;
        auto colour = colourForCents (run.front()->centsOff);

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
            const juce::Point<float> pt { map.x (run[i]->sample), yForMidi (run[i]->midi) };
            const auto c = colourForCents (run[i]->centsOff);

            if (i == 0)                { path.startNewSubPath (pt); continue; }
            path.lineTo (pt);
            if (c != colour)           { flush(); colour = c; path.startNewSubPath (pt); }
        }
        flush();
    });
}

void PitchLane::drawCurrent (juce::Graphics& g, const TimeMap& map)
{
    if (session.myPitch.empty())
        return;

    const auto& p = session.myPitch.back();
    const juce::Point<float> c { map.x (session.playhead), yForMidi (p.midi) };
    const auto col = colourForCents (p.centsOff);

    g.setColour (col.withAlpha (0.16f));
    g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (c));
    g.setColour (colours::bgDeep);
    g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c));
    g.setColour (col);
    g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c), 2.5f);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));

    // セント値はプロのみ（DESIGN 4.3）
    if (session.mode == project::Mode::pro)
    {
        const auto cents = juce::roundToInt (p.centsOff);
        const auto txt = (cents >= 0 ? "+" : "") + juce::String (cents) + " cent";
        g.setFont (mono (11.0f, Weight::semibold));
        g.setColour (col);
        g.drawText (txt, juce::Rectangle<float> (c.x + 14.0f, c.y - 22.0f, 90.0f, 16.0f), juce::Justification::centredLeft, false);
    }
}

void PitchLane::drawFooter (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRect (footerArea);
    paint::hline (g, (float) footerArea.getY(), 0.0f, (float) getWidth());

    paint::microLabel (g, footerArea.withWidth (metrics::gutter).toFloat().withTrimmedLeft ((float) metrics::pad),
                       "PITCH", colours::textMute);

    auto r = legendArea.toFloat();
    const auto lf = sans (11.0f);

    auto label = [&] (const juce::String& s, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (lf);
        const auto w = textWidth (lf, s) + 2.0f;
        g.drawText (s, r.removeFromLeft (w), juce::Justification::centredLeft, false);
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
        label (jp ("お手本（±") + juce::String ((int) session.pitchToleranceCents) + jp ("c の帯）"), colours::textDim);
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
        label (jp ("自分（合う / ±50c / 外れ）"), colours::textDim);
    }
}
} // namespace vb
