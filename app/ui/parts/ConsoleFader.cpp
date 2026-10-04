#include "ConsoleFader.h"

namespace vb
{
ConsoleFader::ConsoleFader (double value, colours::Tone line)
    : capLine (line)
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (0.0, 1.0, 0.0);   // 値は丸めない（DESIGN 4.10）。表示だけ整数にする
    setValue (value, juce::dontSendNotification);
    setSliderSnapsToMousePosition (false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    focus::tabOnly (*this);   // Tab で移って ← → ↑ ↓ で動かす（#28）
    shown.snap ((float) value);
    unityGlow = isAtUnity() ? 1.0f : 0.0f;
}

void ConsoleFader::valueChanged()
{
    // ドラッグ・ホイール・外からの値：つまみはその場へ。ダブルクリックの戻りだけばねで動かす
    if (! springing)
        shown.snap ((float) getValue());
    if (springing || ! juce::approximatelyEqual (unityGlow, isAtUnity() ? 1.0f : 0.0f))
        startAnimating();
}

void ConsoleFader::mouseDown (const juce::MouseEvent& e)
{
    focus::handBack (*this);   // Tab で移っていたフォーカスはメイン画面へ返す（#28）
    if (! isEnabled()) return;
    springing = false;
    shown.snap ((float) getValue());
    raw = motion::fader::detent (travel()).toRaw (getValue());   // 掴んだ所から相対で動かす（飛ばない）
    lastY = e.position.y;
}

void ConsoleFader::mouseDrag (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    const auto dy = e.position.y - lastY;
    lastY = e.position.y;
    raw += motion::fader::dragDelta (dy, travel(), e.mods.isShiftDown());
    setValue (motion::fader::valueFromRaw (raw, motion::fader::detent (travel())), juce::sendNotificationSync);
}

void ConsoleFader::mouseUp (const juce::MouseEvent&)
{
    repaint();
}

void ConsoleFader::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled()) return;
    // 値はすぐ 0 dB ちょうど。つまみはばねで戻る
    springing = isShowing() && ! motion::prefersReducedMotion();
    setValue (motion::fader::unity, juce::sendNotificationSync);
    if (! springing) shown.snap ((float) motion::fader::unity);
    startAnimating();
}

bool ConsoleFader::advanceAnimation (float dt)
{
    const bool reduced = motion::prefersReducedMotion();
    const auto target = (float) getValue();
    const auto glowTarget = isAtUnity() ? 1.0f : 0.0f;

    if (! isShowing() || reduced)
    {
        shown.snap (target);
        springing = false;
        unityGlow = glowTarget;
        repaint();
        return false;
    }

    if (springing)
    {
        shown.step (target, dt, motion::fader::springK, motion::fader::springC);
        if (shown.atRest (target, 0.0005f, 0.01f))
        {
            shown.snap (target);
            springing = false;
        }
    }

    unityGlow = motion::approach (unityGlow, glowTarget, 25.0f, dt);
    if (std::abs (unityGlow - glowTarget) < 0.01f) unityGlow = glowTarget;
    repaint();
    return springing || ! juce::approximatelyEqual (unityGlow, glowTarget);
}

