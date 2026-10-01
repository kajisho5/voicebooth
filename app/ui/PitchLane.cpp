#include "PitchLane.h"
#include "LaneCommon.h"

namespace vb
{
namespace
{
    constexpr int rulerH = 24;
    constexpr int footerH = 34;
    constexpr float minConfidence = 0.5f;
    constexpr int64 maxGapSamples = 720;   // 15 ms 以上空いたら線を切る

    bool isBlackKey (int midi)
    {
        const auto pc = ((midi % 12) + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }
}

PitchLane::PitchLane (const dummy::Session& s) : session (s)
{
    octaveAlign.setToggleState (s.octaveAlign, juce::dontSendNotification);
    fullRange.setToggleState (s.fullRange, juce::dontSendNotification);

    for (auto* b : { &octaveAlign, &octaveUp, &fullRange })
    {
        b->setFontSize (11.0f);
        b->setSubtle (true);
        addAndMakeVisible (b);
    }
}

void PitchLane::resized()
{
    auto r = getLocalBounds();
    rulerArea = r.removeFromTop (rulerH);
    footerArea = r.removeFromBottom (footerH);
    gutterArea = r.removeFromLeft (metrics::gutter);
    plotArea = r;

    auto f = footerArea.reduced (metrics::pad, 0);
    auto place = [&f] (ChipButton& b)
    {
        const auto w = b.idealWidth();
        b.setBounds (f.removeFromRight (w).withSizeKeepingCentre (w, 24));
        f.removeFromRight (6);
    };
    place (fullRange);
    place (octaveUp);
    place (octaveAlign);
    legendArea = f;
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
    if (a <= session.pitchToleranceCents) return colours::accent;
    if (a <= 50.0f)                       return colours::warn;
    return colours::bad;
}

void PitchLane::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);

    const auto plot = plotArea.toFloat();
    const auto map = lane::makeMap (session, plot);

    // ルーラー（ガター部分は空き）
    g.setColour (colours::panel);
    g.fillRect (rulerArea.withWidth (metrics::gutter));
    lane::drawRuler (g, session, map, rulerArea.withTrimmedLeft (metrics::gutter).toFloat());

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plotArea);

        drawGrid (g, map);
        lane::drawRange (g, session, map, plot);
        drawReference (g, map);
        drawMine (g, map);
        lane::drawPlayhead (g, session, map, plot);
        drawCurrentDot (g, map);
    }

    drawKeyboard (g);
    drawFooter (g);
}

void PitchLane::drawGrid (juce::Graphics& g, const TimeMap& map)
{
    const auto plot = plotArea.toFloat();

    for (int m = session.lowMidi; m <= session.highMidi; ++m)
    {
        const auto y0 = yForMidi ((float) m + 0.5f), y1 = yForMidi ((float) m - 0.5f);
        if (isBlackKey (m))
        {
            g.setColour (colours::bgDeep);
            g.fillRect (juce::Rectangle<float> (plot.getX(), y0, plot.getWidth(), y1 - y0));
        }

        if (m % 12 == 0)
        {
            g.setColour (colours::grid.brighter (0.2f));
            g.fillRect (juce::Rectangle<float> (plot.getX(), std::round (y1), plot.getWidth(), 1.0f));
        }
    }

    lane::drawTimeGrid (g, session, map, plot);
}

void PitchLane::drawKeyboard (juce::Graphics& g)
{
    const auto r = gutterArea.toFloat();
    g.setColour (colours::panel);
    g.fillRect (r);

    // 現在の自分の音（ハイライト用）
    int currentMidi = -1;
    if (! session.myPitch.empty())
        currentMidi = (int) std::lround (session.myPitch.back().midi);

    const auto keyX = r.getRight() - 22.0f;

    for (int m = session.lowMidi; m <= session.highMidi; ++m)
    {
        const auto y0 = yForMidi ((float) m + 0.5f), y1 = yForMidi ((float) m - 0.5f);
        const auto row = juce::Rectangle<float> (keyX, y0, 22.0f, y1 - y0);

        if (m == currentMidi)
        {
            g.setColour (colours::accent.withAlpha (0.22f));
            g.fillRect (juce::Rectangle<float> (r.getX(), y0, r.getWidth(), y1 - y0));
        }

        g.setColour (isBlackKey (m) ? colours::bgDeep : colours::grid.brighter (0.25f));
        g.fillRect (isBlackKey (m) ? row.withTrimmedRight (6.0f) : row);

        if (m % 12 == 0 || m == currentMidi)
        {
            g.setColour (m == currentMidi ? colours::accent : colours::textDim);
            g.setFont (font (11.0f, FontWeight::bold));
            g.drawText (dummy::noteName ((float) m), juce::Rectangle<float> (r.getX() + 8.0f, (y0 + y1) * 0.5f - 7.0f, keyX - r.getX() - 12.0f, 14.0f),
                        juce::Justification::centredRight, false);
        }
    }

    g.setColour (colours::border);
    g.fillRect (juce::Rectangle<float> (r.getRight() - 1.0f, r.getY(), 1.0f, r.getHeight()));
}

