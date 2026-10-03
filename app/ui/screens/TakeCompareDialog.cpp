#include "TakeCompareDialog.h"
#include "../WaveLane.h"
#include "../Timeline.h"
#include "project/TakeCompare.h"

namespace vb
{
namespace
{
    constexpr int rangeRowH = 40, columnRowH = 20, hintH = 46, maxVisibleRows = 7;

    /** 入りのずれの表示と色（ピッチレーンの ONSET の札と同じ基準：±15 ms は「ぴったり」、30 ms までライム・80 ms までアンバー） */
    std::pair<juce::String, juce::Colour> onsetText (const dummy::Session::TakeStats& st)
    {
        const auto ms = juce::roundToInt (st.onsetMs);
        const auto absMs = std::abs (ms);
        const auto text = absMs <= 15 ? tr ("analysis.onset.onTime")
                        : ms > 0     ? tr ("analysis.onset.value", "+" + juce::String (ms))
                                     : tr ("analysis.onset.early", juce::String (absMs));
        return { text, absMs <= 30 ? colours::signal : (absMs <= 80 ? colours::warn : colours::bad) };
    }

    juce::Colour pitchColour (int pct)
    {
        return pct >= 80 ? colours::signal : (pct >= 60 ? colours::warn : colours::bad);
    }
}

//==============================================================================
TakeCompareDialog::TakeCompareDialog (UiSession& u)
    : DialogPanel (tr ("compare.title", trackName (u->compare.track)), tr ("compare.micro")),
      SessionView (u),
      playKey (tr ("compare.play"))
{
    serial = u->compare.serial;

    playKey.withIcon (Icon::play).withLed (colours::signal).withToggle (false).withShortcut ("Space");
    playKey.setTooltip (tr ("compare.play.tooltip"));
    playKey.onClick = [this] { session.toggleCompareAudition(); };
    addAndMakeVisible (playKey);

    // キーは右から並ぶ：使う（主）・キャンセル
    useKey = &addFooterKey (tr ("compare.use"), KeyRole::primary, [this] { use(); });
    addFooterKey (tr ("common.cancel"), KeyRole::normal, [this] { cancel(); });
    onCloseRequest = [this] { cancel(); };

    view.setViewedComponent (&list, false);
    view.setScrollBarsShown (true, false);
    view.setScrollBarThickness (8);
    addAndMakeVisible (view);

    rebuild();
    const auto rows = juce::jlimit (2, maxVisibleRows, (int) entries.size());
    setSize (pro() ? 680 : 600, headerH + 14 + rangeRowH + 8 + columnRowH + rows * rowH + hintH + footerH + 6);
    refreshKeys();
}

TakeCompareDialog::~TakeCompareDialog()
{
    // 閉じ方によらず（別のダイアログに替わった等）、比べたままにしない：元の採用区間に戻す
    if (! finished && state().compare.active && state().compare.serial == serial)
    {
        finished = true;
        session.endTakeCompare (false);
    }
}

void TakeCompareDialog::use()    { finish (true); }
void TakeCompareDialog::cancel() { finish (false); }

void TakeCompareDialog::finish (bool commit)
{
    if (finished)
        return;
    finished = true;
    if (state().compare.active && state().compare.serial == serial)
        session.endTakeCompare (commit);
    if (onFinished)
        onFinished();
}

//==============================================================================
void TakeCompareDialog::rebuild()
{
    // 並び：「いまの採用」を先頭に、比べられるテイクを新しい順に。数値は比べる範囲の中だけで出す
    const auto& s = state();
    const auto& c = s.compare;
    entries.clear();
    entries.push_back ({});

    if (const auto* track = s.project.findTrack (c.track))
        for (auto* k : project::compareCandidates (*track, c.from, c.to))
        {
            Entry e;
            e.takeId = k->id;
            e.created = k->created;
            e.clip = k->clip;
            e.rehearsal = k->recMode == project::RecMode::practice;
            e.coverage = project::takeCoverage (*k, c.from, c.to);
            e.share = project::compShare (c.original, k->id, c.from, c.to);
            e.stats = session.takeStatsIn (c.track, k->id, c.from, c.to);
            entries.push_back (std::move (e));
        }

    statsSignature = juce::String ((int) s.takePitch.size()) + "/" + juce::String ((int) s.refPitch.size()) + "/"
                   + juce::String (s.pitchToleranceCents) + "/" + juce::String ((int) s.takeStats.size());

    const auto w = view.getMaximumVisibleWidth() > 0 ? view.getMaximumVisibleWidth() : view.getWidth();
    list.setSize (juce::jmax (1, w), (int) entries.size() * rowH);
    list.repaint();
}

int TakeCompareDialog::selectedIndex() const
{
    const auto& id = state().compare.previewing;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].takeId == id)
            return i;
    return 0;
}

