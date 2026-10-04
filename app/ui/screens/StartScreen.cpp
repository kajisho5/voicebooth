#include "StartScreen.h"
#include "../TopBar.h"
#include "../parts/LedMeter.h"
#include "../Animator.h"
#include "project/ProjectFile.h"
#include "separation/SeparatorClient.h"
#include "models/ModelManifest.h"
#include "models/ModelDownloader.h"

namespace vb
{
namespace
{
    struct Recent { const char* name; const char* date; const char* length; project::Mode mode; };

    // 見本（UI_MOCK）のダミー（曲名・日付はデータ。翻訳しない）
    const Recent dummyRecents[] = {
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

    /** 解析の項目（DESIGN 7.1）。B1 で本物なのは最初の 1 つ（形式・長さ・SR・チャンネル＋波形） */
    const char* const stepKeys[] = {
        "analyze.step.format",
        "analyze.step.separation",
        "analyze.step.pitch",
        "analyze.step.tempo",
        "analyze.step.range",
    };
}

StartScreen::StartScreen (UiSession& u, audio::SongLoader& l, bool isFirstRun)
    : SessionView (u), loader (l), firstRun (isFirstRun),
      openFolder (tr ("start.openProject")),
      continueKey (tr ("analyze.continue")),
      cancelKey (tr ("common.cancel")),
      anotherKey (tr ("analyze.chooseAnother")),
      originalKey (tr ("start.fromOriginal")),
      modelKey (tr ("model.getButton"))
{
    const char* keys[] = { "start.first.sing", "start.first.deliver", "start.first.detail" };
    const project::Mode modes[] = { project::Mode::easy, project::Mode::standard, project::Mode::pro };
    for (int i = 0; i < 3; ++i)
    {
        auto* k = firstRunKeys.add (new KeyButton (tr (keys[i])));
        k->withFont (sans (13.0f, Weight::semibold));
        k->onClick = [this, m = modes[i]]
        {
            session.setMode (m);
            // 本物のアプリでまだ曲が無ければ、閉じずにそのまま曲を選んでもらう（閉じると見本の画面が見えてしまう）
            if (state().engineAttached && state().projectFile == juce::File())
            {
                firstRun = false;
                resized();
                repaint();
                if (onModeChosen) onModeChosen();
                return;
            }
            if (onModeChosen) onModeChosen();
            if (onDone) onDone();
        };
        addChildComponent (k);
    }

    openFolder.withIcon (Icon::folder);
    openFolder.onClick = [this] { if (state().engineAttached) chooseProject(); else if (onDone) onDone(); };
    refreshRecents();
    addChildComponent (openFolder);

    setWantsKeyboardFocus (true);   // 読み込みが終わったら Enter で進む
    continueKey.withLed (colours::signal).withToggle (false);
    continueKey.setToggleState (true, juce::dontSendNotification);
    continueKey.onClick = [this] { if (onDone) onDone(); };
    cancelKey.onClick = [this]
    {
        if (phase == Phase::separating) { session.stopSeparation(); return; }   // 止まったら知らせが来て最初の画面へ
        loader.cancel();
        setPhase (Phase::home);
    };
    anotherKey.withIcon (Icon::folder);
    anotherKey.onClick = [this] { setPhase (Phase::home); chooseFile(); };
    addChildComponent (continueKey);
    addChildComponent (cancelKey);
    addChildComponent (anotherKey);

    originalKey.withIcon (Icon::mic);
    originalKey.onClick = [this] { startFromOriginal (guideFile); };
    addChildComponent (originalKey);

    // 分離モデルがまだ無い時だけ（ダウンロードは押した時だけ。11.7）
    modelKey.withIcon (Icon::download);
    modelKey.onClick = [this] { if (onInstallModels) onInstallModels(); };
    addChildComponent (modelKey);
}

bool StartScreen::canInstallModels() const
{
    // 分離・リードボーカル・音程のどれかがまだ無い（受け取り中は出さない）
    return session.modelsMissing();
}

StartScreen::~StartScreen()
{
    // 画面を閉じたら読み込みも止める（結果を受け取る相手がいなくなるため）
    if (phase == Phase::loading)
        loader.cancel();
    if (phase == Phase::separating)
        session.stopSeparation();
}

void StartScreen::setPhase (Phase p)
{
    phase = p;
    if (phase == Phase::loading || phase == Phase::separating) startTimerHz (30);
    else                         stopTimer();
    resized();
    repaint();
    if (phase == Phase::loaded && isShowing())
        grabKeyboardFocus();   // Enter で［メイン画面へ進む］
}

bool StartScreen::keyPressed (const juce::KeyPress& k)
{
    if (phase == Phase::loaded && k == juce::KeyPress::returnKey && onDone != nullptr)
    {
        onDone();
        return true;
    }
    return false;
}

void StartScreen::chooseFile (bool guide)
{
    chooser = std::make_unique<juce::FileChooser> (guide ? tr ("start.chooser.guide") : tr ("start.chooser.title"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   audio::songWildcard());
    juce::Component::SafePointer<StartScreen> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, guide] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe == nullptr || f == juce::File())
                                  return;
                              if (guide) safe->setGuide (f);
                              else       safe->openFile (f);
                          });
}

