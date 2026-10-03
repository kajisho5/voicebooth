#include "StatusBar.h"
#include "parts/Icons.h"
#include "../models/ModelDownloader.h"

namespace vb
{
StatusBar::StatusBar (UiSession& u) : SessionView (u)
{
    mini.setLevels (u->inputPeakDb, u->inputRmsDb, u->inputPeakHoldDb, u->inputClipped);
    mini.onClick = [this] { session.resetInputClip(); };
    mini.setTooltip (tr ("meter.clip.tooltip"));
    addAndMakeVisible (mini);
}

void StatusBar::onSessionChanged (juce::uint32 c)
{
    if (c & (change::meter | change::device))
    {
        const auto& s = state();
        mini.setLevels (s.inputPeakDb, s.inputRmsDb, s.inputPeakHoldDb, s.inputClipped);
    }
    if (c & (change::transport | change::view | change::practice | change::mode | change::device | change::song | change::meter))
        repaint();

    // 新しいバージョンの知らせが出た：右から滑り込み、LED が 2 回点滅
    if ((c & change::device) && state().updateVersion != noticeVersion)
    {
        noticeVersion = state().updateVersion;
        if (noticeVersion.isNotEmpty())
        {
            chipSlide.snap (0.0f);
            chipShownAt = motion::now();
            startAnimating();
        }
    }
}

void StatusBar::mouseUp (const juce::MouseEvent& e)
{
    if (updateChip.contains (e.position) && onUpdateClicked)
        onUpdateClicked();
}

void StatusBar::mouseMove (const juce::MouseEvent& e)
{
    const bool over = updateChip.contains (e.position);
    setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    setChipHover (over);
}

void StatusBar::mouseExit (const juce::MouseEvent&)
{
    setChipHover (false);
}

void StatusBar::setChipHover (bool h)
{
    if (h == chipHover) return;
    chipHover = h;
    startAnimating();
}

bool StatusBar::advanceAnimation (float dt)
{
    const bool reduced = motion::prefersReducedMotion();
    if (! isShowing() || reduced)
    {
        chipSlide.snap (1.0f);
        chipLift = chipHover ? 1.0f : 0.0f;
        chipShownAt = -10.0;
        repaint();
        return false;
    }

    chipSlide.step (1.0f, dt, motion::notice::springK, motion::notice::springC);
    const bool landed = chipSlide.atRest (1.0f, 0.002f, 0.02f);
    if (landed) chipSlide.snap (1.0f);
    chipLift = motion::approach (chipLift, chipHover ? 1.0f : 0.0f, 22.0f, dt);
    if (std::abs (chipLift - (chipHover ? 1.0f : 0.0f)) < 0.01f) chipLift = chipHover ? 1.0f : 0.0f;
    const bool blinking = motion::now() - chipShownAt < motion::notice::blinkSeconds + 0.05;

    // 知らせの所だけ描き直す（滑り込む前の右側も含めて）
    if (! updateChip.isEmpty())
        repaint (updateChip.withRight ((float) getWidth()).expanded (8.0f, 4.0f).getSmallestIntegerContainer());
    else
        repaint();

    return ! landed || blinking || ! juce::approximatelyEqual (chipLift, chipHover ? 1.0f : 0.0f);
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

    // レイテンシ（録音位置の補正に使う値）：申告値・手入力はそう書く。実測はそのまま。UI_MOCK はダミー
    const auto ld = latencyDisplay (s);
    const auto latency = ! ld.known  ? juce::String ("-")
                       : ld.reported ? tr ("status.latency.reported", juce::String (ld.ms, 1))
                       : ld.manual   ? tr ("status.latency.manual", juce::String (ld.ms, 1))
                                     : juce::String (ld.ms, 1) + " ms";

    item ({}, formatDb (s.inputPeakDb) + " dBFS", colours::text);
    item (tr ("status.latency"), latency, colours::text);
    item (tr ("status.recTo"), s.recMode == project::RecMode::delivery ? tr ("status.recTo.delivery") : tr ("status.recTo.practice"),
          s.isRecording ? colours::rec : colours::text);
    item (tr ("status.export"), tr ("status.export.value", formatKhz (s.sampleRate()), formatBits (s.project.bitDepthExport)), colours::text);
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
        const auto w = textWidth (uf, txt) + 44.0f;
        updateChip = r.removeFromRight (w).withSizeKeepingCentre (w, 20.0f);
        r.removeFromRight (8.0f);

        // 滑り込み（自分の場所の中だけで見せる：隣の札に重ならない）・ホバーで少し浮く
        const auto slide = isAnimating() ? chipSlide.x : 1.0f;
        const auto lift = isAnimating() ? chipLift : (chipHover ? 1.0f : 0.0f);
        // 行き過ぎ（ばね）は左の札に重ならない 6 px まで
        const auto box = updateChip.translated (juce::jmax (-6.0f, (1.0f - slide) * (w + 8.0f)), -1.0f * lift);

        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (updateChip.withRight ((float) getWidth()).withTrimmedLeft (-6.0f).expanded (0.0f, 4.0f).getSmallestIntegerContainer());
        g.setOpacity (juce::jlimit (0.0f, 1.0f, slide * 1.4f));

        if (lift > 0.0f)
            paint::glow (g, box.expanded (8.0f, 6.0f).translated (0.0f, 3.0f), colours::signal.withAlpha (0.10f * lift));
        g.setColour (colours::signal.withAlpha (0.16f + 0.10f * lift));
        g.fillRoundedRectangle (box, 3.0f);

        auto inner = box;
        const auto blink = isAnimating() ? motion::notice::led (motion::now() - chipShownAt) : 1.0f;
        paint::led (g, { inner.getX() + 9.0f, inner.getCentreY() }, 2.3f, colours::signal, blink);
        inner.removeFromLeft (14.0f);
        drawIcon (g, Icon::download, inner.removeFromLeft (20.0f).withSizeKeepingCentre (12.0f, 12.0f), colours::signal);
        g.setColour (colours::signal);
        g.setFont (uf);
        g.drawText (txt, inner, juce::Justification::centredLeft, false);
    }

