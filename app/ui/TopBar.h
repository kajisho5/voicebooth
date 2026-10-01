#pragma once

#include "DummySession.h"
#include "Timeline.h"
#include "parts/KeyButton.h"
#include "parts/Readout.h"

namespace vb
{
/** ブースのロゴマーク（窓＋カプセル＋タリー） */
void drawBoothMark (juce::Graphics&, juce::Rectangle<float>, bool recording);

/** DESIGN 4.1 トップバー：ロゴ / 曲名 / モード / 入力デバイス / タリー / 設定 */
class TopBar : public juce::Component
{
public:
    explicit TopBar (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 52;

private:
    const dummy::Session& session;
    SegmentedKeys mode;
    KeyButton device;
    TallyLamp tally;
    KeyButton settings;

    juce::Rectangle<int> logoArea, songArea, modeLabelArea;
};
} // namespace vb
