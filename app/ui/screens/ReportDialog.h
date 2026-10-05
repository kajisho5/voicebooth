#pragma once

#include "../Overlay.h"
#include "../UiSession.h"

/*  不具合を報告する（2026-10-05。持ち主の案。ヘルプのメニューから）
    アプリと機器の情報（help::environmentReport）を見せて、コピーするか、GitHub の Issues を題と本文を入れた状態でブラウザで開く。
    送るのは使う人がブラウザで行う（GitHub のアカウントが要る）。アプリからは何も送らない */

namespace vb
{
class ReportDialog : public DialogPanel
{
public:
    explicit ReportDialog (UiSession&);

    /** 「GitHub で報告」を押した（MainComponent がブラウザを開く。報告は閉じる） */
    std::function<void()> onOpenIssue;

    /** 見せている情報（テスト用） */
    juce::String shownText() const { return info.getText(); }

    void parentHierarchyChanged() override { fitToParent(); }
    void parentSizeChanged() override { fitToParent(); }

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void fitToParent();

    juce::TextEditor info;
    KeyButton* copyKey = nullptr;
    juce::Rectangle<int> introArea, labelArea, noteArea;
};
} // namespace vb
