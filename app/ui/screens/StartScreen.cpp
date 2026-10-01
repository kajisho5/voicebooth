#include "StartScreen.h"
#include "../TopBar.h"
#include "../parts/LedMeter.h"

namespace vb
{
namespace
{
    struct Recent { const char* name; const char* date; const char* length; project::Mode mode; };

    // ダミー（曲名・日付はデータ。翻訳しない）
    const Recent recents[] = {
        { "Tanuki_mix_demo",    "2026-09-30 21:30", "2:16", project::Mode::standard },
        { "Kitsune_offvocal",   "2026-09-27 23:12", "3:48", project::Mode::pro },
        { "Natsu_no_owari_inst","2026-09-21 19:05", "4:02", project::Mode::easy },
    };

    juce::String modeName (project::Mode m)
    {
        switch (m)
        {
            case project::Mode::easy:     return tr ("mode.easy");
            case project::Mode::standard: return tr ("mode.standard");
            case project::Mode::pro:      return tr ("mode.pro");
        }
        return {};
    }

    struct Step { const char* key; float progress; bool failed; };
    const Step steps[] = {
        { "analyze.step.format",     1.0f,  false },
        { "analyze.step.separation", 0.62f, false },
        { "analyze.step.pitch",      0.0f,  false },
        { "analyze.step.tempo",      1.0f,  false },
        { "analyze.step.lyrics",     0.0f,  false },
        { "analyze.step.range",      0.0f,  false },
    };
}

StartScreen::StartScreen (UiSession& u, bool isAnalyzing, bool isFirstRun)
    : SessionView (u), analyzing (isAnalyzing), firstRun (isFirstRun),
      openFolder (tr ("start.openProject")),
      continueKey (tr ("analyze.continue")),
      cancelKey (tr ("common.cancel"))
{
    const char* keys[] = { "start.first.sing", "start.first.deliver", "start.first.detail" };
    const project::Mode modes[] = { project::Mode::easy, project::Mode::standard, project::Mode::pro };
    for (int i = 0; i < 3; ++i)
    {
        auto* k = firstRunKeys.add (new KeyButton (tr (keys[i])));
        k->withFont (sans (13.0f, Weight::semibold));
        k->onClick = [this, m = modes[i]] { session.setMode (m); if (onDone) onDone(); };
        addChildComponent (k);
    }

    openFolder.withIcon (Icon::folder);
    openFolder.onClick = [this] { if (onDone) onDone(); };
    addChildComponent (openFolder);

    continueKey.withLed (colours::signal).withToggle (false);
    continueKey.setToggleState (true, juce::dontSendNotification);
    continueKey.onClick = [this] { if (onDone) onDone(); };
    cancelKey.onClick = [this] { analyzing = false; resized(); repaint(); };
    addChildComponent (continueKey);
    addChildComponent (cancelKey);
}

void StartScreen::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (1080, getWidth() - 80), juce::jmin (700, getHeight() - 80));

    for (auto* k : firstRunKeys) k->setVisible (! analyzing && firstRun);   // 最初の 1 回だけ（DESIGN 2）
    openFolder.setVisible (! analyzing);
    continueKey.setVisible (analyzing);
    cancelKey.setVisible (analyzing);

    if (analyzing)
    {
        auto f = panel.reduced (40, 0).removeFromBottom (80);
        for (auto* k : { &continueKey, &cancelKey })
        {
            k->setSize (10, 36);
            const auto w = juce::jmax (110, k->idealWidth());
            k->setBounds (f.removeFromRight (w).withSizeKeepingCentre (w, 36));
            f.removeFromRight (10);
        }
        return;
    }

    auto r = panel.reduced (40, 0).withTrimmedTop (112).withTrimmedBottom (32);
    firstRunArea = {};
    if (firstRun)
    {
        firstRunArea = r.removeFromBottom (128);
        r.removeFromBottom (24);
    }
    dropArea = r.removeFromLeft (r.getWidth() * 55 / 100);
    r.removeFromLeft (28);
    recentArea = r;

    recentRows.clear();
    auto rows = recentArea.withTrimmedTop (32);
    for (size_t i = 0; i < std::size (recents); ++i)
    {
        recentRows.push_back (rows.removeFromTop (58));
        rows.removeFromTop (6);
    }
    openFolder.setSize (10, 32);
    openFolder.setBounds (rows.removeFromTop (40).removeFromLeft (openFolder.idealWidth()).withSizeKeepingCentre (openFolder.idealWidth(), 32));

    if (! firstRun)
        return;

    auto keys = firstRunArea.withTrimmedTop (44);
    const auto w = (keys.getWidth() - 20) / 3;
    for (auto* k : firstRunKeys)
    {
        k->setBounds (keys.removeFromLeft (w).withHeight (64));
        keys.removeFromLeft (10);
    }
}

