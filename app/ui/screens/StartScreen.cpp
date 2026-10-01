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
        { "Demo_song",    "2026-09-30 21:30", "2:16", project::Mode::standard },
        { "Practice_offvocal",   "2026-09-27 23:12", "3:48", project::Mode::pro },
        { "Sample_inst","2026-09-21 19:05", "4:02", project::Mode::easy },
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

    /** 解析の項目（DESIGN 7.1）。B1 で本物なのは最初の 1 つ（形式・長さ・SR・チャンネル＋波形） */
    const char* const stepKeys[] = {
        "analyze.step.format",
        "analyze.step.separation",
        "analyze.step.pitch",
        "analyze.step.tempo",
        "analyze.step.lyrics",
        "analyze.step.range",
    };
}

StartScreen::StartScreen (UiSession& u, audio::SongLoader& l, bool isFirstRun)
    : SessionView (u), loader (l), firstRun (isFirstRun),
      openFolder (tr ("start.openProject")),
      continueKey (tr ("analyze.continue")),
      cancelKey (tr ("common.cancel")),
      anotherKey (tr ("analyze.chooseAnother"))
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
    cancelKey.onClick = [this] { loader.cancel(); setPhase (Phase::home); };
    anotherKey.withIcon (Icon::folder);
    anotherKey.onClick = [this] { setPhase (Phase::home); chooseFile(); };
    addChildComponent (continueKey);
    addChildComponent (cancelKey);
    addChildComponent (anotherKey);
}

StartScreen::~StartScreen()
{
    // 画面を閉じたら読み込みも止める（結果を受け取る相手がいなくなるため）
    if (phase == Phase::loading)
        loader.cancel();
}

void StartScreen::setPhase (Phase p)
{
    phase = p;
    if (phase == Phase::loading) startTimerHz (30);
    else                         stopTimer();
    resized();
    repaint();
}

void StartScreen::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("start.chooser.title"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   audio::songWildcard());
    juce::Component::SafePointer<StartScreen> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f != juce::File())
                                  safe->openFile (f);
                          });
}

void StartScreen::openFile (const juce::File& f)
{
    file = f;
    info = {};
    error = audio::LoadResult::Error::none;

    if (! audio::hasSongExtension (f))
    {
        setPhase (Phase::failed);   // 拡張子で弾く（中身を読む前に分かるもの）
        return;
    }

    setPhase (Phase::loading);
    juce::Component::SafePointer<StartScreen> safe (this);
    loader.start (f, [safe] (audio::LoadResult r)
    {
        if (safe != nullptr)
            safe->loadFinished (std::move (r));
    });
}

void StartScreen::loadFinished (audio::LoadResult r)
{
    info = r.info;
    error = r.error;

    if (! r.ok())
    {
        setPhase (Phase::failed);
        return;
    }

    // 曲を差し替える（後ろのメイン画面もこの時点で実波形になる）
    session.loadSong (r.info.file, juce::roundToInt (r.info.sampleRate), r.info.lengthSamples, r.overview, r.audio);
    setPhase (Phase::loaded);
}

//==============================================================================
bool StartScreen::isInterestedInFileDrag (const juce::StringArray& files)
{
    return phase != Phase::loading && files.size() > 0;
}

void StartScreen::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void StartScreen::fileDragExit (const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void StartScreen::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    openFile (juce::File (files[0]));   // 複数なら先頭だけ
}

