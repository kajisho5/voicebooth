#pragma once

#include "../Theme.h"

/*  アイコン。24x24 グリッドで設計し、描画先の矩形へ拡縮する。
    すべて塗りつぶし用の Path を返す（線のアイコンも輪郭化済み）。 */

namespace vb
{
enum class Icon
{
    play, pause, stop, toStart, rec,
    loop, rangeIn, rangeOut, close,
    metronome, gear, mic, headphones, edit, compare, lock,
    chevronDown, chevronRight, minus, plus,
    exportFile, folder, note, check, warning, globe,
    download, shield, help
};

juce::Path makeIcon (Icon, juce::Rectangle<float> area);

void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float> area, juce::Colour);
} // namespace vb