    // ボーカル分離（B16。裏で進む）
    if (s.separating)
    {
        const auto pct = juce::roundToInt (s.separationProgress * 100.0f);
        chip (s.separationEta > 0.0 ? tr ("status.separatingEta", pct, juce::jmax (1, juce::roundToInt (s.separationEta / 60.0)))
                                    : tr ("status.separating", pct), colours::ref);
    }

    // 分離モデルのダウンロード（B16。画面を閉じても裏で続く）
    {
        using DS = models::DownloadStatus::Stage;
        const auto st = s.modelDl.stage;
        if (st == (int) DS::downloading || st == (int) DS::verifying || st == (int) DS::waiting || st == (int) DS::interrupted)
        {
            const auto pct = s.modelDl.size > 0 ? (int) (100 * s.modelDl.received / s.modelDl.size) : 0;
            chip (tr (s.modelDl.paused ? "status.modelDlPaused" : "status.modelDl", pct), colours::ref);
        }
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

    // 入力が使えない理由（許可なし・デバイスなし・開けない・完全な無音）
    if (const auto problem = inputProblemShort (s); problem.isNotEmpty())
    {
        const bool severe = s.input.problem == audio::InputProblem::permissionDenied
                         || s.input.problem == audio::InputProblem::openFailed
                         || s.input.problem == audio::InputProblem::noChannels
                         || s.input.problem == audio::InputProblem::stalled;
        chip (problem, severe && ! s.input.open ? colours::bad : colours::warn);
    }

    const auto& o = s.output;
    if (! o.open)
    {
        g.setColour (colours::bad);
        g.setFont (sans (10.5f));
        g.drawText (o.stalled ? tr ("status.deviceStalled") : tr ("status.noOutput", o.error),
                    r.withTrimmedRight (8.0f), juce::Justification::centredRight, true);
        return;
    }

    if (o.converting)
        chip (tr ("status.converting"), colours::warn);

    // 出力：デバイス名・SR・バッファ（右寄せ）
    const auto value = o.deviceName + "   " + formatKhz (juce::roundToInt (o.sampleRate)) + " kHz   " + juce::String (o.bufferSize);
    const auto label = tr ("status.output");
    const auto vw = juce::jmin (textWidth (vf, value) + 4.0f, r.getWidth() - textWidth (lf, label) - 8.0f);   // 長い機器名は省略
    g.setColour (colours::text);
    g.setFont (vf);
    g.drawText (value, r.removeFromRight (juce::jmax (0.0f, vw)), juce::Justification::centredRight, true);
    paint::microLabel (g, r.removeFromRight (textWidth (lf, label) + 8.0f), label, colours::textMute);
}
} // namespace vb
