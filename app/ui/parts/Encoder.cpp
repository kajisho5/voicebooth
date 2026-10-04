#include "Encoder.h"
#include "Icons.h"

namespace vb
{
namespace
{
    constexpr int ledCount = 25;
    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    constexpr float endAngle   =  juce::MathConstants<float>::pi * 0.75f;
}

Encoder::Encoder (double min, double max, double value, double step, bool bi, colours::Tone led)
    : bipolar (bi), ledColour (led), defaultValue (bi ? 0.0 : value)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (min, max, step);
    setValue (value, juce::dontSendNotification);
    setRotaryParameters (startAngle + juce::MathConstants<float>::twoPi, endAngle + juce::MathConstants<float>::twoPi, true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    focus::tabOnly (*this);   // Tab で移って ← → ↑ ↓ で動かす（#28）
    shown.snap ((float) value);
}

void Encoder::valueChanged()
{
    // 指標・LED の輪・数値はばねで追う
    if (isShowing() && ! motion::prefersReducedMotion())
        startAnimating();
    else
    {
        shown.snap ((float) getValue());
        if (onShownChange) onShownChange();
    }
}

void Encoder::mouseDown (const juce::MouseEvent& e)
{
    focus::handBack (*this);   // Tab で移っていたフォーカスはメイン画面へ返す（#28）
    if (! isEnabled()) return;
    raw = motion::encoder::detent (defaultValue, range()).toRaw (getValue());   // 掴んだ所から相対
    lastY = e.position.y;
    lastDragMs = juce::Time::getMillisecondCounterHiRes();
}

void Encoder::mouseDrag (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto dy = lastY - e.position.y;   // 上が増える
    const auto ms = nowMs - lastDragMs;
    lastY = e.position.y;
    lastDragMs = nowMs;

    const auto detent = motion::encoder::detent (defaultValue, range());
    raw = juce::jlimit (detent.toRaw (getMinimum()), detent.toRaw (getMaximum()),
                        raw + motion::encoder::dragDelta (dy, ms, range(), e.mods.isShiftDown()));
    const auto before = getValue();
    setValue (detent.toValue (raw), juce::sendNotificationSync);   // 刻み（setRange の step）は Slider が合わせる

    // 既定値でカチッと止まった：見出しを一瞬光らせる
    if (juce::exactlyEqual (getValue(), defaultValue) && ! juce::exactlyEqual (before, defaultValue))
    {
        flash = 1.0f;
        startAnimating();
    }
}

void Encoder::mouseUp (const juce::MouseEvent&)
{
    repaint();
}

void Encoder::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled()) return;
    if (! juce::exactlyEqual (getValue(), defaultValue))
        flash = 1.0f;
    setValue (defaultValue, juce::sendNotificationSync);   // 値はすぐ既定値、見た目はばねで戻る
    startAnimating();
}

bool Encoder::advanceAnimation (float dt)
{
    const bool reduced = motion::prefersReducedMotion();
    const auto target = (float) getValue();

    if (! isShowing() || reduced)
    {
        shown.snap (target);
        flash = 0.0f;
        repaint();
        if (onShownChange) onShownChange();
        return false;
    }

    shown.step (target, dt, motion::encoder::springK, motion::encoder::springC);
    const bool rest = shown.atRest (target, 0.002f * (float) range(), 0.02f * (float) range());
    if (rest) shown.snap (target);
    flash = juce::jmax (0.0f, flash - dt * 1.4f);

    repaint();
    if (onShownChange) onShownChange();
    return ! rest || flash > 0.0f;
}

