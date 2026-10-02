#pragma once

#include "UiSession.h"
#include "Actions.h"
#include "Timeline.h"
#include "parts/KeyButton.h"
#include "parts/Readout.h"

namespace vb
{
/** ブースのロゴマーク（窓＋カプセル＋タリー） */
void drawBoothMark (juce::Graphics&, juce::Rectangle<float>, bool recording);

/** DESIGN 4.1 トップバー：ロゴ / 曲名 / KEY・BPM（押すと曲の情報。B4b）/ モード / 入力デバイス / タリー / 書き出し / 設定 */
class TopBar : public juce::Component, private SessionView
{
public:
    TopBar (UiSession&, Actions&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int height = 52;

private:
    void onSessionChanged (juce::uint32) override;

    Actions& actions;
    SegmentedKeys mode;
    KeyButton device { {}, KeyButton::Kind::ghost };
    KeyButton songInfo { {}, KeyButton::Kind::ghost };   // 「KEY C  BPM 128」：押すとテンポ・キー・区間のパネル
    TallyLamp tally;
    KeyButton exportKey, settings;

    juce::Rectangle<int> logoArea, songArea, modeLabelArea;
};
} // namespace vb
