#include "VoiceBoothLookAndFeel.h"
#include "parts/Icons.h"

namespace vb
{
VoiceBoothLookAndFeel::VoiceBoothLookAndFeel()
{
    applySkinColours();
}

void VoiceBoothLookAndFeel::applySkinColours()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg0);
    setColour (juce::DocumentWindow::textColourId, colours::text);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::raisedHi);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);

    // 文字入力（スキンエディタの名前・HEX）・色の選択
    setColour (juce::TextEditor::backgroundColourId, colours::bgDeep);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::signal.withAlpha (0.3f));
    setColour (juce::TextEditor::highlightedTextColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, colours::line);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::lineHi);
    setColour (juce::CaretComponent::caretColourId, colours::signal);

    // ファイル選択（OS のダイアログが無い時の JUCE 製）
    setColour (juce::ListBox::backgroundColourId, colours::bgDeep);
    setColour (juce::ListBox::textColourId, colours::text);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, colours::raisedHi);
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::raised);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::raised);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::line);
    setColour (juce::ScrollBar::thumbColourId, colours::lineHi);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
}

juce::Typeface::Ptr VoiceBoothLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return sansTypeface (f.isBold() ? Weight::semibold : Weight::regular);
}

namespace
{
    /** ツールチップの文「説明\tショートカット」を分ける（KeyButton::withShortcut。DESIGN 4.10.1 TT） */
    struct TooltipParts { juce::String text, shortcut; };

    TooltipParts splitTooltip (const juce::String& s)
    {
        const auto tab = s.indexOfChar ('\t');
        if (tab < 0) return { s, {} };
        return { s.substring (0, tab), s.substring (tab + 1) };
    }

    constexpr float kbdPad = 5.0f, kbdGap = 8.0f;

    float kbdWidth (const juce::String& shortcut)
    {
        return shortcut.isEmpty() ? 0.0f : textWidth (mono (11.0f), shortcut) + kbdPad * 2.0f + kbdGap;
    }
}

juce::Rectangle<int> VoiceBoothLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto parts = splitTooltip (tip);
    const auto w = (int) std::ceil (textWidth (sans (12.0f), parts.text) + kbdWidth (parts.shortcut)) + 20;
    const auto h = 26;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parent);
}

void VoiceBoothLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tip, int width, int height)
{
    const auto parts = splitTooltip (tip);
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (colours::raisedHi);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::lineHi);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    auto content = r.reduced (10.0f, 0.0f);
    if (parts.shortcut.isNotEmpty())
    {
        // ショートカットはキーの形（下辺を太く）で右に添える
        const auto kf = mono (11.0f);
        auto key = content.removeFromRight (textWidth (kf, parts.shortcut) + kbdPad * 2.0f).withSizeKeepingCentre (textWidth (kf, parts.shortcut) + kbdPad * 2.0f, 17.0f);
        content.removeFromRight (kbdGap);
        g.setColour (colours::lineHi);
        g.drawRoundedRectangle (key.reduced (0.5f), 3.0f, 1.0f);
        g.fillRect (key.withTop (key.getBottom() - 2.0f).reduced (2.0f, 0.0f));
        g.setColour (colours::textDim);
        g.setFont (kf);
        g.drawText (parts.shortcut, key.withTrimmedBottom (1.0f), juce::Justification::centred, false);
    }

    g.setColour (colours::text);
    g.setFont (sans (12.0f));
    g.drawText (parts.text, content, juce::Justification::centredLeft, false);
}

//==============================================================================
void VoiceBoothLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.fillAll (colours::panel);
    g.setColour (colours::lineHi);
    g.drawRect (r, 1.0f);
}

void VoiceBoothLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& text)
{
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f, Weight::medium));
    g.drawText (text, area.toFloat().withTrimmedLeft (13.0f).withTrimmedRight (8.0f), juce::Justification::bottomLeft, true);
}

juce::Font VoiceBoothLookAndFeel::getPopupMenuFont()
{
    return sans (13.0f);
}

void VoiceBoothLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                               bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                               bool hasSubMenu, const juce::String& text, const juce::String&,
                                               const juce::Drawable* icon, const juce::Colour*)
{
    if (isSeparator)
    {
        paint::hline (g, (float) area.getCentreY(), (float) area.getX() + 8.0f, (float) area.getRight() - 8.0f);
        return;
    }

    auto r = area.toFloat().reduced (3.0f, 1.0f);
    if (isHighlighted && isActive)
    {
        g.setColour (colours::raisedHi);
        g.fillRoundedRectangle (r, 3.0f);
    }

    r.removeFromLeft (10.0f);
    const auto ledArea = r.removeFromLeft (14.0f);
    paint::led (g, ledArea.getCentre(), 2.8f, colours::signal, isTicked);
    r.removeFromLeft (8.0f);

    // 添え物の絵（スキンエディタの色の並びなど）は右に寄せる
    if (icon != nullptr)
    {
        const auto b = icon->getDrawableBounds();
        const auto h = juce::jmin (r.getHeight() - 8.0f, b.getHeight());
        const auto w = b.getHeight() > 0.0f ? b.getWidth() * h / b.getHeight() : 0.0f;
        icon->drawWithin (g, r.removeFromRight (w + 10.0f).withTrimmedRight (10.0f).withSizeKeepingCentre (w, h),
                          juce::RectanglePlacement::centred, isActive ? 1.0f : 0.4f);
        r.removeFromRight (8.0f);
    }

    // 下にメニューがある項目は右に ›（区間の「ここから区間」など）
    if (hasSubMenu)
        drawIcon (g, Icon::chevronRight, r.removeFromRight (22.0f).withSizeKeepingCentre (12.0f, 12.0f),
                  isHighlighted ? colours::signal : colours::textDim);

    g.setColour (isActive ? (isTicked ? colours::text : colours::text.withAlpha (0.85f)) : colours::textMute);
    g.setFont (sansForLanguageName (text, 13.0f, isTicked ? Weight::semibold : Weight::regular));
    g.drawText (text, r, juce::Justification::centredLeft, true);
}
} // namespace vb