void StartScreen::refreshRecents()
{
    recents.clear();
    if (! state().engineAttached)
    {
        for (auto& r : dummyRecents)
            recents.push_back ({ r.name, r.date, r.length, r.mode, {} });
        return;
    }
    // 最近のプロジェクト（B14）：名前・最後に保存した日時・長さ・モード
    for (auto& path : state().recentProjects)
    {
        const juce::File f (path);
        const auto l = project::fromJson (f.loadFileAsString());
        if (! l.ok)
            continue;
        RecentRow row;
        row.name = f.getFileNameWithoutExtension();
        row.date = f.getLastModificationTime().formatted ("%Y-%m-%d %H:%M");
        row.length = l.project.sampleRate > 0 ? formatTime (l.project.lengthSamples, l.project.sampleRate, false) : juce::String();
        row.mode = l.project.modeLast;
        row.file = f;
        recents.push_back (row);
        if (recents.size() >= 3)
            break;
    }
}

void StartScreen::chooseProject()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("start.chooser.project"),
                                                   UiSession::projectFolderFor ({}).getParentDirectory(),
                                                   juce::String ("*") + project::fileExtension);
    juce::Component::SafePointer<StartScreen> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f != juce::File())
                                  safe->openFile (f);
                          });
}

bool StartScreen::openProject (const juce::File& f)
{
    // .vbooth：中身を確かめて、曲のコピーを読み込む（読み終わったら続きを戻す。B14）
    projectError = {};
    const auto l = project::fromJson (f.loadFileAsString());
    if (! l.ok)
    {
        projectError = l.error;
        return false;
    }
    // 書いてある場所（相対か、コピーし終える前に保存された元の場所）。無ければプロジェクトの中のコピー（Audio/）
    auto song = project::findMedia (f.getParentDirectory(), l.project.songPath, "Audio");
    if (! song.existsAsFile())
    {
        projectError = "project.error.songMissing";
        return false;
    }
    session.setPendingProject (f, l);
    file = song;
    return true;
}

void StartScreen::setGuide (const juce::File& f)
{
    // 読むのはオフボを開いた後（時間合わせにオフボが要る）。ここでは覚えるだけ
    if (! audio::hasSongExtension (f))
        return;
    guideFile = f;
    resized();
    repaint();
}

void StartScreen::startFromOriginal (const juce::File& original)
{
    if (original == juce::File())
        return;
    guideFile = original;
    if (! session.separationAvailable())
    {
        // モデルが無い：入れられるなら確認へ（ダウンロードは押した時だけ。11.7）。入れられなければ理由を出す
        if (onNeedModel && separation::SeparatorClient::executable().existsAsFile() && ! models::trustedKeys().empty())
        {
            onNeedModel (original);
            return;
        }
        file = original;
        fromOriginal = true;
        separationError = tr ("separation.noModelOriginal");
        setPhase (Phase::failed);
        return;
    }

    file = original;
    info = {};
    error = audio::LoadResult::Error::none;
    projectError = {};
    separationError = {};
    fromOriginal = true;
    setPhase (Phase::separating);
    juce::Component::SafePointer<StartScreen> safe (this);
    session.makeOffVocal (original, [safe, original] (juce::File made, juce::String why)
    {
        if (safe == nullptr)
            return;
        if (made == juce::File())
        {
            if (why == tr ("separation.stopped")) { safe->fromOriginal = false; safe->setPhase (Phase::home); return; }
            safe->file = original;
            safe->separationError = why;
            safe->setPhase (Phase::failed);
            return;
        }
        // 作ったオフボを開く。開き終わったら原曲をお手本に重ねる（原曲 − オフボ = 分離した声。B9 の引き算）
        safe->guideFile = original;
        safe->openFile (made);
    });
}

