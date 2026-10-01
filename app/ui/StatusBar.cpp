#include "StatusBar.h"
#include "parts/Icons.h"

namespace vb
{
StatusBar::StatusBar (UiSession& u) : SessionView (u)
{
    mini.setLevels (u->inputPeakDb, u->inputRmsDb, u->inputPeakHoldDb, false);
    addAndMakeVisible (mini);
}

void StatusBar::mouseUp (const juce::MouseEvent& e)
{
    if (updateChip.contains (e.position) && onUpdateClicked)
        onUpdateClicked();
}

void StatusBar::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (updateChip.contains (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void StatusBar::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 0);
    r.removeFromLeft ((int) textWidth (mono (9.5f, Weight::medium, 0.12f), tr ("label.input")) + 10);
    meterArea = r.removeFromLeft (120);
    mini.setBounds (meterArea.withSizeKeepingCentre (meterArea.getWidth(), 8));
}

void StatusBar::paint (juce::Graphics& g)
{
    const auto& s = state();
    g.fillAll (colours::bgDeep);
    paint::hline (g, 0.0f, 0.0f, (float) getWidth());

    auto r = getLocalBounds().reduced (metrics::pad, 0).toFloat();
    const auto lf = mono (9.5f, Weight::medium, 0.12f);
    paint::microLabel (g, r.removeFromLeft (textWidth (lf, tr ("label.input")) + 10.0f), tr ("label.input"), colours::textMute);
    r.removeFromLeft ((float) meterArea.getWidth() + 10.0f);

    const auto vf = mono (11.0f, Weight::medium);
    auto item = [&] (const juce::String& label, const juce::String& value, juce::Colour vc)
    {
        if (label.isNotEmpty())
            paint::microLabel (g, r.removeFromLeft (textWidth (lf, label) + 8.0f), label, colours::textMute);
        g.setColour (vc);
        g.setFont (vf);
        g.drawText (value, r.removeFromLeft (textWidth (vf, value) + 4.0f), juce::Justification::centredLeft, false);
        r.removeFromLeft (12.0f);
        paint::vline (g, r.getX(), r.getY() + 8.0f, r.getBottom() - 8.0f);
        r.removeFromLeft (13.0f);
    };

    const auto latencyMs = (double) s.latencySamples * 1000.0 / s.sampleRate();

    item ({}, juce::String (s.inputPeakDb, 1) + " dBFS", colours::text);
    item (tr ("status.latency"), juce::String (latencyMs, 1) + " ms", colours::text);
    item (tr ("status.recTo"), s.recMode == project::RecMode::delivery ? tr ("status.recTo.delivery") : tr ("status.recTo.practice"),
          s.isRecording ? colours::rec : colours::text);
    item (tr ("status.export"), tr ("status.export.value", formatKhz (s.sampleRate()), s.project.bitDepthExport), colours::text);
    if (s.mode != project::Mode::easy)
        item (tr ("status.driver"), s.driver + " " + juce::String (s.bufferSize), colours::textDim);

    // 右端：目立たせたい注意（UI MOCK / 入力はダミー / SR 変換中）を札で
    auto chip = [&] (const juce::String& txt, juce::Colour c)
    {
        const auto mf = mono (10.0f, Weight::semibold, 0.1f);
        const auto w = textWidth (mf, txt) + 22.0f;
        const auto box = r.removeFromRight (w).withSizeKeepingCentre (w, 18.0f);
        g.setColour (c.withAlpha (0.14f));
        g.fillRoundedRectangle (box, 3.0f);
        paint::led (g, { box.getX() + 8.0f, box.getCentreY() }, 2.3f, c, true);
        g.setColour (c);
        g.setFont (mf);
        g.drawText (txt, box.withTrimmedLeft (15.0f), juce::Justification::centredLeft, false);
        r.removeFromRight (8.0f);
    };

    // 新しいバージョン（押すと詳細。DESIGN 11.7）
    updateChip = {};
    if (s.updateVersion.isNotEmpty())
    {
        const auto txt = tr ("update.notice", s.updateVersion);
        const auto uf = sans (11.0f, Weight::semibold);
        const auto w = textWidth (uf, txt) + 34.0f;
        updateChip = r.removeFromRight (w).withSizeKeepingCentre (w, 20.0f);
        r.removeFromRight (8.0f);
        const bool hover = updateChip.contains (getMouseXYRelative().toFloat());
        g.setColour (colours::signal.withAlpha (hover ? 0.28f : 0.16f));
        g.fillRoundedRectangle (updateChip, 3.0f);
        auto inner = updateChip;
        drawIcon (g, Icon::download, inner.removeFromLeft (24.0f).withSizeKeepingCentre (12.0f, 12.0f), colours::signal);
        g.setColour (colours::signal);
        g.setFont (uf);
        g.drawText (txt, inner, juce::Justification::centredLeft, false);
    }

    if (! s.engineAttached)
    {
        // UI_MOCK ビルド：音は出ない
        chip (tr ("status.uiMock"), colours::warn);
        g.setColour (colours::textMute);
        g.setFont (sans (10.5f));
        g.drawText (tr ("status.noAudio"), r.withTrimmedRight (8.0f), juce::Justification::centredRight, true);
        return;
    }

    // 入力（メーター・レイテンシ）は B3 までダミー
    chip (tr ("status.inputMock"), colours::warn);

    const auto& o = s.output;
    if (! o.open)
    {
        g.setColour (colours::bad);
        g.setFont (sans (10.5f));
        g.drawText (tr ("status.noOutput", o.error), r.withTrimmedRight (8.0f), juce::Justification::centredRight, true);
        return;
    }

    if (o.converting)
        chip (tr ("status.converting"), colours::warn);

    // 出力：デバイス名・SR・バッファ（右寄せ）
    const auto value = o.deviceName + "   " + formatKhz (juce::roundToInt (o.sampleRate)) + " kHz   " + juce::String (o.bufferSize);
    const auto vw = textWidth (vf, value) + 4.0f;
    g.setColour (colours::text);
    g.setFont (vf);
    g.drawText (value, r.removeFromRight (vw), juce::Justification::centredRight, true);
    const auto label = tr ("status.output");
    paint::microLabel (g, r.removeFromRight (textWidth (lf, label) + 8.0f), label, colours::textMute);
}
} // namespace vb