void TakeCompareDialog::select (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) entries.size()))
        return;
    session.previewCompareTake (entries[(size_t) index].takeId);   // 鳴っていればそのまま次の音へ（聴き比べ）
    // 選んだ行が見えるように（↑ ↓ で隠れた所へ移った時）
    const auto y = index * rowH, top = view.getViewPositionY(), h = view.getMaximumVisibleHeight();
    if (y < top)               view.setViewPosition (0, y);
    else if (y + rowH > top + h) view.setViewPosition (0, y + rowH - h);
}

void TakeCompareDialog::refreshKeys()
{
    const auto& s = state();
    useKey->setEnabled (s.compare.active && s.compare.previewing.isNotEmpty());
    playKey.setToggleState (s.isPlaying, juce::dontSendNotification);
    playKey.withIcon (s.isPlaying ? Icon::stop : Icon::play);
    playKey.repaint();
}

void TakeCompareDialog::onSessionChanged (juce::uint32 changes)
{
    if (finished)
        return;
    // ほかの所で比べ終わった（曲を開き直した等）：パネルも下げる
    if (! state().compare.active || state().compare.serial != serial)
    {
        finished = true;
        juce::Component::SafePointer<TakeCompareDialog> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr && safe->onFinished) safe->onFinished(); });
        return;
    }
    if (changes & (change::takes | change::view | change::mode))
    {
        // テイクの音程の解析・お手本が後から届いたら数値を入れ直す
        const auto& s = state();
        const auto sig = juce::String ((int) s.takePitch.size()) + "/" + juce::String ((int) s.refPitch.size()) + "/"
                       + juce::String (s.pitchToleranceCents) + "/" + juce::String ((int) s.takeStats.size());
        if (sig != statsSignature)
            rebuild();
    }
    if (changes & (change::transport | change::tracks | change::takes))
    {
        refreshKeys();
        list.repaint();
    }
}

bool TakeCompareDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        playKey.flash();
        session.toggleCompareAudition();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey)
    {
        select (juce::jlimit (0, (int) entries.size() - 1, selectedIndex() + (key.getKeyCode() == juce::KeyPress::upKey ? -1 : 1)));
        return true;
    }
    return false;   // Esc はメイン画面がキャンセルとして扱う
}

//==============================================================================
void TakeCompareDialog::layoutBody (juce::Rectangle<int> r)
{
    rangeArea = r.removeFromTop (rangeRowH);
    playKey.setSize (10, 32);
    const auto pw = juce::jmax (96, playKey.idealWidth());
    playKey.setBounds (rangeArea.withTrimmedLeft (rangeArea.getWidth() - pw).withSizeKeepingCentre (pw, 32));
    r.removeFromTop (8);
    columnArea = r.removeFromTop (columnRowH);
    hintArea = r.removeFromBottom (hintH);
    view.setBounds (r);
    const auto w = view.getMaximumVisibleWidth() > 0 ? view.getMaximumVisibleWidth() : view.getWidth();
    list.setSize (juce::jmax (1, w), (int) entries.size() * rowH);
}

void TakeCompareDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    const auto& s = state();
    const auto& c = s.compare;

    // 範囲：曲全体 / IN・OUT / 選んだ区間
    {
        auto r = rangeArea.withTrimmedRight (playKey.getWidth() + 12).toFloat();
        paint::microLabel (g, r.removeFromTop (14.0f), tr ("compare.range"), colours::textMute);
        const auto a = formatTime (c.from, s.sampleRate(), true), b = formatTime (c.to, s.sampleRate(), true);
        using Scope = dummy::Session::TakeCompare::Scope;
        const auto text = c.scope == Scope::song  ? tr ("compare.range.song")
                        : c.scope == Scope::inOut ? tr ("compare.range.inOut", a, b)
                                                  : tr ("compare.range.segment", a, b);
        g.setColour (colours::text);
        g.setFont (sans (13.5f, Weight::medium));
        g.drawText (text, r, juce::Justification::centredLeft, true);
    }

    // 列の見出し（右から VIB・PITCH・ONSET）
    {
        auto r = columnArea.withTrimmedRight (view.getScrollBarThickness()).toFloat();
        paint::hline (g, r.getBottom() - 0.5f, r.getX(), r.getRight());
        if (pro())
            paint::microLabel (g, r.removeFromRight ((float) vibW), tr ("analysis.vibrato"), colours::textMute);
        paint::microLabel (g, r.removeFromRight ((float) pitchW), tr ("analysis.pitch"), colours::textMute);
        paint::microLabel (g, r.removeFromRight ((float) onsetW), tr ("analysis.onset"), colours::textMute);
        paint::microLabel (g, r.withTrimmedLeft (28.0f), tr ("label.take"), colours::textMute);
    }

    // 使い方（お手本が無ければ、数値が出ない理由も）
    {
        auto r = hintArea.toFloat().withTrimmedTop (8.0f);
        paint::hline (g, (float) hintArea.getY() + 0.5f, r.getX(), r.getRight());
        g.setColour (colours::textDim);
        g.setFont (sans (11.5f));
        const bool noGuide = s.engineAttached && s.refPitch.empty();
        juce::String text = tr ("compare.hint", undoKeyName());
        if (noGuide)
            text << "\n" << tr ("compare.hint.noGuide");
        g.drawFittedText (text, r.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    }
}

//==============================================================================
void TakeCompareDialog::List::paint (juce::Graphics& g)
{
    const auto sel = owner.selectedIndex();
    for (int i = 0; i < (int) owner.entries.size(); ++i)
        paintRow (g, juce::Rectangle<float> (0.0f, (float) (i * rowH), (float) getWidth(), (float) rowH).reduced (0.0f, 2.0f),
                  owner.entries[(size_t) i], i == sel, i == hover);
}

