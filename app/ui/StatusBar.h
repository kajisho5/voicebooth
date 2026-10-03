#pragma once

#include "UiSession.h"
#include "Timeline.h"
#include "parts/LedMeter.h"

namespace vb
{
/** 最下段：入力 / レイテンシ / 録音先 / 書き出し形式 / ドライバ / 出力デバイス（B2）または UI MOCK 表示
    新しいバージョンの知らせ（DESIGN 4.10）：ばねで滑り込み、LED が 2 回点滅して点灯のまま。ホバーで少し浮く */
class StatusBar : public juce::Component, private SessionView, private motion::Animated
{
public:
    explicit StatusBar (UiSession&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** 「新しいバージョン」の知らせを押した */
    std::function<void()> onUpdateClicked;

    static constexpr int height = 28;

private:
    void onSessionChanged (juce::uint32 c) override;
    bool advanceAnimation (float dt) override;
    void setChipHover (bool);

    LedMeter mini { LedMeter::Style::compact };
    juce::Rectangle<int> meterArea;
    juce::Rectangle<float> updateChip;   // paint で決まる（滑り込んだ後の場所）

    // 知らせの動き
    juce::String noticeVersion;          // いま出しているバージョン（変わったら滑り込み直す）
    motion::Spring chipSlide { 1.0f };   // 0 = 右の外、1 = 所定の位置
    double chipShownAt = -10.0;          // 出した時刻（LED の点滅）
    float chipLift = 0.0f;               // ホバーで浮く（0..1）
    bool chipHover = false;
};
} // namespace vb
