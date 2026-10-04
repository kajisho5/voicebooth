#include "Overlay.h"

namespace vb
{
DialogPanel::DialogPanel (const juce::String& t, const juce::String& m)
    : title (t), microTitle (m)
{
    closeKey.withIcon (Icon::close);
    closeKey.setTooltip (tr ("common.close"));
    closeKey.onClick = [this] { if (onCloseRequest) onCloseRequest(); };
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
    g.setFont (titleExact ? sansExact (titleHeight, Weight::semibold) : sans (titleHeight, Weight::semibold));
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

    // キーボードの操作をダイアログへ（Tab で中のキーに移れるように）。キーにはまだフォーカスを置かない（Enter で誤って押さない）。
    // Tab はダイアログの中だけを回る（後ろの画面のキーへ出ると、見えないまま Enter で押せてしまう。#28）。
    // ダイアログ自身がフォーカスを受けない画面（起動画面など）にも移す：後ろの部品にフォーカスを残さない
    content->setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);
    if (! content->getWantsKeyboardFocus())
        content->setWantsKeyboardFocus (true);
    if (content->isShowing())
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
    if (content == nullptr || placing) return;
    const juce::ScopedValueSetter<bool> guard (placing, true);
    if (content->getWidth() == 0 || content->getHeight() == 0)
    {
        content->setTransform ({});
        content->setBounds (getLocalBounds());
        return;
    }

    // 見えている範囲（窓が画面より大きいとき・小さい画面は、画面の作業領域と重なる所）。
    // パネルがそこに収まらなければ、全体を縮めて収める（書き出しの見出しと［書き出す］が画面の外に出ていた。#18）
    auto visible = getLocalBounds();
    if (isShowing())
        if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
            visible = visible.getIntersection (getLocalArea (nullptr, d->userArea));
    if (visible.isEmpty())
        visible = getLocalBounds();
    const auto avail = visible.reduced (16);
    const auto w = (float) content->getWidth(), h = (float) content->getHeight();
    const auto k = juce::jmin (1.0f, (float) avail.getWidth() / w, (float) avail.getHeight() / h);

    if (k >= 1.0f)
    {
        content->setTransform ({});
        if (placement == Placement::side)
            content->setTopLeftPosition (getWidth() - content->getWidth() - 16,
                                         juce::jmax (8, (getHeight() - content->getHeight()) / 2));
        else
            content->setCentrePosition (getLocalBounds().getCentre());
        return;
    }
    const auto x = placement == Placement::side ? (float) avail.getRight() - w * k : (float) avail.getCentreX() - w * k * 0.5f;
    const auto y = (float) avail.getCentreY() - h * k * 0.5f;
    content->setTopLeftPosition (0, 0);
    content->setTransform (juce::AffineTransform::scale (juce::jmax (0.4f, k)).translated (x, y));
}

void OverlayHost::childBoundsChanged (juce::Component* child)
{
    if (child == content.get())
        resized();
}

void OverlayHost::paint (juce::Graphics& g)
{
    if (placement == Placement::centre)
        g.fillAll (colours::shadow (0.58f));
    else if (content != nullptr)
        juce::DropShadow (colours::shadow (0.55f), 28, { 0, 8 }).drawForRectangle (g, content->getBoundsInParent());   // 暗くしない代わりに影で浮かせる
}

void OverlayHost::mouseDown (const juce::MouseEvent& e)
{
    if (dismissible && content != nullptr && ! content->getBoundsInParent().contains (e.getPosition()))
        close();
}
} // namespace vb