void TakeCompareDialog::List::paintRow (juce::Graphics& g, juce::Rectangle<float> b, const Entry& e, bool selected, bool over)
{
    // 選んだ行はキーキャップのように浮かせ、左にライムの線（トラックカードと同じ形）
    if (selected)
    {
        paint::keycap (g, b, { over, false, true, true });
        g.setColour (colours::signal);
        g.fillRoundedRectangle (b.reduced (1.5f).withWidth (3.0f), 1.5f);
    }
    else if (over)
    {
        g.setColour (colours::raised.withAlpha (0.6f));
        g.fillRoundedRectangle (b, metrics::keyRadius);
    }

    auto r = b.reduced (10.0f, 4.0f);
    paint::led (g, { r.getX() + 4.0f, r.getCentreY() }, 3.0f, colours::signal, selected);
    r.removeFromLeft (18.0f);

    // 数値の列（右から）
    const bool pro = owner.pro();
    auto vib = pro ? r.removeFromRight ((float) vibW) : juce::Rectangle<float>();
    auto pitch = r.removeFromRight ((float) pitchW);
    auto onset = r.removeFromRight ((float) onsetW);
    const auto vf = mono (11.5f, Weight::semibold);
    auto value = [&] (juce::Rectangle<float> area, const juce::String& text, juce::Colour col)
    {
        g.setColour (col);
        g.setFont (vf);
        g.drawText (text, area.withHeight (area.getHeight() * 0.5f + 2.0f), juce::Justification::bottomLeft, false);
    };

    if (e.takeId.isEmpty())
    {
        // いまの採用（元のまま）：数値はテイクごとなので出さない
        g.setColour (colours::text);
        g.setFont (sans (13.5f, Weight::semibold));
        g.drawText (tr ("compare.current"), r.withHeight (r.getHeight() * 0.5f + 2.0f), juce::Justification::bottomLeft, true);
        g.setColour (colours::textMute);
        g.setFont (sans (11.0f));
        g.drawText (tr ("compare.current.detail"), r.withTrimmedTop (r.getHeight() * 0.5f + 4.0f), juce::Justification::topLeft, true);
        return;
    }

    // 名前と録った時刻
    {
        auto top = r.withHeight (r.getHeight() * 0.5f + 2.0f);
        const auto nf = mono (13.0f, Weight::semibold);
        const auto name = e.takeId.toUpperCase();
        g.setColour (colours::text);
        g.setFont (nf);
        g.drawText (name, top, juce::Justification::bottomLeft, false);
        if (e.created.toMilliseconds() > 0)
        {
            g.setColour (colours::textMute);
            g.setFont (mono (10.5f));
            g.drawText (e.created.formatted ("%m/%d %H:%M"), top.withTrimmedLeft (textWidth (nf, name) + 10.0f), juce::Justification::bottomLeft, false);
        }
    }

    // 印：採用中 / 一部採用中・範囲の一部だけ・クリップ
    {
        auto line = r.withTrimmedTop (r.getHeight() * 0.5f + 5.0f).withHeight (16.0f);
        auto tag = [&] (const juce::String& text, juce::Colour col, bool filled)
        {
            const auto f = sans (10.5f, Weight::medium);
            const auto w = textWidth (f, text) + 12.0f;
            if (w > line.getWidth())
                return;
            const auto box = line.removeFromLeft (w);
            line.removeFromLeft (5.0f);
            g.setColour (filled ? col : col.withAlpha (0.16f));
            g.fillRoundedRectangle (box, 2.0f);
            g.setColour (filled ? colours::onFill (col) : col);
            g.setFont (f);
            g.drawText (text, box, juce::Justification::centred, false);
        };
        if (e.rehearsal)           tag (tr ("record.practice"), colours::textDim, true);
        if (e.share >= 0.999f)     tag (tr ("compare.inUse"), colours::signal, true);
        else if (e.share > 0.0f)   tag (tr ("compare.inUsePart"), colours::signal, false);
        if (e.coverage < 0.999f)   tag (tr ("compare.partial", juce::jmax (1, juce::roundToInt (e.coverage * 100.0f))), colours::textDim, false);
        if (e.clip)                tag (tr ("wave.clip"), colours::bad, true);
    }

    // 数値（範囲の中だけ）。お手本・解析が無ければ「-」
    const auto& st = e.stats;
    if (st.has_value() && st->matched > 0)
    {
        const auto [text, col] = onsetText (*st);
        value (onset, text, col);
    }
    else
        value (onset, "-", colours::textMute);

    if (st.has_value() && st->pitchFrames > 50)
    {
        const auto pct = juce::roundToInt (st->inBand * 100.0f);
        value (pitch, tr ("compare.pitch.value", pct), pitchColour (pct));
    }
    else
        value (pitch, "-", colours::textMute);

    if (pro)
    {
        if (st.has_value() && st->vibNotes > 0)
            value (vib, tr ("analysis.vibrato.value", juce::String (st->vibRateHz, 1), juce::roundToInt (st->vibDepthCents)), colours::text);
        else
            value (vib, "-", colours::textMute);

        // プロ：合った入りの数と、音程のずれの平均（数値の下に小さく）
        if (st.has_value() && (st->entries > 0 || st->pitchFrames > 50))
        {
            const auto detail = onset.getUnion (pitch).getUnion (vib).withTrimmedTop (onset.getHeight() * 0.5f + 5.0f);
            g.setColour (colours::textMute);
            g.setFont (sans (10.5f));
            g.drawText (tr ("compare.detail", st->matched, st->entries, juce::roundToInt (st->meanAbsCents)), detail,
                        juce::Justification::topLeft, true);
        }
    }
}

void TakeCompareDialog::List::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
        return;
    owner.select (e.y / rowH);
}

void TakeCompareDialog::List::mouseMove (const juce::MouseEvent& e)
{
    const auto h = juce::isPositiveAndBelow (e.y / rowH, (int) owner.entries.size()) ? e.y / rowH : -1;
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}
} // namespace vb
