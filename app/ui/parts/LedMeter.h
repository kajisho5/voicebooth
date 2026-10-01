#pragma once

#include "../Theme.h"

namespace vb
{
/** セグメント LED メーター（DESIGN 4.8）
    ピーク（薄く点灯）/ RMS（点灯）/ ピークホールド / クリップ LED / 目標帯 -12〜-6 dBFS */
class LedMeter : public juce::Component,
                 public juce::SettableTooltipClient
{
public:
    enum class Style { full, compact };

    explicit LedMeter (Style = Style::full);

    void setLevels (float peakDb, float rmsDb, float holdDb, bool clipped);

    /** クリックした（クリップ表示を消す）。設定するとポインタが指に変わる */
    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr float minDb = -60.0f;
    static constexpr float targetLow = -12.0f, targetHigh = -6.0f;

private:
    juce::Colour zoneColour (float db) const;
    float dbToX (float db, juce::Rectangle<float> bar) const;

    Style style;
    float peakDb = minDb, rmsDb = minDb, holdDb = minDb;
    bool clipped = false;
};
} // namespace vb
