#include "RangeDialog.h"

namespace vb
{
namespace
{
    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        std::nth_element (v.begin(), v.begin() + (long) (v.size() / 2), v.end());
        return v[v.size() / 2];
    }
}

RangeDialog::RangeDialog (UiSession& u)
    : DialogPanel (tr ("range.title"), tr ("range.micro")), SessionView (u)
{
    for (int i = 0; i < 2; ++i)
    {
        measureKey[i].setButtonText (tr ("range.measure"));
        measureKey[i].withIcon (Icon::mic);
        measureKey[i].onClick = [this, i] { if (measuring == i) finish(); else start (i); };
        downKey[i].withIcon (Icon::minus);
        upKey[i].withIcon (Icon::plus);
        downKey[i].setTooltip (tr ("range.nudge.down"));
        upKey[i].setTooltip (tr ("range.nudge.up"));
        downKey[i].onClick = [this, i] { nudge (i, -1); };
        upKey[i].onClick = [this, i] { nudge (i, +1); };
        for (auto* k : { &measureKey[i], &downKey[i], &upKey[i] })
            addAndMakeVisible (k);
    }

    // 右から：おすすめのキーにする / 閉じる
    applyKey = &addFooterKey (tr ("range.apply"), KeyRole::primary, [this]
    {
        session.applySuggestedKey();
        if (onCloseRequest) onCloseRequest();
    });
    addFooterKey (tr ("common.close"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });

    setSize (640, headerH + 14 + 300 + footerH + 6);
    refreshKeys();
}

RangeDialog::~RangeDialog()
{
    if (measuring >= 0)
        session.stopRangeMeasure();
}

void RangeDialog::onSessionChanged (juce::uint32 c)
{
    if ((c & (change::practice | change::device | change::meter | change::takes)) != 0)
        refreshKeys();
}

void RangeDialog::refreshKeys()
{
    const auto& s = state();
    const bool live = s.inputLive();
    for (int i = 0; i < 2; ++i)
    {
        measureKey[i].setEnabled (live && (measuring < 0 || measuring == i));
        measureKey[i].setButtonText (measuring == i ? tr ("range.stop") : tr ("range.measure"));
        const auto v = i == 0 ? s.voiceLow : s.voiceHigh;
        downKey[i].setEnabled (measuring < 0);
        upKey[i].setEnabled (measuring < 0);
        (void) v;
    }
    const auto k = session.keySuggestion();
    applyKey->setEnabled (k.ok && measuring < 0 && ! session.deliveryLocked());
    repaint();
}

void RangeDialog::start (int which)
{
    problem.clear();
    measuring = which;
    startedAt = juce::Time::getMillisecondCounterHiRes() * 0.001;
    liveNote = 0.0f;
    session.startRangeMeasure();
    startTimerHz (30);
    refreshKeys();
}

void RangeDialog::finish()
{
    stopTimer();
    const auto which = measuring;
    measuring = -1;
    const auto notes = session.rangeSamples();
    session.stopRangeMeasure();
    if (which < 0) return;

    if (notes.size() < minVoiced)
    {
        problem = tr ("range.noVoice");
    }
    else
    {
        const auto n = juce::roundToInt (median (notes));
        auto low = state().voiceLow, high = state().voiceHigh;
        if (which == 0) low = n; else high = n;
        if (low >= 0 && high >= 0 && high <= low)
            problem = tr ("range.inverted");
        session.setVoiceRange (low < 0 ? (which == 0 ? n : -1) : low, high < 0 ? (which == 1 ? n : -1) : high);
    }
    refreshKeys();
}

void RangeDialog::nudge (int which, int delta)
{
    auto low = state().voiceLow, high = state().voiceHigh;
    auto& v = which == 0 ? low : high;
    v = v < 0 ? (which == 0 ? 48 : 67) + delta : juce::jlimit (24, 96, v + delta);   // まだ無ければ C3 / G4 から
    problem.clear();
    session.setVoiceRange (low, high);
}

void RangeDialog::timerCallback()
{
    const auto& notes = session.rangeSamples();
    if (! notes.empty())
    {
        const auto from = notes.size() > 12 ? notes.size() - 12 : 0;
        liveNote = median (std::vector<float> (notes.begin() + (long) from, notes.end()));
    }
    if (juce::Time::getMillisecondCounterHiRes() * 0.001 - startedAt >= measureSeconds)
        finish();
    repaint (liveArea);
    repaint (rowArea[juce::jmax (0, measuring)]);
}

