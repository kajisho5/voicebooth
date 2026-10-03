#include "PitchLane.h"
#include "SongMarks.h"

namespace vb
{
namespace
{
    constexpr int rulerH = 24;
    constexpr int footerH = 38;
    constexpr float minConfidence = 0.5f;
    constexpr double maxGapSeconds = 0.015;   // 15 ms 以上空いたら線を切る（嘘でつながない）
    constexpr float maxJumpSemitones = 4.0f;  // 隣の点と 4 半音より離れたら線を切る（縦の筋を描かない。1 点だけの外れは消える）
    constexpr float harmonyOffset = 4.0f;  // ダミーのハモリ（長 3 度上）

    bool isBlackKey (int midi)
    {
        const auto pc = ((midi % 12) + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    /** 信頼できる点だけを、途切れごとの区間に分ける */
    template <typename Fn>
    void forEachRun (const std::vector<dummy::PitchPoint>& pts, int64 from, int64 to, double sampleRate, Fn&& fn)
    {
        std::vector<const dummy::PitchPoint*> run;
        int64 last = 0;
        const auto maxGapSamples = (int64) (maxGapSeconds * sampleRate);

        auto flush = [&] { if (run.size() > 1) fn (run); run.clear(); };

        for (auto& p : pts)
        {
            if (p.sample < from || p.sample > to) continue;
            if (p.confidence < minConfidence) { flush(); continue; }
            if (! run.empty() && (p.sample - last > maxGapSamples || std::abs (p.midi - run.back()->midi) > maxJumpSemitones)) flush();
            run.push_back (&p);
            last = p.sample;
        }
        flush();
    }
}

PitchLane::PitchLane (UiSession& u, Actions& a)
    : SessionView (u), actions (a),
      octaveAlign (tr ("pitch.octaveAlign")),
      octaveUp (tr ("pitch.octaveUp")),
      fullRange (tr ("pitch.fullRange")),
      listenOriginal (tr ("pitch.listenOriginal"))
{
    for (auto* b : { &octaveAlign, &octaveUp, &fullRange, &listenOriginal })
    {
        b->withLed().withToggle (false).withFont (sans (11.5f, Weight::medium));
        addAndMakeVisible (b);
    }
    octaveAlign.setTooltip (tr ("pitch.octaveAlign.tooltip"));
    octaveUp.setTooltip (tr ("pitch.octaveUp.tooltip"));
    fullRange.setTooltip (tr ("pitch.fullRange.tooltip"));

    octaveAlign.onClick = [this] { session.setOctaveAlign (! state().octaveAlign); };
    octaveUp.onClick    = [this] { session.setOctaveUp (! state().octaveUp); };
    fullRange.onClick   = [this] { session.setFullRange (! state().fullRange); };
    // 原曲で聴く（聞き比べ・時間合わせの確認。お手本を入れて合わせられた時だけ押せる）
    listenOriginal.setTooltip (tr ("pitch.listenOriginal.tooltip"));
    listenOriginal.onClick = [this] { session.setListenOriginal (! state().listenOriginal); };

    onSessionChanged (change::all);
}

void PitchLane::onSessionChanged (juce::uint32 changes)
{
    const auto& s = state();
    octaveAlign.setToggleState (s.octaveAlign, juce::dontSendNotification);
    octaveUp.setToggleState (s.octaveUp, juce::dontSendNotification);
    fullRange.setToggleState (s.fullRange, juce::dontSendNotification);
    listenOriginal.setToggleState (s.listenOriginal, juce::dontSendNotification);
    listenOriginal.setEnabled (s.guideOriginal != nullptr);
    if (listenOriginal.isVisible() != s.engineAttached)
    {
        listenOriginal.setVisible (s.engineAttached);   // 見本（UI_MOCK）には原曲が無い
        resized();
    }

    if (changes & change::mode)
        resized();

    // このレーンに関わらない変更（入力メーター 30 Hz・知らせ・保存など）では描き直さない。
    // 再生ヘッドだけが動いたときは、前と今の位置の間と鍵盤だけ（#26）
    constexpr juce::uint32 unrelated = change::meter | change::notice | change::project | change::prefs
                                     | change::latency | change::recordFormat | change::device;
    const auto relevant = changes & ~unrelated;
    if (relevant == 0)
        return;
    if ((relevant & ~juce::uint32 (change::playhead)) != 0)
        staticDirty = true;
    const auto x = map().x (s.playhead);
    const auto dirty = relevant == change::playhead ? lane::playheadDirty (headX, x, getHeight()) : juce::Rectangle<int>();
    headX = x;
    if (dirty.isEmpty())
    {
        repaint();
        return;
    }
    repaint (dirty);
    // 鍵盤は光る鍵が変わったときだけ（毎フレーム頼むと、鍵盤から再生ヘッドまでがまとめて描き直される）
    if (const auto keys = litKeys(); keys != gutterKeys)
    {
        gutterKeys = keys;
        repaint (gutterArea);
    }
}

void PitchLane::resized()
{
    auto r = getLocalBounds();
    rulerArea = r.removeFromTop (rulerH);
    footerArea = r.removeFromBottom (footerH);
    gutterArea = r.removeFromLeft (metrics::gutter);
    plotArea = r;

    auto f = footerArea.reduced (metrics::pad, 0);
    for (auto* b : { &fullRange, &octaveUp, &octaveAlign, &listenOriginal })
    {
        if (! b->isVisible())
            continue;
        b->setSize (10, 26);
        const auto w = b->idealWidth();
        b->setBounds (f.removeFromRight (w).withSizeKeepingCentre (w, 26));
        f.removeFromRight (6);
    }
    f.removeFromRight (10);
    analysisArea = f.removeFromRight (state().mode == project::Mode::pro ? 300 : (state().mode == project::Mode::standard ? 140 : 0));
    legendArea = f.withTrimmedLeft (metrics::gutter - metrics::pad);
}

TimeMap PitchLane::map() const
{
    return lane::makeMap (state(), plotArea.toFloat());
}

float PitchLane::yForMidi (float midi) const
{
    const auto& s = state();
    const auto p = plotArea.toFloat();
    const auto k = (midi - ((float) s.lowMidi - 0.5f)) / (float) (s.highMidi - s.lowMidi + 1);
    return p.getBottom() - k * p.getHeight();
}

juce::Colour PitchLane::colourForCents (float cents) const
{
    const auto a = std::abs (cents);
    if (a <= state().pitchToleranceCents) return colours::signal;
    if (a <= 50.0f)                       return colours::warn;
    return colours::bad;
}

juce::Colour PitchLane::colourFor (const dummy::PitchPoint& p) const
{
    // お手本がまだ無い（B9 の前）：合っている・外れているは言えないので中立の色
    return p.judged ? colourForCents (p.centsOff) : (juce::Colour) colours::text;
}

// 練習でキーを変えている時（B11）は、お手本の線もその分ずらす（伴奏と同じキーで歌う）
// ハモリのお手本はまだ作れない。本物のアプリでハモリのトラックを選んでも、メインのお手本をそのまま出す（ずらした嘘の線を出さない）
// ハモリのトラックを選んでいて、ハモリのお手本がある（本物：分離でリードと分けられた時。見本：メインを長 3 度上げた物）
bool PitchLane::harmonyGuide() const
{
    const auto& s = state();
    return s.isHarmonySelected() && (! s.engineAttached || ! s.refPitchHarm.empty());
}
float PitchLane::mockHarmonyOffset() const { return harmonyGuide() && ! state().engineAttached ? harmonyOffset : 0.0f; }
float PitchLane::refOffset() const  { return mockHarmonyOffset() + (float) state().keyShift; }
float PitchLane::mineOffset() const { return mockHarmonyOffset() + (state().octaveUp ? 12.0f : 0.0f); }

//==============================================================================
int PitchLane::tagAt (juce::Point<float> p) const
{
    for (auto& t : lane::sectionTags (state(), map(), rulerArea.withTrimmedLeft (metrics::gutter).toFloat()))
        if (t.area.expanded (0.0f, 2.0f).contains (p))
            return t.index;
    return -1;
}

void PitchLane::mouseMove (const juce::MouseEvent& e)
{
    // 区間の札は指、範囲の端（つまんで動かせる）は左右の矢印
    if (tagAt (e.position) >= 0)
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else if (plotArea.contains (e.getPosition()) && lane::rangeEdgeAt (state(), map(), e.position.x) >= 0)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void PitchLane::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { lane::wheel (session, map(), e, w); }
void PitchLane::mouseMagnify (const juce::MouseEvent& e, float scale)                       { lane::magnify (session, map(), e, scale); }

void PitchLane::mouseDown (const juce::MouseEvent& e)
{
    draggingTag = -1;
    draggingRuler = false;

    if (rulerArea.contains (e.getPosition()))
    {
        const auto tag = tagAt (e.position);
        if (e.mods.isPopupMenu())
        {
            const auto screen = juce::Rectangle<int> (e.getScreenX(), e.getScreenY(), 1, 1);
            if (tag >= 0)
                marks::showSectionMenu (session, actions, tag, screen);
            else if (e.x >= plotArea.getX())
                marks::showRulerMenu (session, actions, map().sampleAt (e.position.x), ! e.mods.isAltDown(), screen);
            return;
        }

        if (tag >= 0)
        {
            draggingTag = tag;
            tagMoved = false;
            tagGrabOffset = e.position.x - map().x (state().project.sections[(size_t) tag].startSample);
            session.selectSection (tag);
            return;
        }

        session.selectSection (-1);
        draggingRuler = true;
        session.seek (map().sampleAt ((float) juce::jmax (plotArea.getX(), e.x)));
    }
    else if (plotArea.contains (e.getPosition()))
    {
        // 右クリック：お手本の位置の手直し（お手本がある時。DESIGN 7.1.1）
        menuGesture = e.mods.isPopupMenu();
        if (menuGesture)
        {
            showGuideMenu (map().sampleAt (e.position.x));
            return;
        }
        gesture.down (session, map(), e.position.x);
    }
}

void PitchLane::showGuideMenu (int64 at)
{
    const auto& s = state();
    juce::PopupMenu menu;
    const bool hasGuide = s.engineAttached && ! s.refPitch.empty() && ! s.isRecording;
    const auto now = (s.guideNudgeMs > 0 ? "+" : "") + juce::String (s.guideNudgeMs, 0);
    menu.addSectionHeader (tr ("pitch.menu.nudge", now));
    const double steps[] = { -10.0, -1.0, 1.0, 10.0 };
    const char* keys[] = { "pitch.menu.earlier10", "pitch.menu.earlier1", "pitch.menu.later1", "pitch.menu.later10" };
    for (int i = 0; i < 4; ++i)
        menu.addItem (i + 1, tr (keys[i]), hasGuide);
    menu.addItem (5, tr ("pitch.menu.nudgeReset"), hasGuide && s.guideNudgeMs != 0.0);
    // 「ここが同じ所」：押した所の前後で原曲とカラオケを比べて合わせ直す（合わせた原曲がある時）
    menu.addSeparator();
    menu.addItem (6, tr ("pitch.menu.alignHere"), hasGuide && s.guideOriginal != nullptr && ! s.guideBusy);
    if (! hasGuide)
        menu.addItem (-1, tr ("pitch.menu.noGuide"), false);

    juce::Component::SafePointer<PitchLane> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition().withStandardItemHeight (28),
                        [safe, steps, at] (int chosen)
                        {
                            if (safe == nullptr) return;
                            if (chosen >= 1 && chosen <= 4) safe->session.nudgeGuide (steps[chosen - 1]);
                            if (chosen == 5)                safe->session.resetGuideNudge();
                            if (chosen == 6)                safe->session.alignGuideAt (at);
                        });
}

void PitchLane::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingTag >= 0)
    {
        if (! tagMoved && e.getDistanceFromDragStartX() * e.getDistanceFromDragStartX() < 9)
            return;   // 3px 未満はクリック扱い
        tagMoved = true;
        const auto x = juce::jlimit ((float) plotArea.getX(), (float) plotArea.getRight(), e.position.x - tagGrabOffset);
        draggingTag = session.moveSection (draggingTag, map().sampleAt (x), ! e.mods.isAltDown());
        return;
    }

