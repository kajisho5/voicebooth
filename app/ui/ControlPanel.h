#pragma once

#include "DummySession.h"
#include "Widgets.h"

namespace vb
{
/** DESIGN 4.7 / 4.8 下部コントロール：テンポ / キー / 音量 / モニターリバーブ / 録音 / 入力メーター */
class ControlPanel : public juce::Component
{
public:
    explicit ControlPanel (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 132;

private:
    const dummy::Session& session;

    LabeledKnob tempo, key, reverb;
    FaderRow offVocal, mainVol, harmVol, monitorVol;
    SegmentedControl recMode;
    InputMeter meter;

    juce::Rectangle<int> tempoCard, keyCard, mixCard, reverbCard, recCard, inputCard;
    juce::Rectangle<int> recStatusArea, inputReadoutArea;
};
} // namespace vb