void RangeDialog::layoutBody (juce::Rectangle<int> r)
{
    introArea = r.removeFromTop (54);
    r.removeFromTop (8);
    for (int i = 0; i < 2; ++i)
    {
        rowArea[i] = r.removeFromTop (54);
        r.removeFromTop (8);
        auto keys = rowArea[i].reduced (12, 11);
        upKey[i].setBounds (keys.removeFromRight (32));
        keys.removeFromRight (4);
        downKey[i].setBounds (keys.removeFromRight (32));
        keys.removeFromRight (10);
        measureKey[i].setSize (10, 32);
        measureKey[i].setBounds (keys.removeFromRight (juce::jmax (110, measureKey[i].idealWidth())));
    }
    liveArea = r.removeFromTop (34);
    r.removeFromTop (6);
    resultArea = r;
}

void RangeDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    const auto& s = state();

    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawFittedText (s.inputLive() ? tr ("range.intro") : tr ("range.noInput"), introArea, juce::Justification::topLeft, 3, 1.0f);

    for (int i = 0; i < 2; ++i)
    {
        auto r = rowArea[i].toFloat();
        paint::inset (g, r, 6.0f);
        auto t = rowArea[i].reduced (14, 0);
        g.setColour (colours::text);
        g.setFont (sans (13.0f, Weight::semibold));
        g.drawText (tr (i == 0 ? "range.low" : "range.high"), t.removeFromLeft (150), juce::Justification::centredLeft, false);
        const auto v = i == 0 ? s.voiceLow : s.voiceHigh;
        g.setColour (measuring == i ? colours::signal : (v >= 0 ? (juce::Colour) colours::text : (juce::Colour) colours::textMute));
        g.setFont (mono (20.0f, Weight::semibold));
        const auto shown = measuring == i ? (liveNote > 0.0f ? dummy::noteName (liveNote) : juce::String ("--"))
                                          : (v >= 0 ? dummy::noteName ((float) v) : juce::String ("--"));
        g.drawText (shown, t.removeFromLeft (90), juce::Justification::centredLeft, false);
    }

    // 測っている間：残り時間
    if (measuring >= 0)
    {
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() * 0.001 - startedAt;
        auto bar = liveArea.toFloat().withSizeKeepingCentre ((float) liveArea.getWidth(), 4.0f);
        g.setColour (colours::bgDeep);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (colours::signal);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, elapsed / measureSeconds)), 2.0f);
        g.setColour (colours::textDim);
        g.setFont (sans (11.5f));
        g.drawText (tr (measuring == 0 ? "range.sing.low" : "range.sing.high"), liveArea.withTrimmedBottom (20), juce::Justification::centredLeft, false);
    }
    else if (problem.isNotEmpty())
    {
        g.setColour (colours::warn);
        g.setFont (sans (12.0f));
        g.drawFittedText (problem, liveArea, juce::Justification::centredLeft, 2, 1.0f);
    }

    // この曲のお手本と、おすすめのキー
    auto r = resultArea;
    g.setFont (sans (12.5f));
    const auto k = session.keySuggestion();
    if (s.refPitch.empty())
    {
        g.setColour (colours::textMute);
        g.drawFittedText (tr ("range.result.noGuide"), r, juce::Justification::topLeft, 3, 1.0f);
        return;
    }
    const auto gr = session.guideRange();
    if (! gr.known)
        return;
    g.setColour (colours::textDim);
    g.drawText (tr ("range.result.guide", dummy::noteName (gr.low), dummy::noteName (gr.high)), r.removeFromTop (22), juce::Justification::centredLeft, false);
    if (! k.ok)
        return;
    const auto v = (k.shift > 0 ? "+" : "") + juce::String (k.shift);
    g.setColour (k.fits ? colours::signal : colours::warn);
    g.setFont (sans (14.0f, Weight::semibold));
    g.drawText (k.octave < 0 ? tr ("range.suggest.octDown", v) : k.octave > 0 ? tr ("range.suggest.octUp", v) : tr ("range.suggest", v),
                r.removeFromTop (26), juce::Justification::centredLeft, false);
    g.setColour (colours::textDim);
    g.setFont (sans (12.0f));
    if (! k.fits)
        g.drawFittedText (tr ("range.suggest.over", juce::String (juce::roundToInt (k.overLow)), juce::String (juce::roundToInt (k.overHigh))),
                          r.removeFromTop (36), juce::Justification::topLeft, 2, 1.0f);
    else
        g.drawFittedText (tr ("range.result.fits"), r.removeFromTop (36), juce::Justification::topLeft, 2, 1.0f);
}
} // namespace vb
