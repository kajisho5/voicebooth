#pragma once

#include "../Theme.h"

namespace vb
{
/** 沈んだ表示窓に Mono の数値（TIME / BAR.BEAT / TEMPO / KEY など） */
class Readout : public juce::Component
{
public:
    explicit Readout (const juce::String& label);

    void setValue (const juce::String& main, const juce::String& sub = {});
    void setValueColour (colours::Tone c) { valueColour = c; repaint(); }
    void setMainSize (float h) { mainSize = h; repaint(); }

    int idealWidth() const;

    void paint (juce::Graphics&) override;

private:
    juce::String label, main, sub;
    colours::Tone valueColour = colours::text;
    float mainSize = 20.0f;
};

/** 状態ランプ（STANDBY / PLAY / REC）。REC は本体ごとタリー赤に灯る */
class TallyLamp : public juce::Component
{
public:
    enum class State { standby, play, rec };

    void setState (State s) { state = s; repaint(); }
    void paint (juce::Graphics&) override;

    static int idealWidth() { return 96; }

private:
    State state = State::standby;
};
} // namespace vb