void ConsoleFader::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto cx = std::round (b.getCentreX()) - (meter >= 0.0f ? 4.0f : 0.0f);
    const auto top = b.getY() + capH * 0.5f;
    const auto bottom = b.getBottom() - capH * 0.5f;
    // 外から通知なしで値が変わることもあるので、動いていない時は値そのものに合わせる
    if (! isAnimating())
    {
        shown.snap ((float) getValue());
        unityGlow = isAtUnity() ? 1.0f : 0.0f;
    }
    const auto pos = juce::jlimit (-0.05f, 1.05f, shown.x);   // ばねで少し行き過ぎてもよい
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
    g.setColour (colours::isLight() ? colours::lineHi : juce::Colours::black);
    g.fillRoundedRectangle (groove, 2.0f);
    g.setColour (colours::highlight (0.05f));
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

    g.setColour (colours::shadow (0.5f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 2.5f);

    // キャップはキーより一段明るい（Booth では #403C36 / ホバー #4B4740 相当）
    const auto capTop = colours::isLight() ? (hover ? colours::raisedHi : colours::raised)
                                           : (hover ? colours::lineHi.brighter (0.02f) : colours::raisedHi.interpolatedWith (colours::lineHi, 0.62f));
    g.setGradientFill (juce::ColourGradient (capTop, cap.getX(), cap.getY(), colours::raised.darker (0.2f), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.5f);

    // 指掛けの段差
    g.setColour (colours::shadow (0.25f));
    g.fillRect (cap.withHeight (cap.getHeight() * 0.42f).withY (cap.getY() + 1.0f).reduced (2.0f, 0.0f));
    g.setColour (colours::highlight (0.10f));
    g.fillRect (juce::Rectangle<float> (cap.getX() + 2.0f, cap.getY() + 1.0f, cap.getWidth() - 4.0f, 1.0f));

    g.setColour (colours::shadow (0.6f));
    g.drawRoundedRectangle (cap, 2.5f, 1.0f);

    // 中心線（チャンネル色）。0 dB ちょうどの時は光る（作っておいた光の画像を重ねる）
    const auto lineRect = juce::Rectangle<float> (cap.getX() + 3.0f, std::round (capY) - 1.0f, cap.getWidth() - 6.0f, 2.0f);
    if (unityGlow > 0.0f)
        paint::glow (g, lineRect.expanded (7.0f, 6.0f), capLine.get().withAlpha (0.55f * unityGlow));
    g.setColour (capLine.get().withMultipliedAlpha (0.78f + 0.22f * unityGlow).brighter (0.25f * unityGlow));
    g.fillRect (lineRect);
    ring.paint (g, *this, getLocalBounds().toFloat(), metrics::keyRadius);
}

//==============================================================================
ChannelStrip::ChannelStrip (const juce::String& n, double value, float meterLevel,
                            colours::Tone capLine, bool ms, const juce::String& noteText)
    : name (n), note (noteText), slider (value, capLine), withMuteSolo (ms)
{
    slider.setMeter (meterLevel);
    slider.onValueChange = [this] { repaint (valueArea); if (onFaderChange) onFaderChange(); };
    addAndMakeVisible (slider);

    mute.withLatch (colours::warn).withFont (mono (10.5f, Weight::semibold));
    solo.withLatch (colours::signal).withFont (mono (10.5f, Weight::semibold));
    mute.setTooltip (tr ("monitor.mute"));
    solo.setTooltip (tr ("monitor.solo"));
    // 読み上げの名前（#28）：フェーダーはチャンネルの名前、M / S は「チャンネル + ミュート / ソロ」
    const auto spoken = name.replace ("\n", " ").trim();
    slider.setTitle (spoken);
    mute.setTitle (spoken + " " + tr ("monitor.mute"));
    solo.setTitle (spoken + " " + tr ("monitor.solo"));
    addChildComponent (mute);
    addChildComponent (solo);
    mute.setVisible (ms);
    solo.setVisible (ms);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds();
    // 2 行の名前（お手本 / Main）と小さな注記。文字を大きくしている分（1920x1080）だけ高くする
    nameArea = r.removeFromTop (30 + juce::roundToInt ((note.isNotEmpty() ? 20.0f : 10.0f) * textBoostAmount()));

    auto keys = r.removeFromBottom (22);
    if (withMuteSolo && ! soloShown)
    {
        mute.setBounds (keys);
    }
    else if (withMuteSolo)
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
        // 1 行に入らなければ 2 行に折り返す（「Oreilles seules」「Chỉ để nghe」が 1 行で切れていた。#19）。
        // 2 行にする高さが無ければ（低い画面）、1 行のまま字の幅を詰める
        const auto f = sans (9.5f);
        const auto lineH = 12 + juce::roundToInt (3.0f * textBoostAmount());
        const auto lines = textWidth (f, note) > (float) names.getWidth() && names.getHeight() >= lineH * 2 + 14 ? 2 : 1;
        g.setColour (colours::textMute);
        g.setFont (f);
        g.drawFittedText (note, names.removeFromBottom (lineH * lines), juce::Justification::centred, lines, 0.8f);
    }

    // 1 行に入らない名前は空白で 2 行に分ける（韓国語の「내 목소리」が「내 목소 / 리」と音節の途中で分かれていた。#19）
    const auto nf = sans (11.5f, Weight::medium);
    auto shown = name;
    if (! shown.containsChar ('\n') && shown.containsChar (' ') && textWidth (nf, shown) > (float) names.getWidth())
    {
        // 真ん中に近い空白で分ける
        int best = -1;
        for (int i = shown.indexOfChar (' '); i >= 0; i = shown.indexOfChar (i + 1, ' '))
            if (best < 0 || std::abs (i - shown.length() / 2) < std::abs (best - shown.length() / 2))
                best = i;
        shown = shown.substring (0, best) + "\n" + shown.substring (best + 1);
    }
    g.setColour (colours::text.withAlpha (0.9f));
    g.setFont (nf);
    g.drawFittedText (shown, names, juce::Justification::centredBottom, 2, 0.85f);

    g.setColour (colours::textDim);
    g.setFont (mono (12.0f, Weight::medium));
    g.drawText (juce::String (juce::roundToInt (slider.getValue() * 100.0)), valueArea, juce::Justification::centred, false);
}
} // namespace vb
