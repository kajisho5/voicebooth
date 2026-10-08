#include "HistoryDialog.h"
#include "../WaveLane.h"
#include "../Timeline.h"

namespace vb
{
namespace
{
    constexpr int chartH = 120, columnRowH = 20, maxVisibleRows = 8;
    constexpr int whenW = 96, trackW = 92, kindW = 120, onsetW = 104, pitchW = 64;

    juce::Colour pitchColour (int pct)
    {
        return pct >= 80 ? colours::signal : (pct >= 60 ? colours::warn : colours::bad);
    }

    /** 数値を出せるテイク（原速・原キーで、声のある点が 0.5 秒分より多い） */
    bool hasPitch (const UiSession::HistoryEntry& e) { return e.stats.has_value() && e.stats->pitchFrames > 50; }
}

//==============================================================================
HistoryDialog::HistoryDialog (UiSession& u)
    : DialogPanel (tr ("history.title"), tr ("history.micro")),
      SessionView (u)
{
    addFooterKey (tr ("common.close"), KeyRole::normal, [this] { if (onFinished) onFinished(); });
    onCloseRequest = [this] { if (onFinished) onFinished(); };

    view.setViewedComponent (&list, false);
    view.setScrollBarsShown (true, false);
    view.setScrollBarThickness (8);
    addAndMakeVisible (view);

    rebuild();
    const auto rows = juce::jlimit (3, maxVisibleRows, (int) entries.size());
    setSize (whenW + trackW + kindW + 200 + onsetW + pitchW + 2 * padding,
             headerH + 14 + chartH + 12 + columnRowH + rows * rowH + footerH + 6);
}

void HistoryDialog::rebuild()
{
    const auto& s = state();
    entries = session.history();
    signature = juce::String ((int) s.takeStats.size()) + "/" + juce::String (s.pitchToleranceCents) + "/" + juce::String ((int) entries.size())
              + "/" + juce::String (s.selectedTrack) + "/" + juce::String ((int) s.octaveAlign);
    const auto w = view.getMaximumVisibleWidth() > 0 ? view.getMaximumVisibleWidth() : view.getWidth();
    list.setSize (juce::jmax (1, w), juce::jmax (1, (int) entries.size()) * rowH);
    list.repaint();
    repaint();
}

void HistoryDialog::onSessionChanged (juce::uint32 changes)
{
    // テイクの解析が後から届いた・許容幅やトラックを変えた：入れ直す
    if ((changes & (change::takes | change::view | change::tracks | change::song)) == 0)
        return;
    const auto& s = state();
    const auto sig = juce::String ((int) s.takeStats.size()) + "/" + juce::String (s.pitchToleranceCents) + "/"
                   + juce::String ((int) session.history().size()) + "/" + juce::String (s.selectedTrack) + "/" + juce::String ((int) s.octaveAlign);
    if (sig != signature)
        rebuild();
}

//==============================================================================
void HistoryDialog::layoutBody (juce::Rectangle<int> r)
{
    chartArea = r.removeFromTop (chartH);
    r.removeFromTop (12);
    columnArea = r.removeFromTop (columnRowH);
    view.setBounds (r);
    const auto w = view.getMaximumVisibleWidth() > 0 ? view.getMaximumVisibleWidth() : view.getWidth();
    list.setSize (juce::jmax (1, w), juce::jmax (1, (int) entries.size()) * rowH);
}

void HistoryDialog::paintChart (juce::Graphics& g, juce::Rectangle<float> r)
{
    // いまのトラックの PITCH（許容範囲内の割合）を古い順に。左に 0・50・100% の目盛り
    const auto& s = state();
    const auto track = s.currentTrack().type;
    std::vector<const UiSession::HistoryEntry*> points;
    for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        if (it->track == track && hasPitch (*it))
            points.push_back (&*it);

    auto head = r.removeFromTop (16.0f);
    paint::microLabel (g, head, tr ("history.chart", trackName (track)), colours::textMute);
    if (points.size() >= 2)
    {
        const auto first = juce::roundToInt (points.front()->stats->inBand * 100.0f);
        const auto last = juce::roundToInt (points.back()->stats->inBand * 100.0f);
        g.setColour (colours::textDim);
        g.setFont (sans (11.0f));
        g.drawText (tr ("history.change", first, last, (int) points.size()), head, juce::Justification::centredRight, false);
    }
    r.removeFromTop (4.0f);

    auto plot = r.withTrimmedLeft (34.0f).reduced (6.0f, 6.0f);
    paint::inset (g, r);
    g.setFont (mono (9.5f));
    for (int pct : { 0, 50, 100 })
    {
        const auto y = plot.getBottom() - plot.getHeight() * (float) pct / 100.0f;
        g.setColour (colours::textMute);
        g.drawText (juce::String (pct) + "%", juce::Rectangle<float> (r.getX() + 4.0f, y - 6.0f, 28.0f, 12.0f), juce::Justification::centredRight, false);
        g.setColour (colours::line.withAlpha (pct == 0 ? 0.0f : 0.5f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }
    // 80% の線（苦手なところの目安と同じ）
    {
        const auto y = plot.getBottom() - plot.getHeight() * 0.8f;
        g.setColour (colours::signal.withAlpha (0.35f));
        const float dash[] = { 4.0f, 4.0f };
        g.drawDashedLine ({ plot.getX(), y, plot.getRight(), y }, dash, 2, 1.0f);
    }

    if (points.empty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (11.5f));
        g.drawText (tr (s.refPitch.empty() ? "history.chart.noGuide" : "history.chart.none"), plot, juce::Justification::centred, true);
        return;
    }

    juce::Path line;
    const auto n = (int) points.size();
    auto xAt = [&] (int i) { return n == 1 ? plot.getCentreX() : plot.getX() + plot.getWidth() * (float) i / (float) (n - 1); };
    for (int i = 0; i < n; ++i)
    {
        const auto y = plot.getBottom() - plot.getHeight() * points[(size_t) i]->stats->inBand;
        if (i == 0) line.startNewSubPath (xAt (i), y);
        else        line.lineTo (xAt (i), y);
    }
    g.setColour (colours::ref.withAlpha (0.8f));
    g.strokePath (line, juce::PathStrokeType (1.5f));
    for (int i = 0; i < n; ++i)
    {
        const auto pct = juce::roundToInt (points[(size_t) i]->stats->inBand * 100.0f);
        const auto y = plot.getBottom() - plot.getHeight() * points[(size_t) i]->stats->inBand;
        g.setColour (pitchColour (pct));
        g.fillEllipse (xAt (i) - 3.5f, y - 3.5f, 7.0f, 7.0f);
    }
}

void HistoryDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    paintChart (g, chartArea.toFloat());

    // 列の見出し
    auto r = columnArea.withTrimmedRight (view.getScrollBarThickness()).toFloat().withTrimmedLeft (10.0f);
    paint::hline (g, r.getBottom() - 0.5f, r.getX(), r.getRight());
    paint::microLabel (g, r.removeFromLeft ((float) whenW), tr ("history.col.when"), colours::textMute);
    paint::microLabel (g, r.removeFromLeft ((float) trackW), tr ("history.col.track"), colours::textMute);
    paint::microLabel (g, r.removeFromLeft ((float) kindW), tr ("history.col.kind"), colours::textMute);
    paint::microLabel (g, r.removeFromRight ((float) pitchW), tr ("analysis.pitch"), colours::textMute);
    paint::microLabel (g, r.removeFromRight ((float) onsetW), tr ("analysis.onset"), colours::textMute);
    paint::microLabel (g, r, tr ("history.col.where"), colours::textMute);

    if (entries.empty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (12.0f));
        g.drawText (tr ("history.none"), view.getBounds().toFloat(), juce::Justification::centred, true);
    }
}

