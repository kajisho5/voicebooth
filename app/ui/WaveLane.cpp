#include "WaveLane.h"
#include "project/TakeCompare.h"

namespace vb
{
namespace
{
    constexpr float topPad = 6.0f;
    constexpr float compH = 18.0f;
    constexpr float bigH = 56.0f;
    constexpr float easyH = 40.0f;
    constexpr float thinH = 14.0f;
    constexpr float gap = 4.0f;
}

juce::String trackName (project::TrackType t)
{
    using project::TrackType;
    switch (t)
    {
        case TrackType::backing:     return tr ("track.backing");
        case TrackType::guide:       return tr ("track.guide");
        case TrackType::main:        return tr ("track.main");
        case TrackType::doubleTrack: return tr ("track.double");
        case TrackType::harm1:       return tr ("track.harm1");
        case TrackType::harm2:       return tr ("track.harm2");
    }
    return {};
}

int WaveLane::preferredHeight (project::Mode m)
{
    switch (m)
    {
        case project::Mode::easy:     return 86;    // 最小（Main とオフボ）
        case project::Mode::standard: return 142;
        case project::Mode::pro:      return 160;   // Harm 2 の行が増える
    }
    return 142;
}

WaveLane::WaveLane (UiSession& u, Actions& a) : SessionView (u), actions (a)
{
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

juce::Rectangle<float> WaveLane::compBarArea() const
{
    const auto pl = plot();
    return pl.withTop (pl.getY() + topPad).withHeight (compH);
}

bool WaveLane::overCompBar (juce::Point<float> p) const
{
    // 簡単モードには採用区間のバーもテイク比較も無い（DESIGN 2）
    return state().mode != project::Mode::easy && compBarArea().contains (p);
}

void WaveLane::mouseMove (const juce::MouseEvent& e)
{
    // 選び直せる所だけ指のカーソル（波形の上は今までどおり範囲を選ぶ）
    const bool pick = overCompBar (e.position) && session.canCompareTakes();
    const bool edge = ! pick && e.x >= metrics::gutter && lane::rangeEdgeAt (state(), map(), e.position.x) >= 0;
    setMouseCursor (pick ? juce::MouseCursor::PointingHandCursor
                         : (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::IBeamCursor));
}

void WaveLane::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { lane::wheel (session, map(), e, w); }
void WaveLane::mouseMagnify (const juce::MouseEvent& e, float scale)                       { lane::magnify (session, map(), e, scale); }

juce::String WaveLane::getTooltip()
{
    const auto p = getMouseXYRelative().toFloat();
    return overCompBar (p) && session.canCompareTakes() ? tr ("wave.compBar.tooltip") : juce::String();
}

void WaveLane::onSessionChanged (juce::uint32 c)
{
    constexpr juce::uint32 watched = change::playhead | change::range | change::view | change::tracks | change::mode
                                   | change::transport | change::takes | change::songInfo;
    const auto relevant = c & watched;
    if (relevant == 0)
        return;
    if ((relevant & ~juce::uint32 (change::playhead)) != 0)
        staticDirty = true;
    const auto x = map().x (state().playhead);
    const auto dirty = relevant == change::playhead ? lane::playheadDirty (headX, x, getHeight()) : juce::Rectangle<int>();
    headX = x;
    if (dirty.isEmpty())
        repaint();
    else
        repaint (dirty);   // 再生ヘッドだけ動いた：前と今の位置の間だけ（録音中に伸びる波形もこの間）
}

TimeMap WaveLane::map() const
{
    return lane::makeMap (state(), plot());
}

std::vector<WaveLane::Row> WaveLane::layoutRows() const
{
    const auto& s = state();
    const bool easy = s.mode == project::Mode::easy;
    std::vector<Row> rows;
    auto r = plot();
    r.removeFromTop (topPad + (easy ? 0.0f : compH + 2.0f));

    const auto& cur = s.currentTrack();
    rows.push_back ({ cur.type, s.selectedTrack, r.removeFromTop (easy ? easyH : bigH), true });
    r.removeFromTop (gap + 2.0f);

    for (size_t i = 0; i < s.trackUi.size(); ++i)
    {
        const auto& t = s.trackUi[i];
        if ((int) i == s.selectedTrack || ! session.isTrackVisible (t.type))
            continue;

        rows.push_back ({ t.type, (int) i, r.removeFromTop (thinH), false });
        r.removeFromTop (gap);
    }

    rows.push_back ({ project::TrackType::backing, -1, r.removeFromTop (thinH), false });
    return rows;
}

//==============================================================================
void WaveLane::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < metrics::gutter) return;
    menuGesture = e.mods.isPopupMenu();
    if (menuGesture)
    {
        showTakeMenu();
        return;
    }
    compBarPress = overCompBar (e.position) && session.canCompareTakes();
    gesture.down (session, map(), e.position.x);
}

