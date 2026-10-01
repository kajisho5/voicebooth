#pragma once

#include "UiSession.h"
#include "Actions.h"
#include "Timeline.h"
#include "parts/KeyButton.h"
#include "parts/Readout.h"

namespace vb
{
/** 曲全体の概形シーク：オフボ概形 / 再生済み / ループ範囲 / 表示中ウィンドウ / 再生ヘッド
    クリック・ドラッグで移動 */
class OverviewSeek : public juce::Component, private SessionView
{
public:
    explicit OverviewSeek (UiSession& u) : SessionView (u) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { seekTo (e.position.x); }
    void mouseDrag (const juce::MouseEvent& e) override { seekTo (e.position.x); }

private:
    void onSessionChanged (juce::uint32 c) override
    {
        if (c & (change::playhead | change::range | change::view | change::transport)) repaint();
    }
    void seekTo (float x);
    juce::Rectangle<float> inner() const { return getLocalBounds().toFloat().reduced (8.0f, 5.0f); }
};

/** DESIGN 4.2 輸送バー */
class TransportBar : public juce::Component, private SessionView
{
public:
    TransportBar (UiSession&, Actions&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 64;

    /** ショートカットの見た目反応用 */
    KeyButton& playKey()     { return play; }
    KeyButton& recKey()      { return rec; }
    KeyButton& loopKey()     { return loop; }
    KeyButton& rangeInKey()  { return rangeIn; }
    KeyButton& rangeOutKey() { return rangeOut; }

private:
    void onSessionChanged (juce::uint32) override;

    Actions& actions;
    KeyButton toStart, play, stop, rec { {}, KeyButton::Kind::rec };
    Readout time, beat;
    KeyButton loop, rangeIn, rangeOut, clearRange;
    OverviewSeek seek;
    SegmentedKeys countIn;
    KeyButton click;

    juce::Rectangle<int> countLabel;
};
} // namespace vb