void StartScreen::mouseUp (const juce::MouseEvent& e)
{
    if (analyzing) return;

    if (dropArea.contains (e.getPosition()))
    {
        analyzing = true;   // モック：曲を選んだつもりで解析画面へ
        resized();
        repaint();
        return;
    }

    for (auto& row : recentRows)
        if (row.contains (e.getPosition()) && onDone)
            onDone();
}

void StartScreen::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);

    // ブランド
    auto head = panel.reduced (40, 0).removeFromTop (100).toFloat();
    drawBoothMark (g, head.removeFromLeft (40.0f).withSizeKeepingCentre (40.0f, 40.0f), false);
    head.removeFromLeft (16.0f);
    g.setColour (colours::text);
    g.setFont (sans (26.0f, Weight::semibold));
    g.drawText (tr ("app.name"), head.removeFromTop (60.0f).withTrimmedTop (20.0f), juce::Justification::bottomLeft, false);
    g.setColour (colours::textDim);
    g.setFont (sans (13.0f));
    g.drawText (tr ("app.tagline"), head.removeFromTop (24.0f), juce::Justification::topLeft, false);

    if (analyzing) paintAnalyzing (g);
    else           paintHome (g);
}

void StartScreen::paintHome (juce::Graphics& g)
{
    // 曲を読み込む
    {
        const auto r = dropArea.toFloat();
        paint::inset (g, r, 6.0f);
        const float dashes[] = { 6.0f, 5.0f };
        juce::Path border;
        border.addRoundedRectangle (r.reduced (10.0f), 6.0f);
        juce::Path dashed;
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (colours::lineHi);
        g.fillPath (dashed);

        auto c = r.reduced (40.0f);
        drawIcon (g, Icon::note, c.removeFromTop (c.getHeight() * 0.42f).withTrimmedTop (20.0f).withSizeKeepingCentre (44.0f, 44.0f), colours::signal);
        g.setColour (colours::text);
        g.setFont (sans (18.0f, Weight::semibold));
        g.drawText (tr ("start.drop.title"), c.removeFromTop (30.0f), juce::Justification::centred, false);
        g.setColour (colours::textDim);
        g.setFont (sans (12.5f));
        g.drawText (tr ("start.drop.sub"), c.removeFromTop (22.0f), juce::Justification::centred, false);
        c.removeFromTop (12.0f);
        g.setColour (colours::textMute);
        g.setFont (mono (11.0f));
        g.drawText ("wav  flac  aiff  mp3  m4a  ogg", c.removeFromTop (18.0f), juce::Justification::centred, false);
        c.removeFromTop (16.0f);
        g.setFont (sans (11.5f));
        g.drawFittedText (tr ("start.drop.note"), c.toNearestInt(), juce::Justification::centredTop, 2, 1.0f);
    }

    // 最近のプロジェクト
    paint::sectionHeader (g, recentArea.withHeight (24), tr ("start.recent"), tr ("start.recent.sub"));
    for (size_t i = 0; i < recentRows.size(); ++i)
    {
        const auto& rc = recents[i];
        auto r = recentRows[i].toFloat();
        paint::keycap (g, r, { recentRows[i].contains (getMouseXYRelative()), false, false, true });
        r.reduce (14.0f, 8.0f);

        auto right = r.removeFromRight (90.0f);
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        g.drawText (rc.length, right.removeFromTop (right.getHeight() * 0.5f), juce::Justification::bottomRight, false);
        g.setColour (colours::textMute);
        g.setFont (sans (10.5f));
        g.drawText (modeName (rc.mode), right, juce::Justification::topRight, false);

        g.setColour (colours::text);
        g.setFont (sans (13.5f, Weight::semibold));
        g.drawText (rc.name, r.removeFromTop (r.getHeight() * 0.5f), juce::Justification::bottomLeft, true);
        g.setColour (colours::textMute);
        g.setFont (mono (10.5f));
        g.drawText (rc.date, r, juce::Justification::topLeft, false);
    }

    if (! firstRun)
        return;

    // 初回だけ
    paint::hline (g, (float) firstRunArea.getY(), (float) firstRunArea.getX(), (float) firstRunArea.getRight());
    auto q = firstRunArea.withTrimmedTop (10).withHeight (26);
    g.setColour (colours::text);
    g.setFont (sans (14.0f, Weight::semibold));
    g.drawText (tr ("start.first.question"), q, juce::Justification::centredLeft, false);
    g.setColour (colours::textMute);
    g.setFont (sans (11.0f));
    g.drawText (tr ("start.first.note"), q, juce::Justification::centredRight, false);

    // キーの下に説明
    const char* subs[] = { "start.first.sing.sub", "start.first.deliver.sub", "start.first.detail.sub" };
    for (int i = 0; i < firstRunKeys.size(); ++i)
    {
        auto b = firstRunKeys[i]->getBounds();
        g.setColour (colours::textMute);
        g.setFont (sans (10.5f));
        g.drawText (tr (subs[i]), b.translated (0, b.getHeight() + 2).withHeight (16), juce::Justification::centred, true);
    }
}