    if (menuGesture)
        return;
    if (draggingRuler)
        session.seek (map().sampleAt ((float) juce::jlimit (plotArea.getX(), plotArea.getRight(), e.x)));
    else if (plotArea.contains (e.getMouseDownPosition()))
        gesture.drag (session, map(), e.position.x, ! e.mods.isAltDown());
}

void PitchLane::mouseUp (const juce::MouseEvent& e)
{
    if (draggingTag >= 0)
    {
        if (! tagMoved)
            session.goToSection (draggingTag);   // 札をクリック：その区間の頭へ（「サビへ」）
        draggingTag = -1;
        return;
    }

    if (std::exchange (menuGesture, false))
        return;
    if (! draggingRuler && plotArea.contains (e.getMouseDownPosition()))
        gesture.up (session, map(), e.position.x);
    draggingRuler = false;
}

void PitchLane::mouseDoubleClick (const juce::MouseEvent& e)
{
    // 札をダブルクリック：名前を選ぶ（一覧 / 自由入力）
    const auto tag = tagAt (e.position);
    if (tag >= 0)
        marks::showNameMenu (session, actions, tag, { e.getScreenX(), e.getScreenY(), 1, 1 });
}

//==============================================================================
void PitchLane::paint (juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    // 動かない部分は画像から（実際の画素の細かさで作る）
    const auto scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const StaticKey key { getWidth(), getHeight(), skinSerial(), scale, textBoostAmount() };
    if (staticDirty || ! staticLayer.isValid() || key.w != staticKey.w || key.h != staticKey.h || key.skin != staticKey.skin
        || ! juce::exactlyEqual (key.scale, staticKey.scale) || ! juce::exactlyEqual (key.boost, staticKey.boost))
    {
        staticLayer = juce::Image (juce::Image::ARGB, juce::roundToInt ((float) getWidth() * scale), juce::roundToInt ((float) getHeight() * scale), true);
        juce::Graphics ig (staticLayer);
        ig.addTransform (juce::AffineTransform::scale (scale));
        paintStatic (ig);
        staticDirty = false;
        staticKey = key;
    }
    g.drawImageTransformed (staticLayer, juce::AffineTransform::scale (1.0f / scale));

    // 再生ヘッドで変わる物：自分の線（歌ったところまで）・再生ヘッド・いまの音・鍵盤の点灯
    const auto& s = state();
    const auto plot = plotArea.toFloat();
    const auto m = map();
    lane::drawRulerPlayhead (g, s, m, rulerArea.withTrimmedLeft (metrics::gutter).toFloat());
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plotArea);
        if (! drawCompareTake (g, m))   // テイク比較の間は、選んだテイクの線をお手本に重ねる（いま歌った線の代わりに）
            drawMine (g, m);
        lane::drawPlayhead (g, s, m, plot);
        drawCurrent (g, m);
    }
    drawNoteGutter (g);
}

