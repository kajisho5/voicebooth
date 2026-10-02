#include "MainComponent.h"
#include "screens/StartScreen.h"
#include "screens/SetupWizard.h"
#include "screens/ExportDialog.h"
#include "screens/SettingsDialog.h"
#include "screens/SkinEditor.h"
#include "screens/SkinTemplates.h"
#include "screens/WelcomeScreen.h"
#include "screens/UpdateDialog.h"
#include "screens/SongInfoDialog.h"
#include "screens/LyricsDialog.h"
#include "SongMarks.h"

namespace vb
{
MainComponent::MainComponent (UiSession& u, AppHooks& h)
    : SessionView (u),
      hooks (h),
      top (u, actions), transport (u, actions), pitch (u, actions), lyrics (u, actions), wave (u), tracks (u), rack (u, actions), status (u)
{
    actions.toggleRecord = [this] { toggleRecord(); };
    actions.requestTempo = [this] (int v) { requestTempo (v); };
    actions.requestKey   = [this] (int v) { requestKey (v); };
    actions.openStart    = [this] { openStart(); };
    actions.openSetup    = [this] { openSetup(); };
    actions.openExport   = [this] { openExport(); };
    actions.openSettings = [this] { openSettings(); };
    actions.openSongInfo = [this] { openSongInfo(); };
    actions.openLyrics   = [this] { openLyrics(); };
    actions.editSectionName = [this] (int i) { openSectionName (i); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &top, &transport, &pitch, &lyrics, &wave, &tracks, &rack, &status })
        addAndMakeVisible (c);
    toastKey.onClick = [this]
    {
        auto action = std::move (toastAction);
        toastAction = nullptr;
        toastUntil = 0.0;
        toastLayer.setVisible (false);
        repaint();
        if (action) action();
    };
    toastLayer.setInterceptsMouseClicks (false, true);
    toastLayer.painter = [this] (juce::Graphics& g) { paintToast (g, toastLayer.getPosition().toFloat()); };
    toastLayer.addAndMakeVisible (toastKey);
    addChildComponent (toastLayer);
    addChildComponent (overlay);
    overlay.onClosed = [this]
    {
        if (isShowing()) grabKeyboardFocus();
        // 起動画面・ダイアログを閉じた後で、待っていた入力セットアップを出す（B13）
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->maybeOpenSetup(); });
    };
    status.onUpdateClicked = [this] { openUpdate(); };

    setWantsKeyboardFocus (true);
    setSize (defaultWidth, defaultHeight);

    deviceLostSeen = state().deviceLostCount;
    noticeSeen = state().noticeSerial;
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
}

void MainComponent::applyLaunchOptions (const LaunchOptions& o)
{
    if (o.mode == "easy")     session.setMode (project::Mode::easy);
    if (o.mode == "standard") session.setMode (project::Mode::standard);
    if (o.mode == "pro")      session.setMode (project::Mode::pro);

    if (o.track.isNotEmpty())
        for (int i = 0; i < (int) state().trackUi.size(); ++i)
            if (o.track == project::trackKey (state().trackUi[(size_t) i].type))
                session.selectTrack (i);

    if (o.playing)
        session.setPlaying (true);

    if (o.recording)
    {
        // 静止画用：0:35.5 から録り始めて 6 秒進んだところで時計を止める
        session.seek (state().sec (35.5));
        session.setRecording (true);
        session.tick (6.0);
        clockFrozen = true;
    }

    if (o.open != juce::File())     openSong (o.open);
    if (o.guide != juce::File())
    {
        // --open と一緒なら開いた後に重ねる。--guide だけなら起動画面のお手本の枠に入れる（原曲だけで始める。B16）
        if (o.open != juce::File())
            pendingGuide = o.guide;
        else if (auto* start = dynamic_cast<StartScreen*> (overlay.getContent()))
            start->setGuide (o.guide);
        else
            openStart()->setGuide (o.guide);
    }
    if (o.lyrics != juce::File())   openLyrics (o.lyrics);
    if (o.screen == "song-info")    openSongInfo();
    if (o.screen == "lyrics")       openLyrics();
    if (o.screen == "start")        openStart();
    if (o.screen == "setup")        openSetup (0);
    if (o.screen == "setup2")       openSetup (1);
    if (o.screen == "setup3")       openSetup (2);
    if (o.screen == "export")       openExport();
    if (o.screen == "settings")     openSettings();
    if (o.screen == "skin-templates") openSkinTemplates();
    if (o.screen == "skin-editor" || o.screen == "skin-editor-borrow")
        openSkinEditor();
    if (o.screen == "skin-editor-borrow")
    {
        // スクリーンショット用：意味の色の「テンプレート」メニューを開く（ウィンドウが出てから）
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::Timer::callAfterDelay (800, [safe]
        {
            if (safe != nullptr)
                if (auto* editor = dynamic_cast<SkinEditor*> (safe->overlay.getContent()))
                    editor->showGroupMenu (3);
        });
    }
    if (o.screen == "confirm-rec")  { session.setTempo (75); toggleRecord(); }

    // DESIGN 11.7 のモック（通信しない）
    if (o.screen.startsWith ("update"))  session.setUpdateAvailable ("0.2.0");
    if (o.screen == "update")             openUpdate();
    using Stage = ModelDownloadDialog::Stage;
    if (o.screen == "model-download")     openModelDownload ((int) Stage::confirm, true);
    if (o.screen == "model-downloading")  openModelDownload ((int) Stage::downloading, false);
    if (o.screen == "model-interrupted")  openModelDownload ((int) Stage::interrupted, true);   // 3 秒後に続きから再開
    if (o.screen == "model-done")         openModelDownload ((int) Stage::done, false);
    if (o.screen == "model-failed")       openModelDownload ((int) Stage::failed, false);
}

