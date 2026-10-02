#pragma once

#include "../Overlay.h"
#include "../UiSession.h"
#include <optional>

/*  テイク比較（B18c。DESIGN 2「テイク比較：標準は簡易・プロは詳細」/ 3「メイン上のスライドパネル」）
    いまのトラックの、範囲（IN / OUT・採用区間の 1 区間・曲全体）に音のある本番のテイクを新しい順に並べる。
    行を選ぶとその範囲だけ採用区間が差し替わり、トラックの再生（B12）でそのまま聴ける（試聴。範囲はループ）。
    「このテイクを使う」で確定（Ctrl / ⌘+Z で戻せる）、キャンセル・Esc・閉じるで元の採用区間へそっくり戻す。
    右に出して背景を暗くしない：波形レーンの採用区間が差し替わるのを見ながら選べる。
    標準：入り（ONSET）と音程（PITCH）。プロ：＋ビブラート（VIB）と、合った入りの数・ずれの平均（DESIGN 2「解析」はプロ） */

namespace vb
{
class TakeCompareDialog : public DialogPanel,
                          private SessionView
{
public:
    /** UiSession::beginTakeCompare の後に作る */
    explicit TakeCompareDialog (UiSession&);
    ~TakeCompareDialog() override;

    /** 選んだテイクを採用して閉じる */
    void use();
    /** 元のまま閉じる（キャンセル・Esc・閉じるキー） */
    void cancel();
    /** 閉じる時（MainComponent がパネルを下げる） */
    std::function<void()> onFinished;

    /** Space：試聴の再生 / 停止。↑ ↓：テイクを選ぶ（選ぶとすぐ差し替わる） */
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int rowH = 50;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    struct Entry
    {
        juce::String takeId;                   // 空 = いまの採用（元のまま）
        juce::Time created;
        bool clip = false;
        bool rehearsal = false;                // 原速のリハーサル（使うと本番のテイクに移る）
        float coverage = 1.0f;                 // 範囲のうちテイクで埋められる割合
        float share = 0.0f;                    // 範囲のうち、比べ始めた時の採用区間で使っていた割合
        std::optional<dummy::Session::TakeStats> stats;
    };

    /** テイクの行の並び（スクロールする） */
    class List : public juce::Component
    {
    public:
        explicit List (TakeCompareDialog& d) : owner (d) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    private:
        void paintRow (juce::Graphics&, juce::Rectangle<float>, const Entry&, bool selected, bool over);
        TakeCompareDialog& owner;
        int hover = -1;
    };

    void onSessionChanged (juce::uint32) override;
    void rebuild();
    void select (int index);
    int selectedIndex() const;
    void refreshKeys();
    void finish (bool commit);
    bool pro() const { return state().mode == project::Mode::pro; }

    std::vector<Entry> entries;
    int serial = 0;
    bool finished = false;
    juce::String statsSignature;

    KeyButton playKey;
    KeyButton* useKey = nullptr;
    juce::Viewport view;
    List list { *this };
    juce::Rectangle<int> rangeArea, columnArea, hintArea;

    // 数値の列（右から）。プロだけ VIB
    static constexpr int onsetW = 100, pitchW = 56, vibW = 96;
};
} // namespace vb