void PitchLane::paintStatic (juce::Graphics& g)
{
    const auto& s = state();
    const auto plot = plotArea.toFloat();
    const auto m = map();

    g.setColour (colours::panel);
    g.fillRect (rulerArea.withWidth (metrics::gutter));
    paint::hline (g, (float) rulerArea.getBottom() - 1.0f, 0.0f, (float) metrics::gutter);
    lane::drawRuler (g, s, m, rulerArea.withTrimmedLeft (metrics::gutter).toFloat(), false);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plotArea);

        drawBackground (g, m);
        lane::drawRange (g, s, m, plot);
        if (harmonyGuide())
            drawMainGhost (g, m);
        drawReference (g, m);
        drawUncovered (g, m);

        // お手本ピッチがまだない：解析中か、声入りの原曲をここにドロップする案内（B9）
        if (s.refPitch.empty())
        {
            g.setColour (colours::textMute);
            g.setFont (sans (13.0f));
            const auto text = s.guideBusy ? tr ("pitch.analysingGuide")
                            : s.backingWave != nullptr ? tr ("pitch.dropGuide")
                                                       : tr ("pitch.notAnalyzed");
            g.drawText (text, plot.reduced (24.0f), juce::Justification::centred, false);
        }
    }

    drawFooter (g);
}