//==============================================================================
void MainComponent::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = juce::jlimit (0.0, 0.1, (now - lastTick) / 1000.0);
    lastTick = now;

    // 見た目の時計（音は出ない）。テンポに合わせて再生ヘッドが進む
    if (! clockFrozen)
        session.tick (dt);

    // 入力メーター（30 Hz）。時計を止めていても動かす
    session.pollInput();

    if (toastUntil > 0.0 && now > toastUntil)
    {
        toastUntil = 0.0;
        toastAction = nullptr;
        toastLayer.setVisible (false);
        repaint();
    }
}

void MainComponent::onSessionChanged (juce::uint32 changes)
{
    // 曲が開いたら、--guide= のお手本を重ねる（B9）
    if ((changes & change::song) && pendingGuide != juce::File() && state().backingWave != nullptr)
    {
        const auto guide = std::exchange (pendingGuide, juce::File());
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::MessageManager::callAsync ([safe, guide] { if (safe != nullptr) safe->session.loadGuide (guide); });
    }

    if (changes & (change::song | change::device))
        maybeOpenSetup();

    if (changes & change::mode)
        resized();   // 波形レーンの高さがモードで変わる

    if (changes & change::transport)
        repaint();   // 録音中の赤枠

    // 使っていた機器が外れた：再生は止まっている。選び直しを促す（DESIGN 13）
    if ((changes & change::device) && state().deviceLostCount != deviceLostSeen)
    {
        deviceLostSeen = state().deviceLostCount;
        showToast (tr ("device.lostToast"));
    }

    // 分離モデルのダウンロード（B16）：確認を開く・段階が変わったら開き直す
    if (changes & change::notice)
    {
        const auto& m = state().modelDl;
        using DS = models::DownloadStatus::Stage;
        using MS = ModelDownloadDialog::Stage;
        if (m.dialogSerial != modelDialogSeen)
        {
            modelDialogSeen = m.dialogSerial;
            openLiveModelDownload ((int) MS::confirm);
        }
        else if (modelStageShown != -2 && m.stage != modelStageShown)
        {
            const auto ds = m.stage;
            int next = -1;
            if (ds == (int) DS::downloading || ds == (int) DS::verifying) next = (int) MS::downloading;
            else if (ds == (int) DS::waiting || ds == (int) DS::interrupted) next = (int) MS::interrupted;
            else if (ds == (int) DS::done)   next = (int) MS::done;
            else if (ds == (int) DS::failed) next = (int) MS::failed;
            if (next >= 0 && dynamic_cast<ModelDownloadDialog*> (overlay.getContent()) != nullptr)
            {
                // 同じ見た目の段階（取得中 ↔ 照合中）は開き直さない
                const bool sameLook = next == (int) MS::downloading && (modelStageShown == (int) DS::downloading || modelStageShown == (int) DS::verifying);
                modelStageShown = ds;
                if (! sameLook) openLiveModelDownload (next);
            }
            else
                modelStageShown = ds;
        }
        else if (modelStageShown == -2 && m.stage != modelStageBehind)
        {
            // 画面を閉じても裏で続く（進み具合は状態バー）。終わったら知らせる
            modelStageBehind = m.stage;
            juce::Component::SafePointer<MainComponent> safe (this);
            if (m.stage == (int) DS::done && m.kind == 1)
                showToast (tr ("model.lyrics.readyToast"));   // 歌詞はそのまま合わせ始める（UiSession が続ける）
            else if (m.stage == (int) DS::done)
            {
                if (pendingOriginal != juce::File() && state().songOriginal == nullptr && session.separationAvailable())
                {
                    const auto original = std::exchange (pendingOriginal, juce::File());
                    showToast (tr ("model.readyToast"), tr ("start.fromOriginal"),
                               [safe, original] { if (safe != nullptr) safe->openStart()->startFromOriginal (original); });
                }
                else if (state().guideNeedsSeparation && session.separationAvailable())
                    showToast (tr ("model.readyToast"), tr ("separation.confirm.yes"),
                               [safe] { if (safe != nullptr) safe->session.offerSeparation(); });
                else
                    showToast (tr ("model.readyToast"));
            }
            else if (m.stage == (int) DS::failed)
                showToast (tr (m.kind == 1 ? "model.lyrics.failedToast" : "model.failedToast"), tr ("model.details"),
                           [safe] { if (safe != nullptr) safe->openLiveModelDownload ((int) MS::failed); });
        }
    }

    // 引き算では声が取れない：分離するか尋ねる（B16）
    if ((changes & change::notice) && state().separationOfferSerial != separationOfferSeen)
    {
        separationOfferSeen = state().separationOfferSerial;
        const auto minutes = juce::jmax (1, juce::roundToInt (session.separationEstimateSeconds() / 60.0));
        showConfirm (tr ("separation.confirm.title"), tr ("separation.confirm.message", minutes),
                     {
                         { tr ("separation.confirm.yes"), DialogPanel::KeyRole::primary, [this] { session.separateGuide(); } },
                         { tr ("separation.confirm.later"), DialogPanel::KeyRole::normal, {} },
                     });
    }

    // 録音・書き出しの結果、録れない理由（B5）
    if ((changes & change::notice) && state().noticeSerial != noticeSeen)
    {
        noticeSeen = state().noticeSerial;
        const auto& s = state();
        if (s.modelDl.noticeSerial == s.noticeSerial)
            showToast (s.noticeText, tr ("model.getButton"), [this] { session.requestSeparationModel(); });   // 押した時だけ一覧を見に行く
        else if (s.rescueNoticeSerial == s.noticeSerial)
        {
            // リハーサルで録ったテイク（原速・原キー）：本番のつもりだったらその場で入れられる
            const auto type = s.rescueTrack;
            const auto id = s.rescueTakeId;
            showToast (s.noticeText, tr ("rescue.button"), [this, type, id] { session.promoteRehearsalTake (type, id); });
        }
        else
            showToast (s.noticeText);
    }
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    top.setBounds (r.removeFromTop (TopBar::height));
    transport.setBounds (r.removeFromTop (TransportBar::height));
    status.setBounds (r.removeFromBottom (StatusBar::height));
    rack.setBounds (r.removeFromRight (metrics::rackWidth));

    // キャンバス：下から積み、残りはすべてピッチレーン（主役）
    canvasArea = r;
    tracks.setBounds (r.removeFromBottom (TrackTabs::height));
    wave.setBounds (r.removeFromBottom (WaveLane::preferredHeight (state().mode)));
    lyrics.setBounds (r.removeFromBottom (LyricsLane::height));
    pitch.setBounds (r);

    overlay.setBounds (getLocalBounds());
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
}

