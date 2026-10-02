#include "KeyButton.h"

namespace vb
{
namespace
{
    constexpr float ledRadius = 2.6f;
    constexpr float sidePad = 10.0f;
    constexpr float ledSlot = 12.0f;   // LED の幅＋間隔
}

KeyButton::KeyButton (const juce::String& label, Kind k)
    : juce::Button (label), kind (k), labelFont (sans (12.0f, Weight::medium))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);   // ショートカットはメイン画面が受ける
    if (kind == Kind::rec)
        setClickingTogglesState (true);
}

KeyButton& KeyButton::withIcon (Icon i)                 { icon = i; repaint(); return *this; }
KeyButton& KeyButton::withLed (colours::Tone c)         { ledColour = c; setClickingTogglesState (true); repaint(); return *this; }
KeyButton& KeyButton::withToggle (bool t)               { setClickingTogglesState (t); return *this; }
KeyButton& KeyButton::withFont (juce::Font f)           { labelFont = f; repaint(); return *this; }
KeyButton& KeyButton::withIconColour (colours::Tone c)  { iconColour = c; repaint(); return *this; }
KeyButton& KeyButton::withLatch (colours::Tone c)       { latchColour = c; setClickingTogglesState (true); repaint(); return *this; }

void KeyButton::flash()
{
    flashLeft = motion::key::flashSeconds;
    startAnimating();
}

juce::String KeyButton::getTooltip()
{
    // 「説明\tショートカット」：LookAndFeel がショートカットをキーの形で描く（DESIGN 4.10.1 TT）
    const auto tip = juce::Button::getTooltip();
    return shortcut.isEmpty() || tip.isEmpty() ? tip : tip + "\t" + shortcut;
}

void KeyButton::buttonStateChanged()
{
    // 押した・離した・トグルが変わった：ばねと LED を動かす
    startAnimating();
}

void KeyButton::focusGained (FocusChangeType cause)
{
    focusRing = cause == focusChangedByTabKey;   // マウスで押した時は枠を出さない
    repaint();
}

void KeyButton::focusLost (FocusChangeType)
{
    focusRing = false;
    repaint();
}

bool KeyButton::advanceAnimation (float dt)
{
    const bool reduced = motion::prefersReducedMotion();
    const bool on = getToggleState();
    flashLeft = juce::jmax (0.0f, flashLeft - dt);
    const float target = (isDown() || flashLeft > 0.0f) ? 1.0f : 0.0f;

    if (! isShowing())
    {
        // 画面に出ていない：最終状態にして止まる
        press.snap (target);
        ledLevel = on ? 1.0f : 0.0f;
        return flashLeft > 0.0f;
    }

    press.step (target, dt, motion::key::springK, motion::key::springC, reduced);
    ledLevel = motion::key::ledLevel (ledLevel, on, dt, reduced);
    repaint();

    const bool breathing = breathes && on && ! reduced;
    return breathing || flashLeft > 0.0f || ! press.atRest (target)
        || ! juce::approximatelyEqual (ledLevel, on ? 1.0f : 0.0f);
}

int KeyButton::idealWidth() const
{
    const auto h = (float) juce::jmax (24, getHeight());
    float w = sidePad * 2.0f;
    if (ledColour.has_value()) w += ledSlot;
    if (icon.has_value())      w += h * 0.5f + (getButtonText().isNotEmpty() ? 6.0f : 0.0f);
    w += textWidth (labelFont, getButtonText());

    if (getButtonText().isEmpty() && ! ledColour.has_value())
        w = h;   // アイコンだけのキーは正方形

    return (int) std::ceil (w);
}

void KeyButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    KeyState s { over, down || flashLeft > 0.0f, getToggleState(), isEnabled() };
    if (preview) s = *preview;

    // 動きの量（ギャラリーの固定表示では動かさない）
    const float pressed = preview ? 0.0f : press.x;                   // ばね（行き過ぎると少し負）
    const float lit = preview ? (s.on ? 1.0f : 0.0f) : ledLevel;      // LED・点灯色の余韻

    const auto bounds = getLocalBounds().toFloat();
    const bool recOn = kind == Kind::rec && s.on;
    const bool latched = latchColour.has_value() && s.on;

    // ばねで沈む：わずかに縮んで下がる（止まっていれば変形なし＝線がにじまない）
    juce::Graphics::ScopedSaveState saved (g);
    if (std::abs (pressed) > 0.002f)
    {
        const auto k = 1.0f - motion::key::pressScale * juce::jmax (0.0f, pressed);
        g.addTransform (juce::AffineTransform::scale (k, k, bounds.getCentreX(), bounds.getCentreY())
                            .translated (0.0f, 0.5f * pressed));
    }

    // --- 本体 ---
    const bool recCovers = kind == Kind::rec && lit >= 1.0f;   // タリー赤で下が隠れる
    if (kind == Kind::ghost && ! s.over && ! s.down)
    {
        // 地に溶ける：本体なし
    }
    else if (! recCovers)
    {
        auto bodyState = s;
        if (latched) bodyState.down = true;   // ラッチ中は沈んだまま
        if (kind == Kind::rec) bodyState.on = false;
        paint::keycap (g, bounds, bodyState);

        if (latchColour.has_value() && lit > 0.0f)
        {
            g.setColour (latchColour->get().withAlpha (0.12f * lit));
            g.fillRoundedRectangle (bounds.reduced (1.5f), metrics::keyRadius - 1.0f);
        }
    }

    // 録音キー：タリー赤（消える時は余韻で薄れる）
    if (kind == Kind::rec && lit > 0.0f)
    {
        auto r = bounds.reduced (0.5f);
        g.setColour (colours::rec.withAlpha (0.25f * lit));
        g.fillRoundedRectangle (r.expanded (2.0f), metrics::keyRadius + 2.0f);
        g.setGradientFill (juce::ColourGradient (colours::rec.brighter (0.15f).withMultipliedAlpha (lit), r.getX(), r.getY(),
                                                 colours::rec.darker (0.25f).withMultipliedAlpha (lit), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, metrics::keyRadius);
        g.setColour (colours::shadow (0.4f * lit));
        g.drawRoundedRectangle (r, metrics::keyRadius, 1.0f);
    }

    // --- 中身 ---
    const auto pad = juce::jmin (sidePad, bounds.getWidth() * 0.15f);
    auto content = bounds.reduced (pad, 0.0f);
    const float drop = latched ? 1.0f : (preview ? (s.down ? 1.0f : 0.0f) : juce::jlimit (-0.5f, 1.2f, pressed));
    content.translate (0.0f, drop);

    juce::Colour fg = s.enabled ? colours::text : colours::textMute;
    if (kind == Kind::ghost && ! s.over) fg = colours::textDim;
    if (kind == Kind::rec && lit > 0.0f) fg = fg.interpolatedWith (colours::onRec(), lit);
    if (latchColour.has_value() && lit > 0.0f) fg = fg.interpolatedWith (*latchColour, lit);

    const bool iconOnly = getButtonText().isEmpty() && ! ledColour.has_value();

    if (ledColour.has_value())
    {
        const auto slot = content.removeFromLeft (ledSlot);
        paint::led (g, { slot.getX() + ledRadius, slot.getCentreY() }, ledRadius, *ledColour, lit);
    }

    if (icon.has_value())
    {
        const auto size = bounds.getHeight() * (iconOnly ? 0.46f : 0.5f);
        juce::Rectangle<float> iconArea;
        if (iconOnly)
            iconArea = bounds.withSizeKeepingCentre (size, size).translated (0.0f, drop);
        else
            iconArea = content.removeFromLeft (size).withSizeKeepingCentre (size, size);

        juce::Colour ic = iconColour.has_value() ? iconColour->get() : fg;
        if (kind == Kind::rec)
            ic = juce::Colour (colours::rec).interpolatedWith (colours::onRec(), lit);
        if (! s.enabled) ic = ic.withMultipliedAlpha (0.4f);

        // REC 中：キーの LED（●）がゆっくり呼吸する（DESIGN 4.10「キー」）。動きを減らす設定なら止める
        if (recOn && breathes && ! preview)
        {
            const auto b = motion::prefersReducedMotion() ? 1.0f : motion::key::breathe (motion::now());
            paint::glow (g, iconArea.expanded (iconArea.getWidth() * (0.35f + 0.45f * b)), colours::onRec().withAlpha (0.30f * b));
            ic = ic.withMultipliedAlpha (0.75f + 0.25f * b);
        }

        drawIcon (g, *icon, iconArea, ic);

        if (! iconOnly)
            content.removeFromLeft (6.0f);
    }

    if (getButtonText().isNotEmpty())
    {
        g.setColour (fg);
        g.setFont (labelFont);
        const auto just = (ledColour.has_value() || icon.has_value()) ? juce::Justification::centredLeft
                                                                       : juce::Justification::centred;
        g.drawText (getButtonText(), content, just, false);
    }

    // フォーカスの枠（キーボードで移ってきた時だけ。DESIGN 4.10.1 FC）
    if (focusRing && hasKeyboardFocus (false))
    {
        g.setColour (colours::signal);
        g.drawRoundedRectangle (bounds.reduced (1.0f), metrics::keyRadius, 2.0f);
    }
}