void WaveLane::showTakeMenu()
{
    // 録り間違いの救済：いまのトラックのリハーサルのテイクを本番に入れる（原速・原キーで録った物だけ）
    const auto& s = state();
    const auto type = s.currentTrack().type;
    const auto* track = s.project.findTrack (type);
    juce::PopupMenu menu;
    std::vector<juce::String> ids;
    if (track != nullptr && ! s.isRecording)
        for (auto& k : track->takes)
        {
            if (k.recMode != project::RecMode::practice)
                continue;
            if (k.tempoPercent == 100 && k.keyShift == 0)
            {
                const auto from = k.useFrom >= 0 ? k.useFrom : juce::jmax ((project::int64) 0, k.startSample);
                const auto to = k.useTo > from ? k.useTo : k.endSample;
                ids.push_back (k.id);
                menu.addItem ((int) ids.size(), tr ("rescue.menu", k.id, formatTime (from, s.sampleRate(), true) + " - "
                                                                          + formatTime (juce::jmin (s.project.lengthSamples, to), s.sampleRate(), true)));
            }
            else
                menu.addItem (-1, tr ("rescue.menuNotOriginal", k.id, k.tempoPercent, (k.keyShift > 0 ? "+" : "") + juce::String (k.keyShift)), false);
        }
    if (ids.empty() && menu.getNumItems() == 0)
        menu.addItem (-1, tr ("rescue.menuNone"), false);

    juce::Component::SafePointer<WaveLane> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition().withStandardItemHeight (28),
                        [safe, type, ids] (int chosen)
                        {
                            if (safe != nullptr && chosen > 0 && chosen <= (int) ids.size())
                                safe->session.promoteRehearsalTake (type, ids[(size_t) chosen - 1]);
                        });
}

void WaveLane::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getMouseDownX() < metrics::gutter || menuGesture) return;
    gesture.drag (session, map(), e.position.x, ! e.mods.isAltDown());
}

void WaveLane::mouseUp (const juce::MouseEvent& e)
{
    // ガターの細い行（トラック名）をクリック → そのトラックを選択
    if (e.getMouseDownX() < metrics::gutter)
    {
        for (auto& row : layoutRows())
            if (! row.current && row.trackIndex >= 0
                && juce::isPositiveAndBelow (e.position.y - (row.area.getY() - gap * 0.5f), row.area.getHeight() + gap))
                session.selectTrack (row.trackIndex);
        return;
    }
    if (menuGesture) { menuGesture = false; return; }

    // 採用区間のバーをクリック（ドラッグしていない）：その区間（区間の間なら、その空き）のテイクを選び直す
    if (std::exchange (compBarPress, false) && e.getDistanceFromDragStart() < 4 && actions.openTakeCompare)
        if (const auto* track = state().project.findTrack (state().currentTrack().type))
        {
            const auto span = project::compSpanAt (*track, map().sampleAt (e.position.x), state().project.lengthSamples);
            if (span.second > span.first)
            {
                gesture.up (session, map(), (float) e.getMouseDownX());   // 押した所へ移動（クリックと同じ）してから開く
                actions.openTakeCompare (span.first, span.second);
                return;
            }
        }
    gesture.up (session, map(), e.position.x);
}

