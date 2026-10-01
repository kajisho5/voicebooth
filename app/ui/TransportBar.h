#pragma once

#include "DummySession.h"
#include "Widgets.h"
#include "Timeline.h"

namespace vb
{
/** 全体シーク：オフボ概形 + 再生済み + 範囲 + 表示中ウィンドウ */
class SeekBar : public juce::Component
{
public:
    explicit SeekBar (const dummy::Session& s) : session (s) {}
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

    static constexpr int height = 52;

private:
    const dummy::Session& session;

    IconButton toStart { jp ("先頭へ"), Icon::toStart };
    IconButton play    { jp ("再生 / 停止 (Space)"), Icon::pause };
    IconButton stop    { jp ("停止"), Icon::stop };
    IconButton rec     { jp ("録音 (R)"), Icon::rec, colours::rec };

    ChipButton loop    { jp ("ループ") };
    ChipButton rangeIn, rangeOut;
    IconButton clearRange { jp ("範囲解除"), Icon::close };

    SeekBar seek;
    SegmentedControl countIn;
    ChipButton click { jp ("クリック") };

    juce::Rectangle<int> timeArea, beatArea, countInLabel;
};
} // namespace vb
