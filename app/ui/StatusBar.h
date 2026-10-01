#pragma once

#include "UiSession.h"
#include "Timeline.h"
#include "parts/LedMeter.h"

namespace vb
{
/** 最下段：入力 / レイテンシ / 録音先 / 書き出し形式 / ドライバ / UI MOCK 表示 */
class StatusBar : public juce::Component, private SessionView
{
public:
    explicit StatusBar (UiSession&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 28;

private:
    void onSessionChanged (juce::uint32 c) override { if (c & (change::transport | change::practice | change::mode)) repaint(); }

    LedMeter mini { LedMeter::Style::compact };
    juce::Rectangle<int> meterArea;
};
} // namespace vb