void PitchLane::drawBackground (juce::Graphics& g, const TimeMap& m)
{
    // ピアノロール（2026-10-03）：半音ごとの行。黒鍵の行は暗く、白鍵の行は少し明るく、行の境に細い線、オクターブ（B と C の間）は濃い線
    const auto& s = state();
    const auto plot = plotArea.toFloat();
    g.setColour (colours::bgDeep);
    g.fillRect (plot);

    for (int n = s.lowMidi; n <= s.highMidi; ++n)
    {
        const auto y0 = yForMidi ((float) n + 0.5f), y1 = yForMidi ((float) n - 0.5f);
        const juce::Rectangle<float> row (plot.getX(), y0, plot.getWidth(), y1 - y0);
        g.setColour (isBlackKey (n) ? colours::bgDeep.darker (0.35f) : colours::highlight (0.035f));
        g.fillRect (row);
        const auto pc = ((n % 12) + 12) % 12;
        if (pc == 0)
            paint::hline (g, std::round (y1), plot.getX(), plot.getRight(), colours::lineHi.withAlpha (0.9f));   // オクターブの境
        else
            paint::hline (g, std::round (y1), plot.getX(), plot.getRight(), colours::line.withAlpha (pc == 5 ? 0.7f : 0.35f));   // E と F の間は少し濃く
    }

    lane::drawTimeGrid (g, s, m, plot);
}

std::pair<int, int> PitchLane::litKeys() const
{
    // いま歌っている音と、いまのお手本の音（鍵盤を光らせる）
    const auto& s = state();
    int current = -1;
    if (auto* p = dummy::myPitchAt (s, s.playhead))
        current = (int) std::lround (p->midi + mineOffset());
    int guideNow = -1;
    {
        const auto& notes = refNotes();
        auto it = std::upper_bound (notes.begin(), notes.end(), s.playhead, [] (int64 v, const analysis::NoteSpan& n) { return v < n.start; });
        if (it != notes.begin() && s.playhead <= std::prev (it)->end)
            guideNow = (int) std::lround (std::prev (it)->midi + refOffset());
    }
    return { current, guideNow };
}

