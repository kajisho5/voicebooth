#pragma once

#include "../Theme.h"
#include "../Animator.h"
#include <array>

namespace vb
{
/** セグメント LED メーター（DESIGN 4.8）
    ピーク（薄く点灯）/ RMS（点灯）/ ピークホールド / クリップ LED / 目標帯 -12〜-6 dBFS

    メーターの動き（上がる時は即、下がる時は 20 dB/秒、ホールド 1.5 秒、クリップはクリックまで）は
    audio::InputMeter（B3）が作る。ここで足すのは見た目だけ（DESIGN 4.10「入力メーター」）：
    - 消える粒は一瞬で消さず、短い余韻（約 0.1 秒）で暗くなる
    - クリップ LED は点いた時に一度光り、消した時は余韻を残す */
class LedMeter : public juce::Component,
                 public juce::SettableTooltipClient,
                 private motion::Animated
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
    bool advanceAnimation (float dt) override;
    juce::Colour zoneColour (float db) const;
    float dbToX (float db, juce::Rectangle<float> bar) const;

    Style style;
    float peakDb = minDb, rmsDb = minDb, holdDb = minDb;
    bool clipped = false;

    static constexpr int maxSegments = 256;
    std::array<float, maxSegments> afterglow {};   // 消えかけの粒の明るさ（描画で確保しない）
    float clipLevel = 0.0f, clipFlash = 0.0f;
};
} // namespace vb
