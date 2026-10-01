#pragma once

#include "DummySession.h"
#include "Widgets.h"

namespace vb
{
/** 最下段：入力 / レイテンシ / 録音先 / SR・bit / ドライバ */
class StatusBar : public juce::Component
{
public:
    explicit StatusBar (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 28;

private:
    const dummy::Session& session;
    InputMeter mini { true };
    juce::Rectangle<int> meterArea;
};
} // namespace vb
