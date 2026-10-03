#include "Overlay.h"

namespace vb
{
DialogPanel::DialogPanel (const juce::String& t, const juce::String& m)
    : title (t), microTitle (m)
{
    closeKey.withIcon (Icon::close);
    closeKey.setTooltip (tr ("common.close"));
    closeKey.onClick = [this] { if (onCloseRequest) onCloseRequest(); };
    closeKey.setWantsKeyboardFocus (true);              // Tab で移れる（枠はキーボードの時だけ。DESIGN 4.10.1 FC）
    closeKey.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (closeKey);

    // ダイアログ自身がフォーカスを受ける（出した時はここ。Tab で中のキーへ、Esc はメイン画面が閉じる）
    setWantsKeyboardFocus (true);
}

KeyButton& DialogPanel::addFooterKey (const juce::String& text, KeyRole role, std::function<void()> onClick)
{
    auto* k = footerKeys.add (new KeyButton (text));
    if (role == KeyRole::primary) k->withLed (colours::signal).withToggle (false);
    if (role == KeyRole::danger)  k->withLed (colours::rec).withToggle (false);
    k->setToggleState (role != KeyRole::normal, juce::dontSendNotification);   // LED を点けておく
    k->onClick = std::move (onClick);
    k->setWantsKeyboardFocus (true);                    // Tab で移り、Enter で押せる
    k->setMouseClickGrabsKeyboardFocus (false);         // マウスで押した時は枠を出さない
    roles.push_back (role);
    addAndMakeVisible (k);
    resized();
    return *k;
}

juce::Rectangle<int> DialogPanel::body() const
{
    return getLocalBounds().reduced (padding, 0).withTrimmedTop (headerH + 14).withTrimmedBottom (footerH + 6);
}

void DialogPanel::resized()
{
    closeKey.setBounds (getWidth() - padding - 28, (headerH - 28) / 2, 28, 28);

    auto f = getLocalBounds().removeFromBottom (footerH).reduced (padding, 0);
    for (auto* k : footerKeys)
    {
        if (! k->isVisible()) continue;   // 隠したキーは詰める
        k->setSize (10, footerKeyHeight);
        const auto w = juce::jmax (96, k->idealWidth());
        k->setBounds (f.removeFromRight (w).withSizeKeepingCentre (w, footerKeyHeight));
        f.removeFromRight (8);
    }

    layoutBody (body());
}

void DialogPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    g.setColour (colours::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (colours::lineHi);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    // 見出し
    auto head = getLocalBounds().removeFromTop (headerH).reduced (padding, 0);
    paint::microLabel (g, head.removeFromTop (headerH / 2).toFloat().withTrimmedTop (10.0f), microTitle, colours::textMute);
    g.setColour (colours::text);
    g.setFont (sans (titleHeight, Weight::semibold));
    g.drawText (title, head.withTrimmedRight (40).translated (0, -6), juce::Justification::centredLeft, true);

    paint::hline (g, (float) headerH, 0.0f, b.getWidth());
    paint::hline (g, b.getHeight() - (float) footerH, 0.0f, b.getWidth());

    paintBody (g, body());
}

//==============================================================================
ConfirmDialog::ConfirmDialog (const juce::String& t, const juce::String& msg, std::vector<Option> options)
    : DialogPanel (t, tr ("dialog.confirm.micro")), message (msg)
{
    // キーは右から並ぶ（options[0] が一番右＝主な選択肢）
    for (auto& o : options)
        addFooterKey (o.text, o.role, o.action);

    setSize (520, headerH + 14 + 96 + footerH + 6);
}

void ConfirmDialog::paintBody (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (colours::text.withAlpha (0.9f));
    g.setFont (sans (13.5f));
    g.drawFittedText (message, r, juce::Justification::topLeft, 5, 1.0f);
}

//==============================================================================
OverlayHost::OverlayHost()
{
    setInterceptsMouseClicks (true, true);
    setVisible (false);
}

void OverlayHost::show (std::unique_ptr<juce::Component> panel, bool canDismiss, Placement where)
{
    content = std::move (panel);
    dismissible = canDismiss;
    placement = where;
    addAndMakeVisible (*content);
    setVisible (true);
    toFront (false);
    resized();
    repaint();

    // キーボードの操作をダイアログへ（Tab で中のキーに移れるように）。キーにはまだフォーカスを置かない（Enter で誤って押さない）
    if (content->getWantsKeyboardFocus() && content->isShowing())
        content->grabKeyboardFocus();
}

void OverlayHost::close()
{
    if (content == nullptr) return;
    // コールバック中に自分を消さないよう、次のメッセージで破棄する
    auto* old = content.release();
    setVisible (false);
    juce::MessageManager::callAsync ([old] { delete old; });
    if (onClosed) onClosed();
}

void OverlayHost::resized()
{
    if (content == nullptr) return;
    if (content->getWidth() == 0 || content->getHeight() == 0)
        content->setBounds (getLocalBounds());
    else if (placement == Placement::side)
        content->setTopLeftPosition (getWidth() - content->getWidth() - 16,
                                     juce::jmax (8, (getHeight() - content->getHeight()) / 2));
    else
        content->setCentrePosition (getLocalBounds().getCentre());
}

void OverlayHost::paint (juce::Graphics& g)
{
    if (placement == Placement::centre)
        g.fillAll (colours::shadow (0.58f));
    else if (content != nullptr)
        juce::DropShadow (colours::shadow (0.55f), 28, { 0, 8 }).drawForRectangle (g, content->getBounds());   // 暗くしない代わりに影で浮かせる
}

void OverlayHost::mouseDown (const juce::MouseEvent& e)
{
    if (dismissible && content != nullptr && ! content->getBounds().contains (e.getPosition()))
        close();
}
} // namespace vb
