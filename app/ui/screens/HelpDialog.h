#pragma once

#include "../Overlay.h"
#include "../UiSession.h"
#include "../Help.h"

/*  困ったときのヘルプ（2026-10-04。持ち主の案。上のバーの ? と F1）
    よくある困りごとの一覧。見出しを押すと直し方を開く（1 つずつ）。いまの状態に関係する項目は先に並べ、印を付け、最初の 1 つを開いておく。
    直し方の下に、関係する画面へ移るキー（入力セットアップ・遅延の測定・分離モデルのダウンロード・不具合の報告）。
    窓が小さいときは一覧をスクロールする */

namespace vb
{
class HelpDialog : public DialogPanel
{
public:
    explicit HelpDialog (UiSession&);
    ~HelpDialog() override;

    /** 移る先のキーを押した（MainComponent が画面を開く。ヘルプは閉じる） */
    std::function<void (help::Action)> onAction;

    void parentHierarchyChanged() override { fitToParent(); }
    void parentSizeChanged() override { fitToParent(); }

    /** テスト用：並んでいる項目と、開いている項目（なければ -1） */
    const std::vector<help::Item>& shownItems() const;
    int openIndex() const;
    void openItem (int index);

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void fitToParent();

    class List;
    std::unique_ptr<List> list;
    juce::Viewport viewport;
    juce::Rectangle<int> introArea;
};
} // namespace vb
