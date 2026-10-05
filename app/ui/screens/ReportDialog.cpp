#include "ReportDialog.h"
#include "../Help.h"

namespace vb
{
namespace
{
    constexpr int preferredW = 720, preferredH = 600;
}

ReportDialog::ReportDialog (UiSession& session)
    : DialogPanel (tr ("report.title"), tr ("report.micro"))
{
    addFooterKey (tr ("report.open"), KeyRole::primary, [this] { if (onOpenIssue) onOpenIssue(); });
    copyKey = &addFooterKey (tr ("report.copy"), KeyRole::normal, [this]
    {
        juce::SystemClipboard::copyTextToClipboard (info.getText());
        copyKey->setButtonText (tr ("report.copied"));
        resized();   // 文字の幅が変わる
    });
    addFooterKey (tr ("common.close"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });

    // 読むだけ（選んでコピーはできる）。等幅で、行の頭がそろうように
    info.setMultiLine (true, false);
    info.setReadOnly (true);
    info.setCaretVisible (false);
    info.setScrollbarsShown (true);
    info.setIndents (12, 10);
    info.setFont (mono (12.5f));
    info.setText (help::environmentReport (session), false);
    info.setTitle (tr ("report.env"));
    addAndMakeVisible (info);

    setSize (preferredW, preferredH);
}

void ReportDialog::fitToParent()
{
    if (auto* p = getParentComponent())
        setSize (juce::jmin (preferredW, p->getWidth() - 40), juce::jmin (preferredH, p->getHeight() - 40));
}

void ReportDialog::layoutBody (juce::Rectangle<int> r)
{
    introArea = r.removeFromTop (76);
    r.removeFromTop (8);
    labelArea = r.removeFromTop (22);
    noteArea = r.removeFromBottom (44);
    r.removeFromBottom (8);
    info.setBounds (r);
}

void ReportDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawFittedText (tr ("report.intro"), introArea, juce::Justification::topLeft, 4, 1.0f);
    paint::microLabel (g, labelArea.toFloat(), tr ("report.env"), colours::textMute);
    g.setColour (colours::textMute);
    g.setFont (sans (12.0f));
    g.drawFittedText (tr ("report.note"), noteArea, juce::Justification::topLeft, 3, 1.0f);
}
} // namespace vb
