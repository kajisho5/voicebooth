#pragma once

#include "UiSession.h"
#include "Actions.h"
#include "parts/KeyButton.h"

namespace vb
{
/** DESIGN 4.4 歌詞。現在フレーズを強調（歌った分を点灯色でワイプ）、次フレーズを薄く
    B4b（DESIGN 7.5.3）：
    - 時刻のある行は再生位置で、時刻の無い歌詞は今の行（↑ ↓ で手送り）を出す
    - 「タップで合わせる」の間は次に叩く行と Enter の印。Backspace で 1 行戻す、Esc で終わる
    - 歌詞パッド：ファイル（.txt / .lrc）の読み込み・貼り付け・修正 */
class LyricsLane : public juce::Component, private SessionView
{
public:
    LyricsLane (UiSession&, Actions&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 60;

private:
    void onSessionChanged (juce::uint32 c) override;
    void refreshKeys();

    void paintTimed (juce::Graphics&, juce::Rectangle<float>);
    void paintManual (juce::Graphics&, juce::Rectangle<float>);
    void paintSyncing (juce::Graphics&, juce::Rectangle<float>);
    /** 次の行（暗く先に出す）。soon は 0..1：次の行が近づくほど明るくする（目を先に移せるように） */
    void paintNext (juce::Graphics&, juce::Rectangle<float>, const juce::String& text, float soon = 0.0f);

    Actions& actions;
    KeyButton syncKey;
    KeyButton editButton { {}, KeyButton::Kind::ghost };
    juce::Rectangle<int> textArea;
};
} // namespace vb
