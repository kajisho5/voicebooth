#pragma once

#include "DummySession.h"
#include "Widgets.h"
#include "Timeline.h"

namespace vb
{
/** DESIGN 4.3 ピッチレーン（主役）。お手本=紫、自分=状態色 */
class PitchLane : public juce::Component
{
public:
    explicit PitchLane (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    float yForMidi (float midi) const;
    juce::Colour colourForCents (float cents) const;

    void drawKeyboard (juce::Graphics&);
    void drawGrid (juce::Graphics&, const TimeMap&);
    void drawReference (juce::Graphics&, const TimeMap&);
    void drawMine (juce::Graphics&, const TimeMap&);
    void drawCurrentDot (juce::Graphics&, const TimeMap&);
    void drawFooter (juce::Graphics&);

    const dummy::Session& session;

    juce::Rectangle<int> rulerArea, gutterArea, plotArea, footerArea, legendArea;
    ChipButton octaveAlign { jp ("オクターブ合わせ") };
    ChipButton octaveUp    { jp ("自分を+1oct") };
    ChipButton fullRange   { jp ("全体表示") };
};
} // namespace vb