void PitchLane::drawNoteGutter (juce::Graphics& g)
{
    // 鍵盤：白鍵は隣の黒鍵の真ん中まで（C・F は下の境から、E・B は上の境まで）、黒鍵は行の高さで左から 6 割。
    // いま歌っている音の鍵はライム、いまのお手本の音の鍵はアイスブルーに光る。C には音名
    const auto& s = state();
    const auto r = gutterArea.toFloat();
    g.setColour (colours::panel);
    g.fillRect (r);

    const auto [current, guideNow] = litKeys();

    // 鍵盤の色は実物に寄せる（白と黒）。暗いスキンでは白鍵を少し落としてまぶしくしない
    const bool darkSkin = colours::bgDeep.getPerceivedBrightness() < 0.5f;
    const auto whiteKey = juce::Colour (0xffeeeae2).interpolatedWith (colours::panel, darkSkin ? 0.22f : 0.0f);
    const auto blackKey = juce::Colour (0xff1d1c1a);
    const auto keyEdge  = juce::Colour (0xff8f8a80);
    const auto keys = r.withTrimmedRight (1.0f);
    auto lit = [&] (int n, juce::Colour base)
    {
        if (n == current)  return base.interpolatedWith (colours::signal, 0.85f);
        if (n == guideNow) return base.interpolatedWith (colours::ref, 0.75f);
        return base;
    };

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (gutterArea);
    for (int n = s.lowMidi - 1; n <= s.highMidi + 1; ++n)
    {
        if (isBlackKey (n))
            continue;
        const auto top = yForMidi ((float) n + (isBlackKey (n + 1) ? 1.0f : 0.5f));
        const auto bottom = yForMidi ((float) n - (isBlackKey (n - 1) ? 1.0f : 0.5f));
        const juce::Rectangle<float> key (keys.getX(), top, keys.getWidth(), bottom - top);
        g.setColour (lit (n, whiteKey));
        g.fillRect (key);
        g.setColour (keyEdge);
        g.fillRect (key.withHeight (1.0f).withY (std::round (bottom) - 1.0f));
    }
    for (int n = s.lowMidi - 1; n <= s.highMidi + 1; ++n)
    {
        if (! isBlackKey (n))
            continue;
        const auto top = yForMidi ((float) n + 0.5f), bottom = yForMidi ((float) n - 0.5f);
        const juce::Rectangle<float> key (keys.getX(), top, keys.getWidth() * 0.6f, bottom - top);
        g.setColour (lit (n, blackKey));
        g.fillRoundedRectangle (key.withTrimmedLeft (-3.0f), 2.0f);
    }

    // 音名：C（オクターブの印）と、光っている鍵
    const auto rowH = std::abs (yForMidi (61.0f) - yForMidi (60.0f));
    g.setFont (mono (juce::jlimit (8.5f, 10.5f, rowH * 0.8f), Weight::semibold));
    for (int n = s.lowMidi; n <= s.highMidi; ++n)
    {
        const bool lighted = n == current || n == guideNow;
        if (n % 12 != 0 && ! lighted)
            continue;
        const auto yc = yForMidi ((float) n);
        const auto area = juce::Rectangle<float> (keys.getX(), yc - rowH * 0.5f, keys.getWidth() - 4.0f, rowH);
        g.setColour (isBlackKey (n) && ! lighted ? juce::Colour (0xffeeeae2) : juce::Colour (0xff1d1c1a));
        if (isBlackKey (n))
            g.drawText (dummy::noteName ((float) n), area.withWidth (keys.getWidth() * 0.6f), juce::Justification::centred, false);
        else
            g.drawText (dummy::noteName ((float) n), area, juce::Justification::centredRight, false);
    }
    paint::vline (g, r.getRight() - 1.0f, r.getY(), r.getBottom());
}

void PitchLane::drawUncovered (juce::Graphics& g, const TimeMap& m)
{
    // カット版：お手本が使えない所（原曲に無い所）を斜線と「お手本なし」で（DESIGN 7.1.1）
    const auto& s = state();
    if (s.guideCovered.empty() || s.refPitch.empty())
        return;
    const auto plot = plotArea.toFloat();
    int64 from = 0;
    auto gap = [&] (int64 a, int64 b)
    {
        if (b <= a) return;
        const auto x0 = juce::jmax (plot.getX(), m.x (a)), x1 = juce::jmin (plot.getRight(), m.x (b));
        if (x1 - x0 < 2.0f) return;
        const juce::Rectangle<float> r (x0, plot.getY(), x1 - x0, plot.getHeight());
        g.setColour (colours::bgDeep.withAlpha (0.45f));
        g.fillRect (r);
        lane::drawHatch (g, r, colours::line.withAlpha (0.35f));
        const auto text = tr ("pitch.noGuideHere");
        const auto f = sans (11.5f);
        if (textWidth (f, text) + 12.0f <= r.getWidth())
        {
            g.setColour (colours::textMute);
            g.setFont (f);
            g.drawText (text, r.withTrimmedTop (8.0f).withHeight (18.0f), juce::Justification::centred, false);
        }
    };
    for (auto& [a, b] : s.guideCovered)
    {
        gap (from, a);
        from = juce::jmax (from, b);
    }
    gap (from, s.project.lengthSamples);
}