void MainComponent::paintOverChildren (juce::Graphics& g)
{
    // 録音中：キャンバスの縁だけを赤く（画面全体は赤くしない。DESIGN 4.9）
    if (state().isRecording && ! overlay.isShowing())
    {
        g.setColour (colours::rec);
        g.drawRect (canvasArea, 2);
    }

    if (toastUntil > 0.0 && ! toastAction)
        paintToast (g, {});
}

void MainComponent::paintToast (juce::Graphics& g, juce::Point<float> origin)
{
    // origin：描く部品の左上（MainComponent の座標）。キー付きの時は toastLayer の中に描く
    juce::Graphics::ScopedSaveState saved (g);
    g.addTransform (juce::AffineTransform::translation (-origin.x, -origin.y));
    const auto f = sans (12.5f, Weight::medium);
    const auto box = toastBox();
    g.setColour (colours::raisedHi);
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (colours::lineHi);
    g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
    paint::led (g, { box.getX() + 16.0f, box.getCentreY() }, 2.8f, colours::warn, true);
    g.setColour (colours::text);
    g.setFont (f);
    auto textArea = box.withTrimmedLeft (28.0f);
    if (toastAction) textArea.removeFromRight ((float) toastKey.idealWidth() + 14.0f);
    g.drawText (toastText, textArea, juce::Justification::centredLeft, true);
}