//==============================================================================
void WaveLane::paint (juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    // 動かない部分は画像から（実際の画素の細かさで作る。125 % 表示などでぼやけないように）
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

    // 毎フレーム変わる物：録音中の帯と再生ヘッド
    const auto& s = state();
    const auto pl = plot();
    const auto m = map();
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (pl.getSmallestIntegerContainer());
    drawRecording (g, m, layoutRows().front());
    lane::drawPlayhead (g, s, m, pl);
}

void WaveLane::paintStatic (juce::Graphics& g)
{
    const auto& s = state();
    const auto bounds = getLocalBounds().toFloat();
    const auto pl = plot();
    const auto m = map();
    const auto rows = layoutRows();

    g.setColour (colours::bgDeep);
    g.fillRect (pl);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (pl.getSmallestIntegerContainer());

        lane::drawTimeGrid (g, s, m, pl);
        lane::drawRange (g, s, m, pl);
        if (s.mode != project::Mode::easy)
            drawCompBar (g, m, pl.withTop (pl.getY() + topPad).withHeight (compH), rows.front().type);

        for (auto& row : rows)
            drawWave (g, m, row);
    }

    // ガター（トラック名）
    const auto gut = bounds.withWidth ((float) metrics::gutter);
    g.setColour (colours::panel);
    g.fillRect (gut);
    paint::vline (g, gut.getRight() - 1.0f, gut.getY(), gut.getBottom());
    paint::hline (g, bounds.getBottom() - 1.0f, 0.0f, bounds.getRight());

    if (s.mode != project::Mode::easy)
        paint::microLabel (g, juce::Rectangle<float> (gut.getX() + (float) metrics::pad, pl.getY() + topPad, gut.getWidth(), compH),
                           tr ("label.take"), colours::textMute);

    for (auto& row : rows)
    {
        auto label = juce::Rectangle<float> (gut.getX() + (float) metrics::pad, row.area.getY(),
                                             gut.getWidth() - (float) metrics::pad - 6.0f, row.area.getHeight());
        const auto name = trackName (row.type);

        if (row.current)
        {
            g.setColour (colours::text);
            g.setFont (sans (13.0f, Weight::semibold));
            g.drawFittedText (name, label.removeFromTop (row.area.getHeight() * 0.5f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.8f);

            if (row.trackIndex >= 0 && s.trackUi[(size_t) row.trackIndex].armed)
            {
                paint::led (g, { label.getX() + 3.0f, label.getY() + 9.0f }, 2.6f, colours::rec, true);
                paint::microLabel (g, label.withTrimmedLeft (10.0f).withHeight (18.0f), tr ("label.arm"), colours::rec);
            }
        }
        else
        {
            g.setColour (colours::textDim);
            g.setFont (sans (10.5f));
            g.drawFittedText (name, label.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        }
    }
}

void WaveLane::drawCompBar (juce::Graphics& g, const TimeMap& m, juce::Rectangle<float> bar, project::TrackType type)
{
    const auto* track = state().project.findTrack (type);
    if (track == nullptr)
        return;

    if (track->comp.empty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (10.5f));
        g.drawText (tr ("wave.noTake"), bar.withTrimmedLeft (8.0f), juce::Justification::centredLeft, false);
        drawCompareRange (g, m, bar, type);
        return;
    }

    for (auto& c : track->comp)
    {
        const auto x0 = m.x (c.startSample), x1 = m.x (c.endSample);
        if (x1 < bar.getX() || x0 > bar.getRight())
            continue;

        bool clipped = false;
        for (auto& t : track->takes)
            if (t.id == c.takeId)
                clipped = t.clip;

        const auto seg = juce::Rectangle<float> (x0, bar.getY(), x1 - x0, bar.getHeight()).reduced (1.0f, 0.0f);
        g.setColour (colours::raised);
        g.fillRoundedRectangle (seg, 2.0f);
        g.setColour (colours::signal);
        g.fillRect (seg.withWidth (2.0f));

        auto label = seg.withLeft (juce::jmax (seg.getX(), bar.getX()) + 8.0f);
        const auto lf = mono (10.5f, Weight::medium);
        const auto name = c.takeId.toUpperCase();
        g.setColour (colours::text.withAlpha (0.9f));
        g.setFont (lf);
        g.drawText (name, label, juce::Justification::centredLeft, false);

        if (clipped)
        {
            label.removeFromLeft (textWidth (lf, name) + 10.0f);
            const auto tf = mono (9.5f, Weight::semibold, 0.08f);
            const auto tw = textWidth (tf, tr ("wave.clip")) + 14.0f;
            const auto tag = label.removeFromLeft (tw).reduced (0.0f, 3.0f);
            g.setColour (colours::bad);
            g.fillRoundedRectangle (tag, 2.0f);
            g.setColour (colours::onFill (colours::bad));
            g.setFont (tf);
            g.drawText (tr ("wave.clip"), tag, juce::Justification::centred, false);
        }
    }

    // つなぎ目（クロスフェード）
    for (size_t i = 1; i < track->comp.size(); ++i)
    {
        const auto x = m.x (track->comp[i].startSample);
        if (x < bar.getX() || x > bar.getRight())
            continue;

        const auto cy = bar.getCentreY();
        juce::Path d;
        d.addQuadrilateral (x, cy - 5.0f, x + 5.0f, cy, x, cy + 5.0f, x - 5.0f, cy);
        g.setColour (colours::text);
        g.fillPath (d);
    }

    drawCompareRange (g, m, bar, type);
}