void PitchLane::drawMainGhost (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    forEachRun (s.refPitch, s.viewStart - 4800, s.viewEnd + 4800, s.sampleRate(), [&] (const std::vector<const dummy::PitchPoint*>& run)
    {
        juce::Path p;
        for (size_t i = 0; i < run.size(); ++i)
        {
            const juce::Point<float> pt { m.x (run[i]->sample), yForMidi (run[i]->midi + (float) s.keyShift) };
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (colours::text.withAlpha (0.28f));
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    });
}

const std::vector<analysis::NoteSpan>& PitchLane::refNotes() const
{
    const auto& s = state();
    const auto& ref = s.activeRef();
    // メインとハモリは同じ時間の並び（数・頭・終わりが同じ）なので、どちらかも鍵に入れる
    const auto key = ref.empty() ? 0
                   : (juce::int64) ref.size() * 1000003 + ref.front().sample * 31 + ref.back().sample
                     + (juce::int64) s.sampleRate() + (&ref == &s.refPitchHarm ? 7919 : 0);
    if (key != notesKey)
    {
        std::vector<audio::PitchFrame> frames;
        frames.reserve (s.activeRef().size());
        for (auto& p : s.activeRef())
            frames.push_back ({ p.sample, p.midi, p.confidence, 0.0f });
        notesCache = analysis::segmentNotes (frames, s.sampleRate());
        // ピアノロールとして読みやすく（2026-10-03）：音符の間が 0.3 秒より短ければ、次の音符の頭まで伸ばす
        // （音の移り・しゃくりで切れて細切れに見えない。判定は線そのもので行うので、ここは見た目だけ）
        const auto legato = (int64) (0.3 * s.sampleRate());
        for (size_t i = 0; i + 1 < notesCache.size(); ++i)
            if (notesCache[i + 1].start - notesCache[i].end < legato)
                notesCache[i].end = notesCache[i + 1].start;
        notesKey = key;
    }
    return notesCache;
}

void PitchLane::drawReference (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    const auto off = refOffset();
    const auto& notes = refNotes();
    const auto rowH = std::abs (yForMidi (60.0f) - yForMidi (61.0f));

    // 音符の棒：伸ばしている音を、いちばん近い半音の行に（「この音を歌う」がひと目で分かる）。入る時は音名も
    const auto nameFont = mono (9.5f, Weight::semibold);
    for (auto& n : notes)
    {
        if (n.end < s.viewStart || n.start > s.viewEnd)
            continue;
        const auto semi = std::round (n.midi + off);
        const auto x0 = m.x (n.start), x1 = m.x (n.end);
        // ピアノロールの行にぴったり（上下 1 px あける）。歌う音がひと目で分かるよう、塗りを濃く
        const juce::Rectangle<float> bar (x0, yForMidi (semi + 0.5f) + 1.0f, juce::jmax (2.0f, x1 - x0), juce::jmax (2.0f, rowH - 2.0f));
        const auto corner = juce::jmin (3.0f, bar.getHeight() * 0.5f);
        g.setColour (colours::ref.withAlpha (0.42f));
        g.fillRoundedRectangle (bar, corner);
        g.setColour (colours::ref.withAlpha (0.95f));
        g.drawRoundedRectangle (bar.reduced (0.5f), corner, 1.0f);

        const auto name = dummy::noteName (semi);
        if (rowH >= 9.0f && bar.getWidth() >= textWidth (nameFont, name) + 10.0f)
        {
            g.setColour (colours::text);
            g.setFont (nameFont);
            g.drawText (name, bar.withTrimmedLeft (5.0f), juce::Justification::centredLeft, false);
        }
    }

    // 細かい音程の線：音符の中は濃く、音符の外（しゃくり・フォール・つなぎ・取り切れない外れ）は薄く
    forEachRun (s.activeRef(), s.viewStart - 4800, s.viewEnd + 4800, s.sampleRate(), [&] (const std::vector<const dummy::PitchPoint*>& run)
    {
        juce::Path all, inNotes;
        bool drawingIn = false;
        for (size_t i = 0; i < run.size(); ++i)
        {
            const juce::Point<float> pt { m.x (run[i]->sample), yForMidi (run[i]->midi + off) };
            if (i == 0) all.startNewSubPath (pt); else all.lineTo (pt);

            const auto sample = run[i]->sample;
            auto it = std::upper_bound (notes.begin(), notes.end(), sample, [] (int64 v, const analysis::NoteSpan& n) { return v < n.start; });
            const bool in = it != notes.begin() && sample <= std::prev (it)->end;
            if (in && ! drawingIn) inNotes.startNewSubPath (pt);
            else if (in)           inNotes.lineTo (pt);
            drawingIn = in;
        }
        const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
        g.setColour (colours::ref.withAlpha (0.28f));
        g.strokePath (all, stroke (1.0f));
        g.setColour (colours::ref.brighter (0.3f).withAlpha (0.8f));
        g.strokePath (inNotes, stroke (1.2f));
    });
}

void PitchLane::drawMine (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    const auto off = mineOffset();
    const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };

    // 自分の線は「歌ったところ」＝再生ヘッドまで。描き直す範囲の左端より前（16 px の余白より左）は組み立てない
    // （再生中は再生ヘッドの前後だけを描き直すので、線全体をたどると重い。#26）
    const auto from = juce::jmax (s.viewStart - 4800, m.sampleAt ((float) g.getClipBounds().getX() - 16.0f));
    forEachRun (s.myPitch, from, s.playhead, s.sampleRate(), [&] (const std::vector<const dummy::PitchPoint*>& run)
    {
        // 同じ色の連続ごとに描く（境界点は両側で共有して途切れなく見せる）
        juce::Path path;
        auto colour = colourFor (*run.front());

        auto flush = [&]
        {
            g.setColour (colour.withAlpha (0.16f));
            g.strokePath (path, stroke (7.0f));
            g.setColour (colour);
            g.strokePath (path, stroke (2.6f));
            path.clear();
        };

        for (size_t i = 0; i < run.size(); ++i)
        {
            const juce::Point<float> pt { m.x (run[i]->sample), yForMidi (run[i]->midi + off) };
            const auto c = colourFor (*run[i]);

            if (i == 0)       { path.startNewSubPath (pt); continue; }
            path.lineTo (pt);
            if (c != colour)  { flush(); colour = c; path.startNewSubPath (pt); }
        }
        flush();
    });
}