juce::Rectangle<float> MainComponent::toastBox() const
{
    const auto f = sans (12.5f, Weight::medium);
    const auto keyW = toastAction ? (float) toastKey.idealWidth() + 14.0f : 0.0f;
    const auto w = juce::jmin ((float) canvasArea.getWidth() - 24.0f, textWidth (f, toastText) + 48.0f + keyW);
    const auto h = toastAction ? 44.0f : 36.0f;
    return juce::Rectangle<float> (w, h).withCentre ({ (float) canvasArea.getCentreX(), (float) canvasArea.getBottom() - 90.0f });
}

void MainComponent::showToast (const juce::String& text, const juce::String& actionLabel, std::function<void()> action)
{
    toastText = text;
    toastAction = std::move (action);
    toastKey.setButtonText (actionLabel);
    const auto box = toastBox();
    const auto area = box.getSmallestIntegerContainer();
    toastLayer.setBounds (area);
    toastKey.setBounds (juce::Rectangle<int> (toastKey.idealWidth(), area.getHeight() - 14)
                            .withPosition (area.getWidth() - toastKey.idealWidth() - 8, 7));
    toastKey.setVisible (true);
    toastLayer.setVisible (true);
    toastLayer.toFront (false);
    toastLayer.repaint();
    toastUntil = juce::Time::getMillisecondCounterHiRes() + 12000.0;   // 押す時間を取る
    repaint();
}

void MainComponent::showToast (const juce::String& text)
{
    toastText = text;
    toastAction = nullptr;
    toastLayer.setVisible (false);
    // 長い知らせ（書き出し先のパスなど）は長めに出す
    toastUntil = juce::Time::getMillisecondCounterHiRes() + juce::jlimit (2600.0, 6500.0, 1400.0 + 45.0 * text.length());
    repaint();
}

//==============================================================================
bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // ダイアログ表示中は Esc で閉じる（確認ダイアログでは「キャンセル」と同じ）だけ。
    // Tab はダイアログのキーへフォーカスを移すので通す（DESIGN 4.10.1 FC）
    if (overlay.isShowing())
    {
        if (key.getKeyCode() == juce::KeyPress::tabKey)
            return false;
        if (key == juce::KeyPress::escapeKey)
            overlay.close();
        return true;
    }

    const auto c = key.getTextCharacter();
    const auto& s = state();

    // Ctrl / ⌘+Z：直前のテイクを採用から外す（B10）
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        session.undoTake();
        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        transport.playKey().flash();
        session.setPlaying (! s.isPlaying);
        return true;
    }
    if (c == 'r' || c == 'R')
    {
        transport.recKey().flash();
        toggleRecord();
        return true;
    }
    if (c == 'l' || c == 'L')
    {
        transport.loopKey().flash();
        if (s.hasRange()) session.setLoop (! s.loopOn);
        return true;
    }
    if (c == '[')
    {
        transport.rangeInKey().flash();
        session.setRangeInAtPlayhead();
        return true;
    }
    if (c == ']')
    {
        transport.rangeOutKey().flash();
        session.setRangeOutAtPlayhead();
        return true;
    }
    if (c >= '1' && c <= '4')
    {
        const auto index = (int) (c - '1');
        if (juce::isPositiveAndBelow (index, (int) s.trackUi.size()) && session.isTrackVisible (s.trackUi[(size_t) index].type))
            session.selectTrack (index);
        return true;
    }
    if (songInfoKey (key))
        return true;
    if (key == juce::KeyPress::escapeKey)
    {
        if (s.isRecording)
            confirmDiscardRecording();
        return true;
    }
    return false;
}

void MainComponent::notice (const juce::String& text)
{
    if (! state().isRecording)
        showToast (text);
}

void MainComponent::tapTempo()
{
    const auto bpm = session.tapTempo (juce::Time::getMillisecondCounterHiRes() / 1000.0);
    const auto n = session.tapCount();
    notice (bpm > 0.0 ? tr ("songInfo.tap.result", song::formatBpm (bpm), n)
                      : tr ("songInfo.tap.count", n, song::TapTempo::minTaps));
}