void Encoder::paint (juce::Graphics& g)
{
    const auto size = (float) juce::jmin (getWidth(), getHeight());
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto ringR = size * 0.5f - 3.0f;
    const auto dotR = juce::jmax (1.2f, size * 0.021f);
    const bool hover = previewHover || isMouseOverOrDragging();

    // --- LED リング ---
    // 外から通知なしで値が変わることもあるので、動いていない時は値そのものに合わせる
    if (! isAnimating())
        shown.snap ((float) getValue());
    const auto pos = (float) juce::jlimit (0.0, 1.0, valueToProportionOfLength (juce::jlimit (getMinimum(), getMaximum(), getShownValue())));
    const auto from = bipolar ? 0.5f : 0.0f;
    const auto lo = juce::jmin (from, pos), hi = juce::jmax (from, pos);

    for (int i = 0; i < ledCount; ++i)
    {
        const auto k = (float) i / (float) (ledCount - 1);
        const auto ang = startAngle + k * (endAngle - startAngle);
        const auto pt = c.getPointOnCircumference (ringR, ang);
        const auto halfStep = 0.5f / (float) (ledCount - 1);
        const bool lit = (k >= lo - halfStep && k <= hi + halfStep) && (hi - lo > 0.001f || std::abs (k - from) < halfStep);

        if (lit)
        {
            g.setColour (ledColour.get().withAlpha (0.16f));
            g.fillEllipse (juce::Rectangle<float> (dotR * 3.6f, dotR * 3.6f).withCentre (pt));
            g.setColour (ledColour);
        }
        else
        {
            g.setColour (colours::line);
        }
        g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (pt));
    }

    // --- 本体 ---
    const auto bodyR = ringR - dotR * 2.0f - 5.0f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);

    g.setColour (colours::shadow (0.45f));
    g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.0f));

    const auto top = hover ? colours::raisedHi.brighter (0.05f) : colours::raisedHi;
    g.setGradientFill (juce::ColourGradient (top, c.x, body.getY(), colours::raised.darker (0.35f), c.x, body.getBottom(), false));
    g.fillEllipse (body);

    // ローレット（外周の細かい刻み）
    g.setColour (colours::shadow (0.28f));
    for (int i = 0; i < 36; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine ({ c.getPointOnCircumference (bodyR - 2.5f, a), c.getPointOnCircumference (bodyR - 0.5f, a) }, 0.8f);
    }

    g.setColour (colours::shadow (0.6f));
    g.drawEllipse (body, 1.0f);
    g.setColour (colours::highlight (0.07f));
    g.drawEllipse (body.reduced (3.5f), 1.0f);

    // 指標
    const auto ang = startAngle + pos * (endAngle - startAngle);
    g.setColour (colours::text);
    g.drawLine ({ c.getPointOnCircumference (bodyR * 0.30f, ang), c.getPointOnCircumference (bodyR * 0.78f, ang) }, 2.2f);
    ring.paint (g, *this, getLocalBounds().toFloat(), metrics::keyRadius);
}

//==============================================================================
EncoderBlock::EncoderBlock (const juce::String& l, double min, double max, double value, double step,
                            std::function<juce::String (double)> fmt, const juce::String& u,
                            bool bipolar, colours::Tone led)
    : label (l), unit (u), format (std::move (fmt)), enc (min, max, value, step, bipolar, led)
{
    enc.setTitle (label);   // 読み上げの名前（#28）
    enc.onValueChange = [this] { repaint(); if (onChange) onChange (enc.getValue()); };
    enc.onShownChange = [this] { repaint (valueArea.getUnion (captionArea)); };
    addAndMakeVisible (enc);
}

void EncoderBlock::resized()
{
    auto r = getLocalBounds();
    labelArea = r.removeFromTop (18);
    captionArea = r.removeFromBottom (16);
    valueArea = r.removeFromBottom (26);
    const auto s = juce::jmin (r.getWidth(), r.getHeight());
    enc.setBounds (r.withSizeKeepingCentre (s, s));
}

void EncoderBlock::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (sans (11.5f, Weight::medium));
    g.drawText (label, labelArea, juce::Justification::centred, false);

    if (locked)
    {
        const auto w = textWidth (sans (11.5f, Weight::medium), label);
        drawIcon (g, Icon::lock, juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ (float) labelArea.getCentreX() + w * 0.5f + 10.0f,
                                                                                     (float) labelArea.getCentreY() }), colours::warn);
    }

    // 値（Mono）＋単位。ばねで追っている途中の値を刻みに合わせて出す（数値がなめらかに追う）
    const auto step = enc.getInterval() > 0.0 ? enc.getInterval() : 1.0;
    const auto shownValue = enc.getMinimum() + std::round ((enc.getShownValue() - enc.getMinimum()) / step) * step;
    const auto value = format ? format (shownValue) : juce::String (shownValue);
    const auto vf = mono (19.0f, Weight::semibold);
    const auto uf = mono (11.0f, Weight::medium);
    const auto vw = textWidth (vf, value), uw = unit.isEmpty() ? 0.0f : textWidth (uf, unit) + 3.0f;
    auto r = valueArea.toFloat().withSizeKeepingCentre (vw + uw, (float) valueArea.getHeight());

    g.setColour (colours::text);
    g.setFont (vf);
    g.drawText (value, r.removeFromLeft (vw), juce::Justification::centredLeft, false);
    if (unit.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (uf);
        g.drawText (unit, r.withTrimmedLeft (3.0f).translated (0.0f, 2.0f), juce::Justification::centredLeft, false);
    }

    // 既定値に吸い付いた直後は補足（「原速」など）が一瞬点灯色になる
    g.setColour (colours::textMute.interpolatedWith (colours::signal, enc.getDetentFlash()));
    g.setFont (sans (10.5f));
    g.drawText (caption, captionArea, juce::Justification::centredTop, true);
}
} // namespace vb
