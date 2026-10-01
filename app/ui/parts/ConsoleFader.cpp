#include "ConsoleFader.h"

namespace vb
{
ConsoleFader::ConsoleFader (double value, juce::Colour line)
    : capLine (line)
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (0.0, 1.0, 0.01);
    setValue (value, juce::dontSendNotification);
    setDoubleClickReturnValue (true, value);
    setSliderSnapsToMousePosition (false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void ConsoleFader::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto cx = std::round (b.getCentreX()) - (meter >= 0.0f ? 4.0f : 0.0f);
    const auto top = b.getY() + capH * 0.5f;
    const auto bottom = b.getBottom() - capH * 0.5f;
    const auto pos = (float) valueToProportionOfLength (getValue());
    const auto capY = bottom - pos * (bottom - top);

    // 目盛り（10% 刻み、0/50/100 は長く）
    for (int i = 0; i <= 10; ++i)
    {
        const auto y = std::round (bottom - (float) i / 10.0f * (bottom - top));
        const bool major = (i % 5 == 0);
        const auto len = major ? 7.0f : 4.0f;
        g.setColour (major ? colours::textMute : colours::line);
        g.fillRect (juce::Rectangle<float> (cx - capW * 0.5f + 4.0f - len, y, len, 1.0f));
        if (meter < 0.0f)   // メーターがある時は右側をメーターに譲る
            g.fillRect (juce::Rectangle<float> (cx + capW * 0.5f - 4.0f, y, len, 1.0f));
    }

    // 溝
    const auto groove = juce::Rectangle<float> (cx - 2.0f, top - 2.0f, 4.0f, bottom - top + 4.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (groove, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (groove.withX (groove.getRight()).withWidth (1.0f));

    // 横のメーター（セグメント）
    if (meter >= 0.0f)
    {
        const auto mx = cx + capW * 0.5f + 6.0f;
        const auto segH = 3.0f, gap = 1.5f;
        const int n = (int) ((bottom - top) / (segH + gap));
        for (int i = 0; i < n; ++i)
        {
            const auto k = (float) (i + 1) / (float) n;
            const auto y = bottom - (float) (i + 1) * (segH + gap) + gap;
            const auto c = k > 0.9f ? colours::bad : (k > 0.75f ? colours::warn : colours::signal);
            paint::ledBar (g, { mx, y, 3.0f, segH }, c, k <= meter ? 1.0f : 0.0f);
        }
    }

    // キャップ
    const bool hover = previewHover || isMouseOverOrDragging();
    const auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ cx, capY });

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 2.5f);

    const auto capTop = hover ? juce::Colour (0xff4b4740) : juce::Colour (0xff403c36);
    g.setGradientFill (juce::ColourGradient (capTop, cap.getX(), cap.getY(), colours::raised.darker (0.2f), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.5f);

    // 指掛けの段差
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (cap.withHeight (cap.getHeight() * 0.42f).withY (cap.getY() + 1.0f).reduced (2.0f, 0.0f));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillRect (juce::Rectangle<float> (cap.getX() + 2.0f, cap.getY() + 1.0f, cap.getWidth() - 4.0f, 1.0f));

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (cap, 2.5f, 1.0f);

    // 中心線（チャンネル色）
    g.setColour (capLine);
    g.fillRect (juce::Rectangle<float> (cap.getX() + 3.0f, std::round (capY) - 1.0f, cap.getWidth() - 6.0f, 2.0f));
}

//==============================================================================
ChannelStrip::ChannelStrip (const juce::String& n, double value, float meterLevel,
                            juce::Colour capLine, bool ms, const juce::String& noteText)
    : name (n), note (noteText), slider (value, capLine), withMuteSolo (ms)
{
    slider.setMeter (meterLevel);
    slider.onValueChange = [this] { repaint (valueArea); };
    addAndMakeVisible (slider);

    mute.withLatch (colours::warn).withFont (mono (10.5f, Weight::semibold));
    solo.withLatch (colours::signal).withFont (mono (10.5f, Weight::semibold));
    mute.setTooltip (jp ("ミュート"));
    solo.setTooltip (jp ("ソロ"));
    addChildComponent (mute);
    addChildComponent (solo);
    mute.setVisible (ms);
    solo.setVisible (ms);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds();
    nameArea = r.removeFromTop (30);

    auto keys = r.removeFromBottom (22);
    if (withMuteSolo)
    {
        const auto kw = (keys.getWidth() - 4) / 2;
        mute.setBounds (keys.removeFromLeft (kw));
        solo.setBounds (keys.removeFromRight (kw));
    }

    r.removeFromBottom (6);
    valueArea = r.removeFromBottom (18);
    r.removeFromBottom (2);
    slider.setBounds (r);
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto names = nameArea;
    if (note.isNotEmpty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (9.5f));
        g.drawText (note, names.removeFromBottom (12), juce::Justification::centred, false);
    }

    g.setColour (colours::text.withAlpha (0.9f));
    g.setFont (sans (11.5f, Weight::medium));
    g.drawFittedText (name, names, juce::Justification::centredBottom, 2, 0.9f);

    g.setColour (colours::textDim);
    g.setFont (mono (12.0f, Weight::medium));
    g.drawText (juce::String (juce::roundToInt (slider.getValue() * 100.0)), valueArea, juce::Justification::centred, false);
}
} // namespace vb