void StartScreen::openFile (const juce::File& f)
{
    file = f;
    info = {};
    error = audio::LoadResult::Error::none;
    projectError = {};
    separationError = {};
    if (phase != Phase::separating)
        fromOriginal = false;

    if (f.hasFileExtension (project::fileExtension) && ! openProject (f))
    {
        file = f;
        setPhase (Phase::failed);
        return;
    }

    if (! audio::hasSongExtension (file))
    {
        setPhase (Phase::failed);   // 拡張子で弾く（中身を読む前に分かるもの）
        return;
    }

    setPhase (Phase::loading);
    juce::Component::SafePointer<StartScreen> safe (this);
    loader.start (file, [safe] (audio::LoadResult r)
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

    // 曲を差し替える（後ろのメイン画面もこの時点で実波形になる）。知らせで別の画面が開いてこの画面が消えることがある
    const auto guide = std::exchange (guideFile, juce::File());
    auto& ui = session;
    juce::Component::SafePointer<StartScreen> safe (this);
    ui.loadSong (r.info.file, juce::roundToInt (r.info.sampleRate), r.info.lengthSamples, r.overview, r.audio);
    if (safe != nullptr)
        setPhase (Phase::loaded);

    // お手本も入っていれば、オフボと時間を合わせて重ねる（裏で。結果はメイン画面の知らせ）
    if (guide != juce::File())
        ui.loadGuide (guide);
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

void StartScreen::fileDragMove (const juce::StringArray&, int x, int y)
{
    dragPos = { x, y };
    repaint();
}

void StartScreen::fileDragExit (const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void StartScreen::filesDropped (const juce::StringArray& files, int x, int y)
{
    dragHover = false;
    // お手本の枠に落としたらお手本、それ以外はオフボ（複数なら先頭だけ）
    if (phase == Phase::home && guideArea.contains (x, y))
    {
        setGuide (juce::File (files[0]));
        return;
    }
    openFile (juce::File (files[0]));
}

void StartScreen::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (1080, getWidth() - 80), juce::jmin (700, getHeight() - 80));

    const bool home = phase == Phase::home;
    for (auto* k : firstRunKeys) k->setVisible (home && firstRun);   // 最初の 1 回だけ（DESIGN 2）
    openFolder.setVisible (home);
    originalKey.setVisible (home && guideFile != juce::File() && state().engineAttached);
    modelKey.setVisible (home && canInstallModels());
    continueKey.setVisible (phase == Phase::loaded);
    cancelKey.setVisible (phase == Phase::loading || phase == Phase::separating);
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
    localNoteArea = dropArea.removeFromBottom (30);   // 手元のファイルだけ（サブスクの曲は入れられない）
    dropArea.removeFromBottom (8);
    guideArea = dropArea.removeFromBottom (dropArea.getHeight() * 36 / 100);
    dropArea.removeFromBottom (12);
    r.removeFromLeft (28);
    recentArea = r;

    recentRows.clear();
    auto rows = recentArea.withTrimmedTop (32);
    for (size_t i = 0; i < recents.size(); ++i)
    {
        recentRows.push_back (rows.removeFromTop (58));
        rows.removeFromTop (6);
    }
    if (recents.empty())
        rows.removeFromTop (46);   // 「まだありません」の 1 行（paint）の下にキーを置く
    if (originalKey.isVisible())
    {
        // お手本の枠の右下（原曲だけで始める。B16）
        originalKey.setSize (10, 32);
        const auto w = originalKey.idealWidth();
        originalKey.setBounds (guideArea.reduced (22, 20).removeFromBottom (32).removeFromRight (w));
    }
    openFolder.setSize (10, 32);
    openFolder.setBounds (rows.removeFromTop (40).removeFromLeft (openFolder.idealWidth()).withSizeKeepingCentre (openFolder.idealWidth(), 32));
    if (modelKey.isVisible())
    {
        modelKey.setSize (10, 32);
        modelKey.setBounds (rows.removeFromTop (40).removeFromLeft (modelKey.idealWidth()).withSizeKeepingCentre (modelKey.idealWidth(), 32));
    }

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
    if (guideArea.contains (e.getPosition()))
    {
        chooseFile (true);
        return;
    }

    for (size_t i = 0; i < recentRows.size(); ++i)
        if (recentRows[i].contains (e.getPosition()))
        {
            if (recents[i].file != juce::File()) openFile (recents[i].file);
            else if (onDone)                    onDone();
            return;
        }
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
    // オフボ（カラオケ。時間の基準）と、お手本（声入りの原曲。任意）。ドラッグ中は落とす先の枠を光らせる
    const bool overGuide = dragHover && guideArea.contains (dragPos);
    paintSlot (g, dropArea, Icon::note, tr ("start.drop.title"), tr ("start.drop.sub"), tr ("start.drop.note"),
               dragHover && ! overGuide, false);
    paintSlot (g, guideArea, Icon::mic,
               guideFile != juce::File() ? tr ("start.guide.set", guideFile.getFileName()) : tr ("start.guide.title"),
               guideFile != juce::File() ? tr ("start.guide.setSub") : tr ("start.drop.sub"),
               guideFile != juce::File() && originalKey.isVisible() ? tr ("start.guide.noteSet") : tr ("start.guide.note"),
               overGuide, guideFile != juce::File(), originalKey.isVisible() ? originalKey.getWidth() + 16 : 0);

    g.setColour (colours::textDim);
    g.setFont (sans (11.0f));
    g.drawFittedText (tr ("start.localOnly"), localNoteArea, juce::Justification::topLeft, 2, 1.0f);

    // 最近のプロジェクト
    paint::sectionHeader (g, recentArea.withHeight (24), tr ("start.recent"), tr ("start.recent.sub"));
    if (recents.empty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (12.0f));
        g.drawText (tr ("start.recent.empty"), recentArea.withTrimmedTop (32).withHeight (40), juce::Justification::centredLeft, false);
    }
    for (size_t i = 0; i < recentRows.size(); ++i)
    {
        const auto& rc = recents[i];
        auto r = recentRows[i].toFloat();
        paint::keycap (g, r, { recentRows[i].contains (getMouseXYRelative()), false, false, true });
        r.reduce (14.0f, 8.0f);

        auto right = r.removeFromRight (90.0f);
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        g.drawText (rc.length, right.removeFromTop (right.getHeight() * 0.5f), juce::Justification::bottomRight, false);   // （データ）
        g.setColour (colours::textMute);
        g.setFont (sans (10.5f));
        g.drawText (modeName (rc.mode), right, juce::Justification::topRight, false);

        g.setColour (colours::text);
        g.setFont (sansFor (rc.name, 13.5f, Weight::semibold));
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

void StartScreen::paintSlot (juce::Graphics& g, juce::Rectangle<int> area, Icon icon, const juce::String& title,
                             const juce::String& sub, const juce::String& note, bool dropping, bool done, int textRightInset)
{
    const auto r = area.toFloat();
    paint::inset (g, r, 6.0f);
    const bool hot = dropping || area.contains (getMouseXYRelative());
    if (dropping)
    {
        g.setColour (colours::signal.withAlpha (0.06f));
        g.fillRoundedRectangle (r.reduced (10.0f), 6.0f);
    }
    const float dashes[] = { 6.0f, 5.0f };
    juce::Path border;
    border.addRoundedRectangle (r.reduced (10.0f), 6.0f);
    juce::Path dashed;
    juce::PathStrokeType (dropping ? 1.6f : 1.2f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (dropping || done ? colours::signal : (hot ? colours::textMute : colours::lineHi));
    g.fillPath (dashed);

    // 背の高い枠（オフボ）はアイコンを上に大きく、低い枠（お手本）は左に小さく
    auto c = r.reduced (28.0f, 18.0f).withTrimmedRight ((float) textRightInset);
    const bool tall = r.getHeight() > 220.0f;
    if (tall)
        drawIcon (g, icon, c.removeFromTop (c.getHeight() * 0.38f).withTrimmedTop (16.0f).withSizeKeepingCentre (44.0f, 44.0f), colours::signal);
    else
    {
        drawIcon (g, done ? Icon::check : icon, c.removeFromLeft (40.0f).withSizeKeepingCentre (28.0f, 28.0f), done ? colours::signal : colours::ref);
        c.removeFromLeft (10.0f);
        c = c.withSizeKeepingCentre (c.getWidth(), juce::jmin (c.getHeight(), 92.0f));
    }
    const auto just = tall ? juce::Justification::centred : juce::Justification::centredLeft;
    g.setColour (colours::text);
    g.setFont (sansFor (title, tall ? 18.0f : 14.5f, Weight::semibold));
    g.drawText (title, c.removeFromTop (tall ? 30.0f : 24.0f), just, true);
    g.setColour (colours::textDim);
    g.setFont (sans (tall ? 12.5f : 12.0f));
    g.drawText (sub, c.removeFromTop (22.0f), just, true);
    if (tall)
    {
        c.removeFromTop (12.0f);
        g.setColour (colours::textMute);
        g.setFont (mono (11.0f));
        g.drawText ("wav  flac  aiff  mp3  m4a  ogg", c.removeFromTop (18.0f), juce::Justification::centred, false);
    }
    c.removeFromTop (tall ? 12.0f : 6.0f);
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawFittedText (note, c.toNearestInt(), tall ? juce::Justification::centredTop : juce::Justification::topLeft, 2, 1.0f);
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
    if (separationError.isNotEmpty())
        return separationError;
    if (projectError.isNotEmpty())
        return tr (projectError.toRawUTF8(), name);
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
    const auto title = phase == Phase::failed && fromOriginal ? tr ("analyze.titleSeparationFailed", file.getFileName())
                     : phase == Phase::failed     ? tr ("analyze.titleFailed", file.getFileName())
                     : phase == Phase::separating ? tr ("analyze.titleSeparating", name)
                     : phase == Phase::loaded     ? tr ("analyze.titleDone", name)
                                                  : tr ("analyze.title", name);
    g.drawText (title, r.removeFromTop (30), juce::Justification::centredLeft, true);
    // 終わったら、次に何をすればよいかを目立つ色で（［メイン画面へ進む］・Enter）
    const bool ready = phase == Phase::loaded;
    g.setColour (ready ? colours::signal : colours::textDim);
    g.setFont (sans (ready ? 13.5f : 12.5f, ready ? Weight::medium : Weight::regular));
    g.drawFittedText (tr (ready ? "analyze.subDone" : "analyze.sub"), r.removeFromTop (40), juce::Justification::topLeft, 2, 1.0f);
    if (fromOriginal)   // 分離した音の扱い（配ってよいか）を最初から一文で
    {
        g.setColour (colours::warn);
        g.setFont (sans (12.0f));
        g.drawFittedText (tr ("separation.personalUse"), r.removeFromTop (34), juce::Justification::topLeft, 2, 1.0f);
    }
    r.removeFromTop (20);

    for (size_t i = 0; i < std::size (stepKeys); ++i)
    {
        auto row = r.removeFromTop (46).toFloat();
        r.removeFromTop (6);

        // 1 行目（形式・長さ）と、原曲だけで始めた時の 2 行目（分離。B16）がこの画面で進む。テンポ・お手本の音程は「開いた後」、声域は SKIP
        const bool sepRow = i == 1 && fromOriginal;
        const bool real = i == 0 || sepRow;
        const bool sepFailed = phase == Phase::failed && separationError.isNotEmpty();
        const bool waiting = i == 0 && (phase == Phase::separating || sepFailed);   // 分離が終わってからオフボを読む
        const bool failed = sepRow ? sepFailed : (real && phase == Phase::failed && ! sepFailed);
        const bool done = sepRow ? (phase == Phase::loading || phase == Phase::loaded || (phase == Phase::failed && ! sepFailed))
                                 : (real && phase == Phase::loaded);
        const bool running = sepRow ? phase == Phase::separating : (real && phase == Phase::loading);
        const auto progress = done ? 1.0f : (running ? (sepRow ? state().separationProgress : loader.getProgress()) : 0.0f);
        const auto c = failed ? colours::bad : (done ? colours::signal : (running ? colours::warn : colours::textMute));

        paint::led (g, { row.getX() + 6.0f, row.getCentreY() }, 3.2f, c, real && ! waiting);
        row.removeFromLeft (22.0f);

        auto label = row.removeFromLeft (row.getWidth() * 0.42f);
        g.setColour (real ? colours::text : colours::textDim);
        g.setFont (sans (13.0f, Weight::medium));
        g.drawText (tr (stepKeys[i]), label, juce::Justification::centredLeft, true);

        auto status = row.removeFromRight (sepRow ? 170.0f : 120.0f);
        g.setColour (c);
        g.setFont (mono (11.0f, Weight::medium));
        const auto eta = state().separationEta;
        // 分離は「曲を読む → モデルを読む → 最初の部分を処理」まで進み具合も残り時間も届かない（数十秒）。
        // その間は止まって見えないよう、経過時間と流れる棒を表示する
        const bool preparing = sepRow && running && progress <= 0.0f && eta <= 0.0;
        const auto startedMs = state().separationStartedMs;
        const auto elapsed = startedMs > 0.0 ? (int) ((juce::Time::getMillisecondCounterHiRes() - startedMs) / 1000.0) : 0;
        // テンポ・キーは開いた後に裏で推定する（B9b）。お手本があれば、音程と分離（引き算で取れない時・リードとハモリ分け）も開いた後（B9 / B16）
        const bool withGuide = guideFile != juce::File() || guideAfterOpen;   // 分離（要る時・リードとハモリ分け）も開いた後
        const bool later = (i == 3) || (i == 2 && (withGuide || fromOriginal)) || (i == 1 && withGuide);
        const auto statusText = ! real ? tr (later ? "analyze.later" : "analyze.skip")
                              : waiting ? tr ("analyze.wait")
                              : failed ? tr ("analyze.failed")
                              : done   ? tr ("analyze.done")
                              : preparing ? tr ("analyze.preparing", juce::String (elapsed / 60) + ":" + juce::String (elapsed % 60).paddedLeft ('0', 2))
                              : sepRow && eta > 0.0 ? tr ("analyze.eta", juce::roundToInt (progress * 100.0f), juce::jmax (1, juce::roundToInt (eta / 60.0)))
                                       : juce::String (juce::roundToInt (progress * 100.0f)) + "%";
        g.drawText (statusText, status, juce::Justification::centredRight, false);

        auto bar = row.reduced (16.0f, 0.0f).withSizeKeepingCentre (row.getWidth() - 32.0f, 6.0f);
        if (! real || waiting)
        {
            paint::hline (g, std::round (bar.getCentreY()), bar.getX(), bar.getRight(), colours::line.withAlpha (0.6f));
            continue;
        }

        // 終わったら棒の代わりに結果（形式・SR・長さ）か、失敗の理由
        if (done || failed)
        {
            g.setColour (failed ? colours::bad : colours::textDim);
            const auto text = failed ? errorText() : (sepRow ? tr ("analyze.separated") : songInfoLine());
            g.setFont (failed ? sansFor (text, 12.0f) : mono (11.5f, Weight::medium));
            g.drawFittedText (text, row.reduced (16.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
            continue;
        }

        paint::inset (g, bar, 3.0f);
        if (preparing)   // 終わりの分からない間：短い棒が左右に行き来する（動きを減らす設定では、薄い棒を全体に）
        {
            g.setColour (c.withAlpha (0.55f));
            if (motion::prefersReducedMotion())
                g.fillRoundedRectangle (bar, 3.0f);
            else
            {
                const auto t = std::fmod (juce::Time::getMillisecondCounterHiRes() / 1600.0, 2.0);
                const auto pos = (float) (t < 1.0 ? t : 2.0 - t);
                const auto w = bar.getWidth() * 0.22f;
                g.fillRoundedRectangle (bar.withWidth (w).withX (bar.getX() + (bar.getWidth() - w) * pos), 3.0f);
            }
        }
        else if (progress > 0.0f)
        {
            g.setColour (c);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 3.0f);
        }
    }

    r.removeFromTop (8);
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawFittedText (tr ("analyze.laterNote") + " " + tr ("analyze.skipNote") + "\n" + tr ("analyze.note"), r.removeFromTop (40),
                      juce::Justification::topLeft, 3, 1.0f);
}
} // namespace vb
