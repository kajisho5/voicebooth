#pragma once

#include "../Overlay.h"
#include "../UiSession.h"

/*  練習の履歴（Phase C「履歴」。DESIGN 4.2.1）
    録ったテイクを全部のトラックから新しい順に並べる：録った日時・トラック・本番 / リハーサル（練習の速さ・キー）・曲のどこか・
    入り（ONSET）・許容範囲内の割合（PITCH）。上には、いまのトラックの PITCH の移り変わりを古い順に点と線で描く。
    行を押すと、そのテイクのトラックを選んでテイクの頭へ移る（パネルは閉じる）。数値は原速・原キーで解析の済んだテイクだけ */

namespace vb
{
class HistoryDialog : public DialogPanel,
                      private SessionView
{
public:
    explicit HistoryDialog (UiSession&);

    /** 閉じる時（MainComponent がパネルを下げる） */
    std::function<void()> onFinished;

    static constexpr int rowH = 40;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    class List : public juce::Component
    {
    public:
        explicit List (HistoryDialog& d) : owner (d) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    private:
        HistoryDialog& owner;
        int hover = -1;
    };

    void onSessionChanged (juce::uint32) override;
    void rebuild();
    void paintChart (juce::Graphics&, juce::Rectangle<float>);

    std::vector<UiSession::HistoryEntry> entries;
    juce::String signature;
    juce::Rectangle<int> chartArea, columnArea;
    juce::Viewport view;
    List list { *this };
};
} // namespace vb