bool MainComponent::songInfoKey (const juce::KeyPress& key)
{
    const auto& s = state();
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    const auto code = key.getKeyCode();

    // T：タップテンポ（4 回以上。DESIGN 7.5.1）
    if (c == 't')
    {
        tapTempo();
        return true;
    }

    // M：今の位置に区間の頭（小節線に吸い付く。Alt / Option で吸い付かない。DESIGN 7.5.2）
    if (c == 'm')
    {
        const auto index = session.addSectionAtPlayhead (! key.getModifiers().isAltDown());
        notice (tr ("section.added", marks::sectionName (state().project.sections, index)));
        return true;
    }

    // 歌詞：タップで合わせる（Enter で行の歌い出し、Backspace で 1 行戻す、Esc で終わる。DESIGN 7.5.3）
    if (code == juce::KeyPress::returnKey && ! s.project.lyrics.empty())
    {
        if (! s.lyricSyncing)
        {
            session.setLyricSyncing (true);
            notice (tr ("lyrics.sync.started"));
        }
        else if (! session.tapLyric())
            notice (tr ("lyrics.sync.done"));
        return true;
    }
    if (s.lyricSyncing && code == juce::KeyPress::backspaceKey)
    {
        session.undoLyricTap();
        return true;
    }
    if (s.lyricSyncing && code == juce::KeyPress::escapeKey && ! s.isRecording)
    {
        session.setLyricSyncing (false);
        return true;
    }

    // ↑ ↓：今の行を手で送る（時刻の無い歌詞・合わせている途中）
    if ((code == juce::KeyPress::upKey || code == juce::KeyPress::downKey) && ! s.project.lyrics.empty())
    {
        session.stepLyric (code == juce::KeyPress::upKey ? -1 : 1);
        return true;
    }

    // Delete / Backspace：選んだ区間を消す
    if ((code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) && s.selectedSection >= 0)
    {
        session.removeSection (s.selectedSection);
        return true;
    }

    // Esc：区間の選択を外す（録音中は従来どおり破棄の確認）
    if (code == juce::KeyPress::escapeKey && ! s.isRecording && s.selectedSection >= 0)
    {
        session.selectSection (-1);
        return true;
    }
    return false;
}

//==============================================================================
void MainComponent::showConfirm (const juce::String& title, const juce::String& message, std::vector<ConfirmDialog::Option> options)
{
    // 選んだら閉じる
    for (auto& o : options)
    {
        auto action = o.action;
        o.action = [this, action] { overlay.close(); if (action) action(); };
    }

    auto dlg = std::make_unique<ConfirmDialog> (title, message, std::move (options));
    dlg->onCloseRequest = [this] { overlay.close(); };
    overlay.show (std::move (dlg), false);
}

void MainComponent::toggleRecord()
{
    const auto& s = state();

    if (s.isRecording)
    {
        session.setRecording (false);
        return;
    }

    // 納品録音は原速・原キー。ずれていれば選んでもらう（DESIGN 4.7 / 6.1）
    if (s.recMode == project::RecMode::delivery && (s.tempoPercent != 100 || s.keyShift != 0))
    {
        showConfirm (tr ("confirm.deliveryTempo.title"),
                     tr ("confirm.deliveryTempo.message", s.tempoPercent, (s.keyShift > 0 ? "+" : "") + juce::String (s.keyShift)),
                     {
                         { tr ("confirm.deliveryTempo.reset"), DialogPanel::KeyRole::danger,
                           [this] { session.setTempo (100); session.setKey (0); session.setRecording (true); } },
                         { tr ("confirm.deliveryTempo.practice"), DialogPanel::KeyRole::normal,
                           [this] { session.setRecMode (project::RecMode::practice); session.setRecording (true); } },
                         { tr ("common.cancel"), DialogPanel::KeyRole::normal, {} },
                     });
        return;
    }

    session.setRecording (true);
}

void MainComponent::requestTempo (int v)
{
    if (! session.deliveryLocked()) { session.setTempo (v); return; }

    // 納品 REC 中の変更は「練習録音に切り替えるか」確認（DESIGN 4.7）
    session.setTempo (100);   // いったん戻す（エンコーダー表示も戻る）
    showConfirm (tr ("confirm.switchPractice.title"), tr ("confirm.switchPractice.message"),
                 {
                     { tr ("confirm.switchPractice.yes"), DialogPanel::KeyRole::primary,
                       [this, v] { session.setRecording (false); session.setRecMode (project::RecMode::practice); session.setTempo (v); } },
                     { tr ("common.cancel"), DialogPanel::KeyRole::normal, {} },
                 });
}