bool PitchLane::drawCompareTake (juce::Graphics& g, const TimeMap& m)
{
    // テイク比較（B18c）：試聴中のテイクの音程（録った時に裏で取った線。原速・原キーのテイクだけ）を、比べている範囲だけ重ねる。
    // 色は暖かい白（お手本のアイスブルーと見分ける）。いま歌っている線とは別物なので、比べている間は自分の線を描かない
    const auto& s = state();
    const auto& c = s.compare;
    if (! c.active || c.previewing.isEmpty() || c.track != s.currentTrack().type)
        return false;
    const auto it = s.takePitch.find (dummy::takeWaveKey (c.track, c.previewing));
    if (it == s.takePitch.end() || it->second == nullptr)
        return false;

    // お手本と同じだけずらす（練習のキー・ハモリ）。テイクは原キーで録っているので、ずらしたお手本と同じ所に並ぶ
    const auto off = mineOffset() + (float) s.keyShift;
    const auto from = juce::jmax (c.from, s.viewStart - 4800), to = juce::jmin (c.to, s.viewEnd + 4800);
    const auto maxGap = (int64) (0.05 * s.sampleRate());
    juce::Path path;
    bool open = false;
    int64 last = 0;
    float lastMidi = 0.0f;
    for (auto& f : *it->second)
    {
        if (f.songSample < from || f.songSample > to)
            continue;
        if (f.midi <= 0.0f || f.confidence < 0.5f)
        {
            open = false;   // 声の無い所・信頼の低い所はつながない（嘘でつながない）
            continue;
        }
        const juce::Point<float> pt { m.x (f.songSample), yForMidi (f.midi + off) };
        if (! open || f.songSample - last > maxGap || std::abs (f.midi - lastMidi) > 4.0f)
            path.startNewSubPath (pt);
        else
            path.lineTo (pt);
        open = true;
        last = f.songSample;
        lastMidi = f.midi;
    }
    const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
    g.setColour (colours::text.withAlpha (0.14f));
    g.strokePath (path, stroke (6.0f));
    g.setColour (colours::text.withAlpha (0.9f));
    g.strokePath (path, stroke (2.2f));
    return true;
}

void PitchLane::drawCurrent (juce::Graphics& g, const TimeMap& m)
{
    const auto& s = state();
    const auto* p = dummy::myPitchAt (s, s.playhead);
    if (p == nullptr)
        return;   // 無音・子音では出さない

    const juce::Point<float> c { m.x (s.playhead), yForMidi (p->midi + mineOffset()) };
    const auto col = colourFor (*p);

    g.setColour (col.withAlpha (0.16f));
    g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (c));
    g.setColour (colours::bgDeep);
    g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c));
    g.setColour (col);
    g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c), 2.5f);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));

    // セント値はプロのみ（DESIGN 4.3）。お手本が無ければ出さない
    if (s.mode == project::Mode::pro && p->judged)
    {
        const auto cents = juce::roundToInt (p->centsOff);
        g.setFont (mono (11.0f, Weight::semibold));
        g.setColour (col);
        g.drawText (tr ("pitch.cents", (cents >= 0 ? "+" : "") + juce::String (cents)),
                    juce::Rectangle<float> (c.x + 14.0f, c.y - 22.0f, 90.0f, 16.0f), juce::Justification::centredLeft, false);
    }
}