void WaveLane::drawCompareRange (juce::Graphics& g, const TimeMap& m, juce::Rectangle<float> bar, project::TrackType type)
{
    // テイク比較の間：比べている範囲を枠で囲み、試聴中のテイクを出す（バーの区間はもう差し替わった形）
    const auto& c = state().compare;
    if (! c.active || c.track != type)
        return;
    const auto x0 = juce::jmax (bar.getX(), m.x (c.from)), x1 = juce::jmin (bar.getRight(), m.x (c.to));
    if (x1 <= x0)
        return;
    const auto r = juce::Rectangle<float> (x0, bar.getY(), x1 - x0, bar.getHeight()).reduced (0.5f);
    g.setColour (colours::signal.withAlpha (0.10f));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (colours::signal);
    g.drawRoundedRectangle (r, 2.0f, 1.5f);

    const auto text = c.previewing.isEmpty() ? tr ("compare.current") : tr ("compare.auditioning", c.previewing.toUpperCase());
    const auto f = sans (10.5f, Weight::semibold);   // 日本語が入る（Mono には無い）
    const auto w = textWidth (f, text) + 14.0f;
    if (w + 8.0f > r.getWidth())
        return;
    const auto tag = juce::Rectangle<float> (r.getRight() - w - 4.0f, r.getY() + 3.0f, w, r.getHeight() - 6.0f);
    g.setColour (colours::signal);
    g.fillRoundedRectangle (tag, 2.0f);
    g.setColour (colours::onFill (colours::signal));
    g.setFont (f);
    g.drawText (text, tag, juce::Justification::centred, false);
}

