#include "LedMeter.h"

namespace vb
{
LedMeter::LedMeter (Style s) : style (s) {}

void LedMeter::setLevels (float p, float r, float h, bool c)
{
    if (juce::approximatelyEqual (p, peakDb) && juce::approximatelyEqual (r, rmsDb)
        && juce::approximatelyEqual (h, holdDb) && c == clipped)
        return;
    peakDb = p; rmsDb = r; holdDb = h; clipped = c;
    repaint();
}

void LedMeter::mouseEnter (const juce::MouseEvent&)
{
    setMouseCursor (onClick ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void LedMeter::mouseUp (const juce::MouseEvent& e)
{
    if (onClick && e.mouseWasClicked())
        onClick();
}

juce::Colour LedMeter::zoneColour (float db) const
{
    if (db > -3.0f) return colours::bad;
    if (db > targetHigh) return colours::warn;
    return colours::signal;
}

float LedMeter::dbToX (float db, juce::Rectangle<float> bar) const
{
    const auto k = juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb);
    return bar.getX() + k * bar.getWidth();
}

void LedMeter::paint (juce::Graphics& g)
{
    const bool full = style == Style::full;
    auto area = getLocalBounds().toFloat();

    // クリップ LED は右端に独立
    auto clipArea = area.removeFromRight (full ? 16.0f : 9.0f);
    area.removeFromRight (4.0f);

    const auto barH = full ? 14.0f : area.getHeight();
    auto bar = full ? area.withHeight (barH).withY (area.getY() + 7.0f) : area;

    paint::inset (g, bar.expanded (2.0f), 2.0f);

    // セグメント
    const auto segW = full ? 4.0f : 3.0f, gap = full ? 1.5f : 1.0f;
    const int n = (int) std::floor ((bar.getWidth() + gap) / (segW + gap));
    const auto used = (float) n * (segW + gap) - gap;
    const auto x0 = bar.getX() + (bar.getWidth() - used) * 0.5f;

    int holdIndex = -1;
    for (int i = 0; i < n && holdDb > minDb; ++i)   // 無音（下限）ではホールドを出さない
    {
        const auto dbTop = minDb + (float) (i + 1) / (float) n * -minDb;
        if (holdIndex < 0 && dbTop >= holdDb) holdIndex = i;
    }

    for (int i = 0; i < n; ++i)
    {
        const auto dbLo = minDb + (float) i / (float) n * -minDb;
        const auto dbMid = dbLo + 0.5f * -minDb / (float) n;
        const auto seg = juce::Rectangle<float> (x0 + (float) i * (segW + gap), bar.getY(), segW, bar.getHeight());

        float level = 0.0f;
        if (dbMid <= rmsDb)       level = 1.0f;
        else if (dbMid <= peakDb) level = 0.45f;

        auto c = zoneColour (dbMid);
        if (i == holdIndex) { c = colours::text; level = 1.0f; }

        // 目標帯は消灯時もわずかに明るく（どこを狙うか常に見える）
        if (level <= 0.0f && dbMid >= targetLow && dbMid <= targetHigh)
            level = 0.12f;

        paint::ledBar (g, seg, c, level);
    }

    // クリップ LED
    {
        const auto led = clipArea.withSizeKeepingCentre (clipArea.getWidth(), barH).withY (bar.getY());
        paint::inset (g, led.expanded (1.0f), 2.0f);
        paint::ledBar (g, led.reduced (1.5f), colours::rec, clipped ? 1.0f : 0.0f);
    }

    if (! full)
        return;

    // 目標帯のブラケット（上は下向き、下は上向きのカギ）
    {
        const auto tx0 = std::round (dbToX (targetLow, bar)), tx1 = std::round (dbToX (targetHigh, bar));
        const auto yTop = bar.getY() - 5.0f, yBot = bar.getBottom() + 4.0f;
        g.setColour (colours::signal.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (tx0, yTop, tx1 - tx0, 1.0f));
        g.fillRect (juce::Rectangle<float> (tx0, yTop, 1.0f, 3.0f));
        g.fillRect (juce::Rectangle<float> (tx1 - 1.0f, yTop, 1.0f, 3.0f));
        g.fillRect (juce::Rectangle<float> (tx0, yBot, tx1 - tx0, 1.0f));
        g.fillRect (juce::Rectangle<float> (tx0, yBot - 2.0f, 1.0f, 3.0f));
        g.fillRect (juce::Rectangle<float> (tx1 - 1.0f, yBot - 2.0f, 1.0f, 3.0f));
    }

    // 目盛り
    g.setFont (mono (9.5f, Weight::medium));
    for (auto db : { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, -3.0f, 0.0f })
    {
        const auto x = dbToX (db, bar);
        const bool target = juce::approximatelyEqual (db, targetLow) || juce::approximatelyEqual (db, targetHigh);
        g.setColour (target ? colours::signal : colours::textMute);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (x - 14.0f, bar.getBottom() + 7.0f, 28.0f, 12.0f),
                    juce::Justification::centred, false);
    }
}
} // namespace vb