void StartScreen::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (1080, getWidth() - 80), juce::jmin (700, getHeight() - 80));

    const bool home = phase == Phase::home;
    for (auto* k : firstRunKeys) k->setVisible (home && firstRun);   // 最初の 1 回だけ（DESIGN 2）
    openFolder.setVisible (home);
    continueKey.setVisible (phase == Phase::loaded);
    cancelKey.setVisible (phase == Phase::loading);
    anotherKey.setVisible (phase == Phase::loaded || phase == Phase::failed);

    if (! home)
    {
        auto f = panel.reduced (40, 0).removeFromBottom (80);
        for (auto* k : { &continueKey, &cancelKey, &anotherKey })
        {
            if (! k->isVisible()) continue;
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
    if (phase != Phase::home) return;

    if (dropArea.contains (e.getPosition()))
    {
        chooseFile();
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

    if (phase == Phase::home) paintHome (g);
    else                      paintAnalyzing (g);
}

void StartScreen::paintHome (juce::Graphics& g)
{
    // 曲を読み込む
    {
        const auto r = dropArea.toFloat();
        paint::inset (g, r, 6.0f);
        const bool hot = dragHover || dropArea.contains (getMouseXYRelative());
        if (dragHover)
        {
            g.setColour (colours::signal.withAlpha (0.06f));
            g.fillRoundedRectangle (r.reduced (10.0f), 6.0f);
        }
        const float dashes[] = { 6.0f, 5.0f };
        juce::Path border;
        border.addRoundedRectangle (r.reduced (10.0f), 6.0f);
        juce::Path dashed;
        juce::PathStrokeType (dragHover ? 1.6f : 1.2f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (dragHover ? colours::signal : (hot ? colours::textMute : colours::lineHi));
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

juce::String StartScreen::songInfoLine() const
{
    // 例：WAV  48 kHz  24bit  ステレオ  3:45（単位・形式名は翻訳しない）
    const auto sr = juce::roundToInt (info.sampleRate);
    const auto ch = info.numChannels == 1 ? tr ("analyze.mono")
                  : info.numChannels == 2 ? tr ("analyze.stereo")
                                          : juce::String (info.numChannels) + " ch";
    juce::StringArray parts { info.extension().toUpperCase(), formatKhz (sr) + " kHz" };
    // ビット数は非圧縮・可逆だけ（mp3 / ogg などの値は意味がない）
    const bool pcm = juce::StringArray { "wav", "aif", "aiff", "flac" }.contains (info.extension());
    if (pcm && info.bitsPerSample > 0)
        parts.add (juce::String (info.bitsPerSample) + "bit" + (info.floatingPoint ? " float" : ""));
    parts.add (ch);
    parts.add (formatTime (info.lengthSamples, juce::jmax (1, sr), true));
    return parts.joinIntoString ("   ");
}

juce::String StartScreen::errorText() const
{
    const auto name = file.getFileName();
    if (error == audio::LoadResult::Error::none)
        return tr ("load.error.notSong", name);   // 拡張子で弾いた
    if (error == audio::LoadResult::Error::cancelled)
        return tr ("load.error.cancelled");
    return tr (audio::errorKey (error), name, audio::maxSongMinutes);
}

void StartScreen::paintAnalyzing (juce::Graphics& g)
{
    auto r = panel.reduced (40, 0).withTrimmedTop (120).withTrimmedBottom (100);
    const auto name = file.getFileNameWithoutExtension();

    g.setColour (colours::text);
    g.setFont (sansFor (name, 18.0f, Weight::semibold));
    const auto title = phase == Phase::failed ? tr ("analyze.titleFailed", file.getFileName())
                     : phase == Phase::loaded ? tr ("analyze.titleDone", name)
                                              : tr ("analyze.title", name);
    g.drawText (title, r.removeFromTop (30), juce::Justification::centredLeft, true);
    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawText (tr ("analyze.sub"), r.removeFromTop (24), juce::Justification::centredLeft, true);
    r.removeFromTop (20);

    for (size_t i = 0; i < std::size (stepKeys); ++i)
    {
        auto row = r.removeFromTop (46).toFloat();
        r.removeFromTop (6);

        // 1 行目だけ本物。ほかは SKIP（このバージョンでは解析しない）
        const bool real = i == 0;
        const bool failed = real && phase == Phase::failed;
        const bool done = real && phase == Phase::loaded;
        const bool running = real && phase == Phase::loading;
        const auto progress = done ? 1.0f : (running ? loader.getProgress() : 0.0f);
        const auto c = failed ? colours::bad : (done ? colours::signal : (running ? colours::warn : colours::textMute));

        paint::led (g, { row.getX() + 6.0f, row.getCentreY() }, 3.2f, c, real);
        row.removeFromLeft (22.0f);

        auto label = row.removeFromLeft (row.getWidth() * 0.42f);
        g.setColour (real ? colours::text : colours::textDim);
        g.setFont (sans (13.0f, Weight::medium));
        g.drawText (tr (stepKeys[i]), label, juce::Justification::centredLeft, true);

        auto status = row.removeFromRight (120.0f);
        g.setColour (c);
        g.setFont (mono (11.0f, Weight::medium));
        const auto statusText = ! real ? tr ("analyze.skip")
                              : failed ? tr ("analyze.failed")
                              : done   ? tr ("analyze.done")
                                       : juce::String (juce::roundToInt (progress * 100.0f)) + "%";
        g.drawText (statusText, status, juce::Justification::centredRight, false);

        auto bar = row.reduced (16.0f, 0.0f).withSizeKeepingCentre (row.getWidth() - 32.0f, 6.0f);
        if (! real)
        {
            paint::hline (g, std::round (bar.getCentreY()), bar.getX(), bar.getRight(), colours::line.withAlpha (0.6f));
            continue;
        }

        // 終わったら棒の代わりに結果（形式・SR・長さ）か、失敗の理由
        if (done || failed)
        {
            g.setColour (failed ? colours::bad : colours::textDim);
            const auto text = failed ? errorText() : songInfoLine();
            g.setFont (failed ? sansFor (text, 12.0f) : mono (11.5f, Weight::medium));
            g.drawFittedText (text, row.reduced (16.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
            continue;
        }

        paint::inset (g, bar, 3.0f);
        if (progress > 0.0f)
        {
            g.setColour (c);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 3.0f);
        }
    }

    r.removeFromTop (8);
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawFittedText (tr ("analyze.skipNote") + "\n" + tr ("analyze.note"), r.removeFromTop (40), juce::Justification::topLeft, 3, 1.0f);
}
} // namespace vb