void MainComponent::requestKey (int v)
{
    if (! session.deliveryLocked()) { session.setKey (v); return; }

    session.setKey (0);
    showConfirm (tr ("confirm.switchPractice.title"), tr ("confirm.switchPractice.message"),
                 {
                     { tr ("confirm.switchPractice.yes"), DialogPanel::KeyRole::primary,
                       [this, v] { session.setRecording (false); session.setRecMode (project::RecMode::practice); session.setKey (v); } },
                     { tr ("common.cancel"), DialogPanel::KeyRole::normal, {} },
                 });
}

void MainComponent::confirmDiscardRecording()
{
    showConfirm (tr ("confirm.discard.title"), tr ("confirm.discard.message"),
                 {
                     { tr ("confirm.discard.yes"), DialogPanel::KeyRole::danger, [this] { session.setRecording (false); } },
                     { tr ("confirm.discard.no"), DialogPanel::KeyRole::normal, {} },
                 });
}

//==============================================================================
void MainComponent::openWelcome()
{
    auto screen = std::make_unique<WelcomeScreen>();
    screen->onLanguage = [this] (i18n::Language l) { if (hooks.changeLanguage) hooks.changeLanguage (l, ReopenScreen::welcome); };
    screen->onContinue = [this]
    {
        overlay.close();
        if (hooks.firstRunDone) hooks.firstRunDone();
        // 続けて初回だけのモードの質問（起動画面）
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openStart (true); });
    };
    overlay.show (std::move (screen), false);
}

StartScreen* MainComponent::openStart (bool firstRun)
{
    auto screen = std::make_unique<StartScreen> (session, songLoader, firstRun);
    auto* raw = screen.get();
    screen->onDone = [this] { overlay.close(); };
    juce::Component::SafePointer<MainComponent> safe (this);
    screen->onNeedModel = [safe] (const juce::File& original)
    {
        // 起動画面を閉じてモデルの確認へ（押した時だけ。11.7）。入ったら起動画面に戻って続ける
        juce::MessageManager::callAsync ([safe, original]
        {
            if (safe == nullptr) return;
            safe->pendingOriginal = original;
            safe->overlay.close();
            safe->session.requestSeparationModel();
        });
    };
    overlay.show (std::move (screen), false);
    return raw;
}

void MainComponent::openSong (const juce::File& f)
{
    openStart()->openFile (f);
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    // 録音中は受けない。ダイアログ表示中は歌詞パッドへの歌詞のファイルだけ（起動画面は自分で受ける）
    if (files.isEmpty() || state().isRecording)
        return false;
    if (overlay.isShowing())
        return dynamic_cast<LyricsDialog*> (overlay.getContent()) != nullptr && LyricsDialog::isLyricsFile (juce::File (files[0]));
    return true;
}

void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    const juce::File f (files[0]);

    // 曲を開いている時にピッチレーンへ落とした曲は、お手本（声入りの原曲。B9）
    if (state().backingWave != nullptr && getLocalArea (&pitch, pitch.getLocalBounds()).contains (x, y) && audio::hasSongExtension (f))
    {
        session.loadGuide (f);
        return;
    }

    // .txt / .lrc は歌詞（DESIGN 7.5.3：ウィンドウへのドロップ）
    if (LyricsDialog::isLyricsFile (f))
    {
        if (auto* open = dynamic_cast<LyricsDialog*> (overlay.getContent()))
            open->loadFile (f);
        else
            openLyrics (f);
        return;
    }
    openSong (f);
}

void MainComponent::maybeOpenSetup()
{
    // 入力セットアップは初回と、入力の機器を替えた時（DESIGN 5）。曲を開いている時だけ（デモ・UI_MOCK では出さない）。
    // 録音中・再生中・ほかのダイアログを出している時は待つ（次の変化で見直す）。開いたら済みにする（閉じても同じ機器では出さない）
    const auto& s = state();
    if (! hooks.setupDoneFor || ! hooks.setSetupDoneFor || s.backingWave == nullptr || ! s.input.open
        || s.input.deviceName.isEmpty() || s.isRecording || s.isPlaying || overlay.isShowing())
        return;
    const auto key = s.input.typeName + "|" + s.input.deviceName;
    if (key == hooks.setupDoneFor())
        return;
    hooks.setSetupDoneFor (key);
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr && ! safe->overlay.isShowing()) safe->openSetup (0); });
}

void MainComponent::openSetup (int step)
{
    auto dlg = std::make_unique<SetupWizard> (session, step);
    dlg->onCloseRequest = [this] { overlay.close(); };
    overlay.show (std::move (dlg), true);
}

