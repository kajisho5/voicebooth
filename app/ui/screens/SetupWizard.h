#pragma once

#include "../Overlay.h"
#include "../UiSession.h"
#include "../parts/LedMeter.h"

namespace vb
{
/** DESIGN 5 入力セットアップ（デバイス → レベル → レイテンシ）。静的モック */
class SetupWizard : public DialogPanel, private SessionView
{
public:
    SetupWizard (UiSession&, int step);

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override {}
    void setStep (int);
    void paintStepper (juce::Graphics&, juce::Rectangle<int>);
    void paintDevice (juce::Graphics&, juce::Rectangle<int>);
    void paintLevel (juce::Graphics&, juce::Rectangle<int>);
    void paintLatency (juce::Graphics&, juce::Rectangle<int>);

    int step = 0;
    LedMeter meter;
    KeyButton measure;
    KeyButton* back = nullptr;
    KeyButton* next = nullptr;
    juce::Rectangle<int> meterArea, measureArea;
};
} // namespace vb