void StartScreen::paintAnalyzing (juce::Graphics& g)
{
    auto r = panel.reduced (40, 0).withTrimmedTop (120).withTrimmedBottom (100);

    g.setColour (colours::text);
    g.setFont (sans (18.0f, Weight::semibold));
    g.drawText (tr ("analyze.title", state().songName), r.removeFromTop (30), juce::Justification::centredLeft, true);
    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawText (tr ("analyze.sub"), r.removeFromTop (24), juce::Justification::centredLeft, true);
    r.removeFromTop (20);

    for (auto& st : steps)
    {
        auto row = r.removeFromTop (46).toFloat();
        r.removeFromTop (6);

        const bool done = st.progress >= 1.0f;
        const bool running = st.progress > 0.0f && ! done;
        const auto c = done ? colours::signal : (running ? colours::warn : colours::textMute);

        paint::led (g, { row.getX() + 6.0f, row.getCentreY() }, 3.2f, c, done || running);
        row.removeFromLeft (22.0f);

        auto label = row.removeFromLeft (row.getWidth() * 0.42f);
        g.setColour (done || running ? colours::text : colours::textDim);
        g.setFont (sans (13.0f, Weight::medium));
        g.drawText (tr (st.key), label, juce::Justification::centredLeft, true);

        auto status = row.removeFromRight (120.0f);
        g.setColour (c);
        g.setFont (mono (11.0f, Weight::medium));
        g.drawText (done ? tr ("analyze.done") : (running ? juce::String (juce::roundToInt (st.progress * 100.0f)) + "%" : tr ("analyze.waiting")),
                    status, juce::Justification::centredRight, false);

        auto bar = row.reduced (16.0f, 0.0f).withSizeKeepingCentre (row.getWidth() - 32.0f, 6.0f);
        paint::inset (g, bar, 3.0f);
        if (st.progress > 0.0f)
        {
            g.setColour (c);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * st.progress), 3.0f);
        }
    }

    r.removeFromTop (8);
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawFittedText (tr ("analyze.note"), r.removeFromTop (36), juce::Justification::topLeft, 2, 1.0f);
}
} // namespace vb
