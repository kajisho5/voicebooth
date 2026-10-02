#pragma once

#include "../Overlay.h"
#include "../UiSession.h"

/*  声域を測る（2026-10-02。DESIGN 18.1）
    楽に出せるいちばん低い声・高い声を、それぞれ 2〜3 秒伸ばしてもらい、マイクの音程の中央値を取る。
    測った値は −/+ で半音ずつ直せる。お手本があれば、その最低音・最高音と、収まるキー（おすすめ）を出す。
    測っている間は曲を止める。声はどこにも保存しない（音程の値だけ設定に残す） */

namespace vb
{
class RangeDialog : public DialogPanel, private SessionView, private juce::Timer
{
public:
    explicit RangeDialog (UiSession&);
    ~RangeDialog() override;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override;
    void timerCallback() override;
    void start (int which);   // 0 = 低い声、1 = 高い声
    void finish();
    void nudge (int which, int delta);
    void refreshKeys();

    static constexpr double measureSeconds = 3.0;
    static constexpr size_t minVoiced = 30;   // 0.3 秒分。これより少なければ「声が取れない」

    KeyButton measureKey[2], downKey[2], upKey[2];
    KeyButton* applyKey = nullptr;
    int measuring = -1;
    double startedAt = 0.0;
    float liveNote = 0.0f;
    juce::String problem;
    juce::Rectangle<int> introArea, rowArea[2], liveArea, resultArea;
};
} // namespace vb