void MainComponent::openExport()
{
    auto dlg = std::make_unique<ExportDialog> (session);
    auto* d = dlg.get();
    dlg->onCloseRequest = [this] { overlay.close(); };
    dlg->onExport = [this, d]
    {
        // 曲を開いていれば本当に書き出す（B5：個別のフル尺 Dry）。見本（UI_MOCK・デモ）は書かない
        const auto chosen = d->selectedTracks();
        const auto bits = d->selectedBitDepth();
        const bool pack = d->packSelected();
        const bool refmix = d->refmixSelected();
        const bool real = state().backingWave != nullptr;
        overlay.close();   // d はここで消える
        if (real && pack) session.exportPack (chosen, bits, refmix);
        else if (real)    session.exportTracks (chosen, bits);
        else      showToast (tr ("export.mockToast"));
    };
    overlay.show (std::move (dlg), true);
}

void MainComponent::openSettings()
{
    std::vector<skin::Skin> skins = hooks.skins != nullptr ? hooks.skins->all() : skin::builtInSkins();
    auto dlg = std::make_unique<SettingsDialog> (session, std::move (skins), hooks.currentSkin ? hooks.currentSkin() : juce::String ("booth"));
    dlg->onCloseRequest = [this] { overlay.close(); };
    // 入れ子のラムダで this を初期化キャプチャすると MSVC が外側のラムダと解釈するため、先に作っておく
    juce::Component::SafePointer<MainComponent> safe (this);
    dlg->onOpenSetup = [this, safe]
    {
        overlay.close();
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openSetup(); });
    };
    dlg->onLanguage = [this] (i18n::Language l) { if (hooks.changeLanguage) hooks.changeLanguage (l, ReopenScreen::settings); };
    dlg->onSkin = [this] (const juce::String& id) { if (hooks.changeSkin) hooks.changeSkin (id, ReopenScreen::settings); };
    dlg->onEditSkin = [this, safe]
    {
        overlay.close();
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openSkinEditor(); });
    };
    dlg->onNewSkin = [this, safe]
    {
        overlay.close();
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openSkinTemplates(); });
    };
    overlay.show (std::move (dlg), true);
}

void MainComponent::openSkinTemplates()
{
    // 録音中はスキンを変えない（DESIGN 4.11）
    if (hooks.skins == nullptr || state().isRecording)
        return;

    auto dlg = std::make_unique<SkinTemplates> (hooks.skins->all(), hooks.currentSkin ? hooks.currentSkin() : juce::String ("booth"));
    juce::Component::SafePointer<MainComponent> safe (this);
    dlg->onChosen = [this, safe] (const skin::Skin& chosen)
    {
        overlay.close();
        juce::MessageManager::callAsync ([safe, chosen] { if (safe != nullptr) safe->openSkinEditor (&chosen); });
    };
    dlg->onCloseRequest = [this, safe]
    {
        overlay.close();
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openSettings(); });
    };
    overlay.show (std::move (dlg), true);
}

void MainComponent::openSkinEditor (const skin::Skin* fromTemplate)
{
    // 録音中はスキンを変えない（DESIGN 4.11）
    if (hooks.skins == nullptr || state().isRecording)
        return;

    const auto id = hooks.currentSkin ? hooks.currentSkin() : juce::String ("booth");
    const auto* found = hooks.skins->find (id);
    const auto active = found != nullptr ? *found : skin::defaultSkin();

    // テンプレートを選んだ時はそのコピー、そうでなければいまのスキン（内蔵ならコピー）
    auto dlg = fromTemplate != nullptr ? std::make_unique<SkinEditor> (*hooks.skins, active, *fromTemplate, true)
                                       : std::make_unique<SkinEditor> (*hooks.skins, active, active, false);

    // エディタはこの画面より長く生きることがある（終了時）ので、this ではなく AppHooks を捕まえる
    auto* h = &hooks;
    dlg->onPreview = [h] (const skin::Skin& s) { if (h->previewSkin) h->previewSkin (s); };
    dlg->onSaved   = [h] (const juce::String& saved) { if (h->changeSkin) h->changeSkin (saved, ReopenScreen::settings); };
    dlg->onDeleted = [h, id] (const juce::String& deleted)
    {
        // 使っていたスキンを消したら既定に戻す
        if (h->changeSkin) h->changeSkin (deleted == id ? juce::String ("booth") : id, ReopenScreen::settings);
    };

    // キャンセル・閉じる：設定に戻る（色はエディタが消える時に元に戻す）
    juce::Component::SafePointer<MainComponent> safe (this);
    dlg->onCloseRequest = [this, safe]
    {
        overlay.close();
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openSettings(); });
    };
    auto* editor = dlg.get();
    overlay.show (std::move (dlg), false, OverlayHost::Placement::side);
    editor->applyPreview();   // テンプレートの色をすぐ後ろの画面に出す
}