void WaveLane::drawWave (juce::Graphics& g, const TimeMap& m, const Row& row)
{
    const auto& s = state();
    const auto a = row.area;
    const auto cy = a.getCentreY();
    const bool backing = row.type == project::TrackType::backing;

    // 未録音は斜線
    float runStart = -1.0f;
    for (float px = a.getX(); px <= a.getRight(); px += 1.0f)
    {
        const bool recorded = px < a.getRight() && dummy::isRecorded (s, row.type, m.sampleAt (px));
        if (! recorded && runStart < 0.0f) runStart = px;
        if ((recorded || px >= a.getRight()) && runStart >= 0.0f)
        {
            const auto hatch = juce::Rectangle<float> (runStart, a.getY(), px - runStart, a.getHeight());
            lane::drawHatch (g, hatch, colours::line.withAlpha (0.9f));
            if (hatch.getWidth() > 120.0f)
            {
                g.setColour (colours::textMute);
                g.setFont (sans (row.current ? 12.0f : 10.0f));
                g.drawText (tr ("wave.unrecorded"), hatch.reduced (8.0f, 0.0f), juce::Justification::centredLeft, false);
            }
            runStart = -1.0f;
        }
    }

    paint::hline (g, std::round (cy), a.getX(), a.getRight(), colours::line.withAlpha (0.5f));

    const auto base = backing ? colours::textMute.withAlpha (0.55f)
                              : (row.current ? colours::text.withAlpha (0.62f) : colours::textDim.withAlpha (0.45f));

    float clipX = -1.0f;
    for (float px = a.getX(); px < a.getRight(); px += 1.0f)
    {
        const auto s0 = m.sampleAt (px), s1 = m.sampleAt (px + 1.0f);
        float amp = 0.0f;
        if (backing)
        {
            // ピーク（薄）＋ RMS（濃）。細い行なので dB スケール（線形だと小さい音が潰れる）
            const auto peak = audio::WaveformOverview::toDbScale (dummy::backingPeak (s, s0, s1));
            const auto rms = juce::jmin (peak, audio::WaveformOverview::toDbScale (dummy::backingRms (s, s0, s1)));
            if (peak <= 0.0f)
                continue;
            const auto hp = juce::jmax (1.0f, peak * (a.getHeight() - 2.0f));
            const auto hr = juce::jmax (1.0f, rms * (a.getHeight() - 2.0f));
            g.setColour (colours::textMute.withAlpha (0.3f));
            g.fillRect (juce::Rectangle<float> (px, cy - hp * 0.5f, 1.0f, hp));
            g.setColour (colours::textDim.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (px, cy - hr * 0.5f, 1.0f, hr));
            continue;
        }
        else
        {
            amp = dummy::vocalPeak (s, row.type, s0, s1);
        }
        if (amp <= 0.0f)
            continue;

        // 大きい行（選択中）は線形：フレーズの抑揚・子音の立ち上がりが読める（dB だと壁になる）
        // 細い行は dB：線形だと形が潰れて見えない。クリップの判定は線形の値で
        const bool clip = amp > 1.0f;
        const auto level = row.current ? juce::jmin (1.0f, amp) : audio::WaveformOverview::toDbScale (amp);
        if (level <= 0.0f)
            continue;
        const auto h = juce::jmax (1.0f, level * (a.getHeight() - 2.0f));
        g.setColour (clip ? colours::bad : base);
        g.fillRect (juce::Rectangle<float> (px, cy - h * 0.5f, 1.0f, h));

        if (clip && row.current && clipX < 0.0f)
            clipX = px;
    }

    if (clipX >= 0.0f)
    {
        juce::Path tri;
        tri.addTriangle (clipX - 5.0f, a.getY() - 2.0f, clipX + 5.0f, a.getY() - 2.0f, clipX, a.getY() + 5.0f);
        g.setColour (colours::bad);
        g.fillPath (tri);
    }
}

void WaveLane::drawRecording (juce::Graphics& g, const TimeMap& m, const Row& row)
{
    const auto& s = state();
    if (! s.isRecording || s.playhead <= s.recordStart)
        return;

    // 今回の録音（新しいテイク）を現在トラックの上に重ねて見せる
    const auto x0 = juce::jmax (row.area.getX(), m.x (s.recordStart));
    const auto x1 = juce::jmin (row.area.getRight(), m.x (s.playhead));
    if (x1 <= x0)
        return;

    const auto r = juce::Rectangle<float> (x0, row.area.getY(), x1 - x0, row.area.getHeight());
    g.setColour (colours::rec.withAlpha (0.12f));
    g.fillRect (r);
    g.setColour (colours::rec);
    g.fillRect (r.withHeight (2.0f));
    paint::microLabel (g, r.withHeight (16.0f).translated (6.0f, 3.0f), tr ("wave.newTake"), colours::rec);
}
} // namespace vb