void PitchLane::drawFooter (juce::Graphics& g)
{
    const auto& s = state();
    g.setColour (colours::panel);
    g.fillRect (footerArea);
    paint::hline (g, (float) footerArea.getY(), 0.0f, (float) getWidth());

    paint::microLabel (g, footerArea.withWidth (metrics::gutter).toFloat().withTrimmedLeft ((float) metrics::pad),
                       tr ("label.pitch"), colours::textMute);

    auto r = legendArea.toFloat();
    const auto lf = sans (11.0f);

    auto label = [&] (const juce::String& text, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (lf);
        const auto w = juce::jmin (r.getWidth(), textWidth (lf, text) + 2.0f);
        g.drawText (text, r.removeFromLeft (w), juce::Justification::centredLeft, true);
        r.removeFromLeft (16.0f);
    };

    // お手本：音符の棒と線
    {
        auto sw = r.removeFromLeft (26.0f).withSizeKeepingCentre (26.0f, 8.0f);
        g.setColour (colours::ref.withAlpha (0.18f));
        g.fillRoundedRectangle (sw, 3.0f);
        g.setColour (colours::ref.withAlpha (0.5f));
        g.drawRoundedRectangle (sw.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (colours::ref.withAlpha (0.95f));
        g.fillRect (sw.withSizeKeepingCentre (sw.getWidth() - 6.0f, 1.4f));
        r.removeFromLeft (6.0f);
        label (harmonyGuide() ? tr ("pitch.legend.refHarmony", (int) s.pitchToleranceCents)
                                     : tr ("pitch.legend.ref", (int) s.pitchToleranceCents), colours::textDim);
        // 時間合わせの確かさが低い：「推定」（右クリックで直せる）
        if (s.guideAlignRough && ! s.refPitch.empty())
            label (tr ("pitch.legend.alignRough"), colours::warn);
    }

    // 自分：3 状態
    {
        const juce::Colour cs[] = { colours::signal, colours::warn, colours::bad };
        for (auto c : cs)
        {
            g.setColour (c);
            g.fillRoundedRectangle (r.removeFromLeft (12.0f).withSizeKeepingCentre (12.0f, 3.0f), 1.5f);
            r.removeFromLeft (2.0f);
        }
        r.removeFromLeft (6.0f);
        label (tr ("pitch.legend.mine"), colours::textDim);
    }

    if (harmonyGuide() && r.getWidth() > 60.0f)
    {
        g.setColour (colours::text.withAlpha (0.35f));
        g.fillRoundedRectangle (r.removeFromLeft (18.0f).withSizeKeepingCentre (18.0f, 2.0f), 1.0f);
        r.removeFromLeft (6.0f);
        label (tr ("pitch.legend.mainGhost"), colours::textDim);
    }

    // 横の拡大の案内（入り切る時だけ。切れた文字は出さない）
    {
       #if JUCE_MAC
        const auto zoom = tr ("pitch.legend.zoom", juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\x98")));
       #else
        const auto zoom = tr ("pitch.legend.zoom", "Ctrl");
       #endif
        if (textWidth (lf, zoom) + 2.0f <= r.getWidth())
            label (zoom, colours::textMute);
    }

    // 入りタイミング（標準以上）/ 解析（プロ）。本物のアプリはいまのトラックのいちばん新しいテイクをお手本と比べた値（B18）。
    // 見本（UI_MOCK）は見本の値。お手本・テイクが無ければ出さない
    if (! analysisArea.isEmpty())
    {
        auto a = analysisArea.toFloat();
        auto chip = [&] (const juce::String& name, const juce::String& value, juce::Colour c)
        {
            const auto lf2 = mono (9.5f, Weight::medium, 0.12f);
            const auto vf = mono (11.0f, Weight::semibold);
            const auto w = textWidth (lf2, name) + textWidth (vf, value) + 22.0f;
            if (w > a.getWidth())
                return;
            auto box = a.removeFromLeft (w).withSizeKeepingCentre (w, 24.0f);
            a.removeFromLeft (6.0f);
            paint::inset (g, box);
            box.reduce (8.0f, 0.0f);
            paint::microLabel (g, box.removeFromLeft (textWidth (lf2, name) + 6.0f), name, colours::textMute);
            g.setColour (c);
            g.setFont (vf);
            g.drawText (value, box, juce::Justification::centredLeft, false);
        };

        if (! s.engineAttached)
        {
            if (! s.myPitch.empty() && s.myPitch.front().judged)
            {
                chip (tr ("analysis.onset"), tr ("analysis.onset.value", "+40"), colours::warn);
                if (s.mode == project::Mode::pro)
                    chip (tr ("analysis.vibrato"), tr ("analysis.vibrato.value", "5.5", "28"), colours::text);
            }
        }
        else if (const auto* st = session.latestTakeStats())
        {
            if (st->matched > 0)
            {
                const auto ms = juce::roundToInt (st->onsetMs);
                const auto absMs = std::abs (ms);
                const auto value = absMs <= 15 ? tr ("analysis.onset.onTime")
                                 : ms > 0     ? tr ("analysis.onset.value", "+" + juce::String (ms))
                                              : tr ("analysis.onset.early", juce::String (absMs));
                chip (tr ("analysis.onset"), value, absMs <= 30 ? colours::signal : (absMs <= 80 ? colours::warn : colours::bad));
            }
            if (s.mode == project::Mode::pro)
            {
                if (st->pitchFrames > 50)
                {
                    const auto pct = juce::roundToInt (st->inBand * 100.0f);
                    chip (tr ("analysis.pitch"), tr ("analysis.pitch.value", pct), pct >= 80 ? colours::signal : (pct >= 60 ? colours::warn : colours::bad));
                }
                if (st->vibNotes > 0)
                    chip (tr ("analysis.vibrato"), tr ("analysis.vibrato.value", juce::String (st->vibRateHz, 1), juce::roundToInt (st->vibDepthCents)), colours::text);
            }
        }
    }
}
} // namespace vb
