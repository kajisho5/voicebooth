#pragma once

#include "DummySession.h"
#include "Widgets.h"
#include "Timeline.h"

namespace vb
{
/** DESIGN 4.1 ヘッダー：ロゴ / 曲名 / モード / 入力デバイス / 状態 / 設定 */
class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 48;

private:
    const dummy::Session& session;
    SegmentedControl mode;
    ChipButton device;
    IconButton settings { juce::String::fromUTF8 ("設定"), Icon::gear };

    juce::Rectangle<int> logoArea, songArea, modeLabelArea, stateArea;
};
} // namespace vb
