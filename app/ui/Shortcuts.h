#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <optional>

/*  1 文字のショートカット（#28。2026-10-04 に持ち主の OK）：使う人が別のキーに変えたり、なし（押しても何も起きない）にしたりできる。
    変えられるのは文字のキーだけ（英字・数字・記号）。Space（再生）・Esc・Enter・矢印・Ctrl / ⌘ との組み合わせは変えない。
    同じキーを 2 つの操作に付けたら、前に付いていた操作はなしになる（押したときに何が起きるか迷わない） */

namespace vb::shortcuts
{
enum class Action { record, loop, rangeIn, rangeOut, tapTempo, addSection, track1, track2, track3, track4 };
constexpr int numActions = 10;

/** 操作の名前の翻訳キー（設定の一覧に出す） */
const char* nameKey (Action);
/** 設定の保存に使う名前（翻訳しない） */
const char* id (Action);

/** 付けられるキーか（表示できる 1 文字。空白は Space の再生に使うので外す）。英字は小文字にしてから比べる */
bool assignable (juce::juce_wchar);
/** キーの表示（英字は大文字。なしは空） */
juce::String keyName (juce::juce_wchar);

struct Map
{
    std::array<juce::juce_wchar, numActions> keys {};   // 小文字。0 = なし

    static Map defaults();

    juce::juce_wchar key (Action a) const { return keys[(size_t) a]; }
    juce::String keyName (Action a) const { return shortcuts::keyName (key (a)); }
    /** 押した文字の操作（大文字・小文字は区別しない）。なければ空 */
    std::optional<Action> actionFor (juce::juce_wchar typed) const;
    /** a に c を付ける。同じキーが付いていた操作はなしにして、その操作を返す。付けられない文字なら何もせず空 */
    std::optional<Action> assign (Action a, juce::juce_wchar c);
    void clear (Action a) { keys[(size_t) a] = 0; }
    bool isDefault() const { return *this == defaults(); }

    /** 保存用の文字列（"record=114;loop=108;…"。キーは文字コード、なしは 0）。書いていない・読めない項目は既定のまま */
    juce::String toString() const;
    static Map fromString (const juce::String&);

    bool operator== (const Map& o) const { return keys == o.keys; }
    bool operator!= (const Map& o) const { return keys != o.keys; }
};
} // namespace vb::shortcuts
