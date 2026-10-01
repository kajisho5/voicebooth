#pragma once

#include "Theme.h"

namespace vb
{
/** JUCE 標準部品（ツールチップ・ポップアップ等）を DESIGN 4.9 に合わせる。
    VoiceBooth 独自の部品は parts/ で自前描画する */
class VoiceBoothLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoiceBoothLookAndFeel();

    /** 色 ID を今のスキンの色で入れ直す（スキンを変えた時。DESIGN 4.11） */
    void applySkinColours();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;

    // ポップアップメニュー（Dropdown）
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override { return 4; }
};
} // namespace vb