//==============================================================================
void HistoryDialog::List::paint (juce::Graphics& g)
{
    const auto& s = owner.state();
    const auto rate = s.sampleRate();
    for (int i = 0; i < (int) owner.entries.size(); ++i)
    {
        const auto& e = owner.entries[(size_t) i];
        auto b = juce::Rectangle<float> (0.0f, (float) (i * rowH), (float) getWidth(), (float) rowH).reduced (0.0f, 2.0f);
        if (i == hover)
        {
            g.setColour (colours::raised.withAlpha (0.6f));
            g.fillRoundedRectangle (b, metrics::keyRadius);
        }
        auto r = b.reduced (10.0f, 0.0f);
        auto cell = [&] (float w, const juce::String& text, juce::Colour col, const juce::Font& f)
        {
            auto a = w > 0.0f ? r.removeFromLeft (w) : r;
            g.setColour (col);
            g.setFont (f);
            g.drawText (text, a, juce::Justification::centredLeft, true);
        };
        const auto mf = mono (11.5f), vf = mono (11.5f, Weight::semibold);
        cell ((float) whenW, e.created.toMilliseconds() > 0 ? e.created.formatted ("%m/%d %H:%M") : juce::String ("-"), colours::textDim, mf);
        cell ((float) trackW, trackName (e.track), e.track == s.currentTrack().type ? colours::text : colours::textDim, sans (12.0f, Weight::medium));
        {
            juce::String kind = tr (e.recMode == project::RecMode::practice ? "record.practice" : "record.delivery");
            if (e.tempoPercent != 100 || e.keyShift != 0)
                kind << " " << juce::String (e.tempoPercent) << "%" << (e.keyShift != 0 ? " " + juce::String (e.keyShift > 0 ? "+" : "") + juce::String (e.keyShift) : juce::String());
            cell ((float) kindW, kind, e.recMode == project::RecMode::practice ? colours::textMute : colours::text, sans (11.5f));
        }
        auto pitch = r.removeFromRight ((float) pitchW);
        auto onset = r.removeFromRight ((float) onsetW);
        cell (0.0f, formatTime (e.start, rate, false) + " - " + formatTime (e.end, rate, false), colours::textDim, mf);

        if (e.stats.has_value() && e.stats->matched > 0)
        {
            const auto ms = juce::roundToInt (e.stats->onsetMs);
            const auto absMs = std::abs (ms);
            const auto text = absMs <= 15 ? tr ("analysis.onset.onTime")
                            : ms > 0     ? tr ("analysis.onset.value", "+" + juce::String (ms))
                                         : tr ("analysis.onset.early", juce::String (absMs));
            g.setColour (absMs <= 30 ? colours::signal : (absMs <= 80 ? colours::warn : colours::bad));
            g.setFont (vf);
            g.drawText (text, onset, juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour (colours::textMute);
            g.setFont (vf);
            g.drawText ("-", onset, juce::Justification::centredLeft, false);
        }
        if (hasPitch (e))
        {
            const auto pct = juce::roundToInt (e.stats->inBand * 100.0f);
            g.setColour (pitchColour (pct));
            g.setFont (vf);
            g.drawText (tr ("compare.pitch.value", pct), pitch, juce::Justification::centredLeft, false);
        }
        else
        {
            g.setColour (colours::textMute);
            g.setFont (vf);
            g.drawText ("-", pitch, juce::Justification::centredLeft, false);
        }
        paint::hline (g, b.getBottom() + 1.5f, b.getX() + 10.0f, b.getRight() - 10.0f);
    }
}

void HistoryDialog::List::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
        return;
    const auto i = e.y / rowH;
    if (! juce::isPositiveAndBelow (i, (int) owner.entries.size()))
        return;
    const auto entry = owner.entries[(size_t) i];   // 写し：移るとトラックが替わって並びを作り直す
    owner.session.goToHistoryEntry (entry);
    if (owner.onFinished)
        owner.onFinished();
}

void HistoryDialog::List::mouseMove (const juce::MouseEvent& e)
{
    const auto h = juce::isPositiveAndBelow (e.y / rowH, (int) owner.entries.size()) ? e.y / rowH : -1;
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}
} // namespace vb
