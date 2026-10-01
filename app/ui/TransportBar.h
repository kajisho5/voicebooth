#pragma once

#include "DummySession.h"
#include "Timeline.h"
#include "parts/KeyButton.h"
#include "parts/Readout.h"

namespace vb
{
/** 曲全体の概形シーク：オフボ概形 / 再生済み / ループ範囲 / 表示中ウィンドウ / 再生ヘッド */
class OverviewSeek : public juce::Component
{
public:
    explicit OverviewSeek (const dummy::Session& s) : session (s) {}
    void paint (juce::Graphics&) override;

private:
    const dummy::Session& session;
};

/** DESIGN 4.2 輸送バー */
class TransportBar : public juce::Component
{
public:
    explicit TransportBar (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 64;

private:
    const dummy::Session& session;

    KeyButton toStart, play, stop, rec { {}, KeyButton::Kind::rec };
    Readout time { "TIME" }, beat { "BAR.BEAT" };
    KeyButton loop, rangeIn, rangeOut, clearRange;
    OverviewSeek seek;
    SegmentedKeys countIn;
    KeyButton click;

    juce::Rectangle<int> countLabel;
};
} // namespace vb
