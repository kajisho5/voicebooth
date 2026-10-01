#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*  見た目トークン（DESIGN 4.9）
    色はここ以外にハードコードしない。派生色もここで定義する。 */

namespace vb
{
using int64 = juce::int64;

/** UTF-8 リテラル → juce::String（MSVC は /utf-8 でビルドする前提） */
inline juce::String jp (const char* utf8) { return juce::String::fromUTF8 (utf8); }

namespace colours
{
    // --- DESIGN 4.9 のトークン ---
    inline const juce::Colour bg0      { 0xff0e141b };
    inline const juce::Colour panel    { 0xff151c25 };
    inline const juce::Colour grid     { 0xff1e2733 };
    inline const juce::Colour text     { 0xffe8eef4 };
    inline const juce::Colour textDim  { 0xff8b97a6 };
    inline const juce::Colour accent   { 0xff5eead4 };   // 自分ピッチ、シーク
    inline const juce::Colour refPitch { 0xffa78bfa };   // お手本
    inline const juce::Colour warn     { 0xfffbbf24 };
    inline const juce::Colour bad      { 0xfff43f5e };
    inline const juce::Colour rec      { 0xffef4444 };
    inline const juce::Colour ok       { 0xff34d399 };

    // --- 派生（トークンから作った中間色） ---
    inline const juce::Colour bgDeep   { 0xff0a0f15 };   // レーン内背景
    inline const juce::Colour panelHi  { 0xff1a2330 };   // ボタン地・ホバー
    inline const juce::Colour border   { 0xff243041 };   // パネル境界
    inline const juce::Colour textMute { 0xff5b6676 };   // さらに弱い文字
}

namespace metrics
{
    constexpr int gutter  = 72;   // ピッチ/波形レーン左の固定幅（時間軸を揃える）
    constexpr int pad     = 16;   // 画面端の余白
    constexpr float radius = 6.0f;
}

enum class FontWeight { regular, bold };

juce::Typeface::Ptr typeface (FontWeight);
juce::Font font (float height, FontWeight = FontWeight::regular);

/** テキスト幅（px） */
float textWidth (const juce::Font&, const juce::String&);

/** パネル（カード）の共通描画 */
void drawCard (juce::Graphics&, juce::Rectangle<float>, juce::Colour fill = colours::panel);

/** カード左上の小見出し */
void drawCardTitle (juce::Graphics&, juce::Rectangle<int> area, const juce::String&);
} // namespace vb
