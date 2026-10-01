#pragma once

#include "DummySession.h"
#include "Timeline.h"
#include "parts/KeyButton.h"

namespace vb
{
/** DESIGN 4.3 ピッチレーン（主役）
    お手本 = アイスブルーの許容帯（±ピッチ許容）＋中心線、自分 = 状態色の線 */
class PitchLane : public juce::Component
{
public:
    explicit PitchLane (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    float yForMidi (float midi) const;
    juce::Colour colourForCents (float cents) const;

    void drawBackground (juce::Graphics&, const TimeMap&);
    void drawNoteGutter (juce::Graphics&);
    void drawReference (juce::Graphics&, const TimeMap&);
    void drawMine (juce::Graphics&, const TimeMap&);
    void drawCurrent (juce::Graphics&, const TimeMap&);
    void drawFooter (juce::Graphics&);

    const dummy::Session& session;

    juce::Rectangle<int> rulerArea, gutterArea, plotArea, footerArea, legendArea;
    KeyButton octaveAlign { jp ("オクターブ合わせ") };
    KeyButton octaveUp    { jp ("自分を +1oct") };
    KeyButton fullRange   { jp ("全体の音域") };
};
} // namespace vb