//==============================================================================
SegmentedKeys::SegmentedKeys (juce::StringArray opts, int sel, colours::Tone led)
    : options (std::move (opts)), selected (sel), ledColour (led), labelFont (sans (12.0f, Weight::medium)),
      slide ((float) sel)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
}

void SegmentedKeys::setSelected (int index, juce::NotificationType n)
{
    if (index == selected || ! juce::isPositiveAndBelow (index, options.size()))
        return;

    selected = index;
    if (isShowing())
        startAnimating();   // キーキャップが滑る
    else
        slide.snap ((float) index);
    repaint();
    if (n != juce::dontSendNotification && onChange)
        onChange (selected);
}

bool SegmentedKeys::advanceAnimation (float dt)
{
    const bool reduced = motion::prefersReducedMotion();
    const auto target = (float) selected;

    if (! isShowing())
    {
        slide.snap (target);
        ledLevel = 1.0f;
        return false;
    }

    slide.step (target, dt, motion::segment::springK, motion::segment::springC, reduced);
    const bool landed = reduced || motion::segment::landed (slide, target);
    if (landed) slide.snap (target);

    // 滑っている間は消え、止まってから点く（約 0.15 秒）
    ledLevel = landed ? motion::approach (ledLevel, 1.0f, 20.0f, dt, reduced)
                      : motion::approach (ledLevel, 0.0f, 30.0f, dt, reduced);
    if (ledLevel > 0.99f) ledLevel = 1.0f;
    repaint();
    return ! (landed && ledLevel >= 1.0f);
}

int SegmentedKeys::idealWidth() const
{
    float w = 6.0f;
    float widest = 0.0f;
    for (auto& o : options)
        widest = juce::jmax (widest, textWidth (labelFont, o));
    w += (widest + 2.0f * sidePad + ledSlot + 4.0f) * (float) options.size();
    return (int) std::ceil (w);
}

juce::Rectangle<float> SegmentedKeys::segmentBounds (float i) const
{
    const auto r = getLocalBounds().toFloat().reduced (3.0f);
    const auto w = r.getWidth() / (float) juce::jmax (1, options.size());
    return { r.getX() + w * i, r.getY(), w, r.getHeight() };
}

int SegmentedKeys::indexAt (juce::Point<int> p) const
{
    for (int i = 0; i < options.size(); ++i)
        if (segmentBounds ((float) i).contains (p.toFloat()))
            return i;
    return -1;
}

void SegmentedKeys::paint (juce::Graphics& g)
{
    const auto frame = getLocalBounds().toFloat();
    paint::inset (g, frame, metrics::keyRadius + 1.0f);

    const auto x = slide.x;   // キーキャップの位置（滑っている途中は小数）

    // 区切り（キーキャップの両隣は描かない）
    for (int i = 1; i < options.size(); ++i)
    {
        const auto b = (float) i;
        if (b >= x - 0.5f && b <= x + 1.5f)
            continue;
        const auto seg = segmentBounds (b);
        paint::vline (g, seg.getX(), seg.getY() + 6.0f, seg.getBottom() - 6.0f, colours::line);
    }

    // 浮いたキーキャップ（行き過ぎても枠の外には出さない）
    {
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (frame.reduced (2.0f).toNearestInt());
        paint::keycap (g, segmentBounds (x), { selected == hover, false, true, isEnabled() });
    }

    for (int i = 0; i < options.size(); ++i)
    {
        const auto seg = segmentBounds ((float) i);

        if (i == selected)
        {
            // LED と文字をひとかたまりで中央に置く。LED はキーキャップと一緒に動き、止まってから点く
            const auto tw = textWidth (labelFont, options[i]);
            auto group = seg.withSizeKeepingCentre (juce::jmin (seg.getWidth() - 4.0f, ledSlot + tw), seg.getHeight());
            const auto slot = group.removeFromLeft (ledSlot);
            const auto ledX = slot.getX() + ledRadius + (segmentBounds (x).getX() - seg.getX());
            paint::led (g, { ledX, slot.getCentreY() }, ledRadius, ledColour, ledLevel);
            g.setColour (colours::text);
            g.setFont (labelFont);
            g.drawText (options[i], group, juce::Justification::centredLeft, false);
        }
        else
        {
            g.setColour (i == hover ? colours::text.withAlpha (0.9f) : colours::textDim);
            g.setFont (labelFont);
            g.drawText (options[i], seg, juce::Justification::centred, false);
        }
    }
}

void SegmentedKeys::mouseMove (const juce::MouseEvent& e) { hover = indexAt (e.getPosition()); repaint(); }
void SegmentedKeys::mouseExit (const juce::MouseEvent&)   { hover = -1; repaint(); }
void SegmentedKeys::mouseDown (const juce::MouseEvent& e) { setSelected (indexAt (e.getPosition())); }
} // namespace vb
