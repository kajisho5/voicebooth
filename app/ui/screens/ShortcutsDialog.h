#pragma once

#include "../Overlay.h"
#include "../UiSession.h"

/*  1 文字のショートカットを変える（#28。設定の「キーボードのショートカット」から開く）
    操作ごとのキーを押すと「キーを押してください」になり、次に押した文字のキーをその操作に付ける。
    Delete / Backspace でなし（押しても何も起きない）、Esc でやめる。同じキーが付いていた操作はなしになり、そのことを下に表示する。
    変えた値はすぐアプリの設定に保存する。［既定に戻す］で最初の割り当てへ */

namespace vb
{
class ShortcutsDialog : public DialogPanel, private SessionView
{
public:
    explicit ShortcutsDialog (UiSession&);

    bool keyPressed (const juce::KeyPress&) override;
    /** いまキーを待っている操作（なければ -1。テスト用） */
    int capturingIndex() const { return capturing; }
    void startCapture (int index);

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override;
    void refreshKeys();

    KeyButton keys[shortcuts::numActions];
    juce::Rectangle<int> introArea, noteArea, rowAreas[shortcuts::numActions];
    int capturing = -1;
    juce::String note;   // 直前の変更の知らせ（ほかの操作から外した・使えないキー）
};
} // namespace vb