void PitchLane::drawReference (juce::Graphics& g, const TimeMap& map)
{
    juce::Path path;
    bool open = false;
    int64 last = 0;

    for (auto& p : session.refPitch)
    {
        if (p.sample < session.viewStart - 4800 || p.sample > session.viewEnd + 4800)
            continue;

        const bool usable = p.confidence >= minConfidence;
        if (! usable) { open = false; continue; }

        const juce::Point<float> pt { map.x (p.sample), yForMidi (p.midi) };
        if (! open || p.sample - last > maxGapSamples) path.startNewSubPath (pt);
        else                                           path.lineTo (pt);

        open = true;
        last = p.sample;
    }

    const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
    g.setColour (colours::refPitch.withAlpha (0.14f));
    g.strokePath (path, stroke (10.0f));
    g.setColour (colours::refPitch.withAlpha (0.92f));
    g.strokePath (path, stroke (4.0f));
}

void PitchLane::drawMine (juce::Graphics& g, const TimeMap& map)
{
    const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };

    juce::Path path;
    juce::Colour colour;
    bool open = false;
    int64 last = 0;
    juce::Point<float> lastPt;

    auto flush = [&]
    {
        if (path.isEmpty()) return;
        g.setColour (colour.withAlpha (0.16f));
        g.strokePath (path, stroke (9.0f));
        g.setColour (colour);
        g.strokePath (path, stroke (3.0f));
        path.clear();
    };

    for (auto& p : session.myPitch)
    {
        if (p.sample < session.viewStart - 4800 || p.sample > session.viewEnd)
            continue;

        if (p.confidence < minConfidence) { flush(); open = false; continue; }

        const juce::Point<float> pt { map.x (p.sample), yForMidi (p.midi) };
        const auto c = colourForCents (p.centsOff);
        const bool continuous = open && p.sample - last <= maxGapSamples;

        if (! continuous)
        {
            flush();
            colour = c;
            path.startNewSubPath (pt);
        }
        else if (c != colour)
        {
            path.lineTo (pt);      // 境界点は両方の色で共有して途切れなく見せる
            flush();
            colour = c;
            path.startNewSubPath (pt);
        }
        else
        {
            path.lineTo (pt);
        }

        open = true;
        last = p.sample;
        lastPt = pt;
    }
    flush();
}

void PitchLane::drawCurrentDot (juce::Graphics& g, const TimeMap& map)
{
    if (session.myPitch.empty())
        return;

    const auto& p = session.myPitch.back();
    const juce::Point<float> c { map.x (session.playhead), yForMidi (p.midi) };
    const auto col = colourForCents (p.centsOff);

    g.setColour (col.withAlpha (0.18f));
    g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (c));
    g.setColour (col);
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (c));
    g.setColour (colours::bg0);
    g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (c), 2.0f);

    // セント値はプロのみ（DESIGN 4.3）
    if (session.mode == project::Mode::pro)
    {
        const auto cents = juce::roundToInt (p.centsOff);
        const auto txt = (cents >= 0 ? "+" : "") + juce::String (cents) + " cent";
        g.setFont (font (12.0f, FontWeight::bold));
        g.setColour (col);
        g.drawText (txt, juce::Rectangle<float> (c.x + 14.0f, c.y - 22.0f, 80.0f, 16.0f), juce::Justification::centredLeft, false);
    }
}

void PitchLane::drawFooter (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRect (footerArea);
    g.setColour (colours::border);
    g.fillRect (footerArea.withHeight (1));

    auto r = legendArea;
    g.setFont (font (11.0f, FontWeight::bold));

    auto swatch = [&] (juce::Colour c, const juce::String& label, bool line)
    {
        auto s = r.removeFromLeft (line ? 22 : 12).toFloat();
        g.setColour (c);
        if (line) g.fillRoundedRectangle (s.withSizeKeepingCentre (18.0f, 4.0f), 2.0f);
        else      g.fillEllipse (s.withSizeKeepingCentre (8.0f, 8.0f));
        r.removeFromLeft (6);
        g.setColour (colours::textDim);
        const auto w = (int) textWidth (font (11.0f, FontWeight::bold), label) + 2;
        g.drawText (label, r.removeFromLeft (w), juce::Justification::centredLeft, false);
        r.removeFromLeft (16);
    };

    swatch (colours::refPitch, jp ("お手本"), true);
    swatch (colours::accent, jp ("自分"), true);
    r.removeFromLeft (10);
    swatch (colours::accent, jp ("±") + juce::String ((int) session.pitchToleranceCents) + "c", false);
    swatch (colours::warn, jp ("±50c"), false);
    swatch (colours::bad, jp ("外れ"), false);
}
} // namespace vb