void MainComponent::openSongInfo()
{
    auto dlg = std::make_unique<SongInfoDialog> (session, actions);
    dlg->onCloseRequest = [this] { overlay.close(); };
    // 右に出す（背景を暗くしない：ルーラー・BAR.BEAT が変わるのを見ながら直す）
    overlay.show (std::move (dlg), true, OverlayHost::Placement::side);
}

void MainComponent::openLyrics (const juce::File& file)
{
    auto dlg = std::make_unique<LyricsDialog> (session);
    auto* raw = dlg.get();
    dlg->onCloseRequest = [this] { overlay.close(); };
    dlg->onApplied = [this] (const juce::String& msg) { overlay.close(); notice (msg); };
    overlay.show (std::move (dlg), false);
    if (file != juce::File())
        raw->loadFile (file);
}

void MainComponent::openSectionName (int index)
{
    // 曲の情報パネルから開いた時は、名前を入れたらパネルに戻る
    const bool fromPanel = dynamic_cast<SongInfoDialog*> (overlay.getContent()) != nullptr;
    auto dlg = std::make_unique<SectionNameDialog> (session, index);
    juce::Component::SafePointer<MainComponent> safe (this);
    auto done = [this, safe, fromPanel]
    {
        overlay.close();
        if (fromPanel)
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openSongInfo(); });
    };
    dlg->onDone = done;
    dlg->onCloseRequest = done;
    overlay.show (std::move (dlg), true);
}

void MainComponent::openUpdate()
{
    auto dlg = std::make_unique<UpdateDialog>();
    dlg->onCloseRequest = [this] { overlay.close(); };
    dlg->onInstall = [this] { overlay.close(); showToast (tr ("update.title")); };   // モック：何もしない
    dlg->onSkip = [this] { overlay.close(); session.setUpdateAvailable ({}); };
    overlay.show (std::move (dlg), true);
}

void MainComponent::openLiveModelDownload (int stage)
{
    auto dlg = std::make_unique<ModelDownloadDialog> ((ModelDownloadDialog::Stage) stage, session);
    modelStageShown = state().modelDl.stage;
    juce::Component::SafePointer<MainComponent> safe (this);
    const bool done = stage == (int) ModelDownloadDialog::Stage::done;
    dlg->onCloseRequest = [this, safe, done]
    {
        modelStageShown = -2;
        modelStageBehind = state().modelDl.stage;
        overlay.close();
        // 原曲だけで始めようとしていた（曲はまだ無い）：起動画面に戻る。入っていればそのまま分離へ
        if (state().modelDl.kind == 0 && pendingOriginal != juce::File() && state().songOriginal == nullptr)
        {
            const auto original = pendingOriginal;
            if (done) pendingOriginal = juce::File();
            juce::MessageManager::callAsync ([safe, original, done]
            {
                if (safe == nullptr) return;
                auto* start = safe->openStart();
                if (done) start->startFromOriginal (original);
                else      start->setGuide (original);
            });
            return;
        }
        // 入ったら、待っていた分離を勧める（お手本の原曲が引き算で取れなかった時）
        if (done && state().modelDl.kind == 0 && state().guideNeedsSeparation && session.separationAvailable())
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->session.offerSeparation(); });
    };
    overlay.show (std::move (dlg), false);
}

void MainComponent::openModelDownload (int stage, bool animate, float from)
{
    auto dlg = std::make_unique<ModelDownloadDialog> ((ModelDownloadDialog::Stage) stage, animate, from);
    dlg->onCloseRequest = [this] { overlay.close(); };
    juce::Component::SafePointer<MainComponent> safe (this);
    dlg->onStage = [safe] (ModelDownloadDialog::Stage next, float progress)
    {
        // 開き直す（呼び出し元のダイアログを消すので次のメッセージで）。届いた分から続ける
        juce::MessageManager::callAsync ([safe, next, progress]
        {
            if (safe != nullptr)
                safe->openModelDownload ((int) next, true, next == ModelDownloadDialog::Stage::downloading ? progress : -1.0f);
        });
    };
    overlay.show (std::move (dlg), false);
}
} // namespace vb
