#include "MainComponent.h"
#include "screens/StartScreen.h"
#include "screens/SetupWizard.h"
#include "screens/ExportDialog.h"
#include "screens/SettingsDialog.h"
#include "screens/WelcomeScreen.h"
#include "screens/UpdateDialog.h"

namespace vb
{
MainComponent::MainComponent (UiSession& u, AppHooks& h)
    : SessionView (u),
      hooks (h),
      top (u, actions), transport (u, actions), pitch (u), lyrics (u), wave (u), tracks (u), rack (u, actions), status (u)
{
    actions.toggleRecord = [this] { toggleRecord(); };
    actions.requestTempo = [this] (int v) { requestTempo (v); };
    actions.requestKey   = [this] (int v) { requestKey (v); };
    actions.openStart    = [this] { openStart(); };
    actions.openSetup    = [this] { openSetup(); };
    actions.openExport   = [this] { openExport(); };
    actions.openSettings = [this] { openSettings(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &top, &transport, &pitch, &lyrics, &wave, &tracks, &rack, &status })
        addAndMakeVisible (c);
    addChildComponent (overlay);
    overlay.onClosed = [this] { if (isShowing()) grabKeyboardFocus(); };
    status.onUpdateClicked = [this] { openUpdate(); };

    setWantsKeyboardFocus (true);
    setSize (defaultWidth, defaultHeight);

    deviceLostSeen = state().deviceLostCount;
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
    if (o.screen == "start")        openStart();
    if (o.screen == "setup")        openSetup (0);
    if (o.screen == "setup2")       openSetup (1);
    if (o.screen == "setup3")       openSetup (2);
    if (o.screen == "export")       openExport();
    if (o.screen == "settings")     openSettings();
    if (o.screen == "confirm-rec")  { session.setTempo (75); toggleRecord(); }

    // DESIGN 11.7 のモック（通信しない）
    if (o.screen.startsWith ("update"))  session.setUpdateAvailable ("0.2.0");
    if (o.screen == "update")             openUpdate();
    using Stage = ModelDownloadDialog::Stage;
    if (o.screen == "model-download")     openModelDownload ((int) Stage::confirm, true);
    if (o.screen == "model-downloading")  openModelDownload ((int) Stage::downloading, false);
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
        repaint();
    }
}

void MainComponent::onSessionChanged (juce::uint32 changes)
{
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

    if (toastUntil > 0.0)
    {
        const auto f = sans (12.5f, Weight::medium);
        const auto w = textWidth (f, toastText) + 48.0f;
        const auto box = juce::Rectangle<float> (w, 36.0f).withCentre ({ (float) canvasArea.getCentreX(), (float) canvasArea.getBottom() - 90.0f });
        g.setColour (colours::raisedHi);
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (colours::lineHi);
        g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
        paint::led (g, { box.getX() + 16.0f, box.getCentreY() }, 2.8f, colours::warn, true);
        g.setColour (colours::text);
        g.setFont (f);
        g.drawText (toastText, box.withTrimmedLeft (28.0f), juce::Justification::centredLeft, false);
    }
}

void MainComponent::showToast (const juce::String& text)
{
    toastText = text;
    toastUntil = juce::Time::getMillisecondCounterHiRes() + 2600.0;
    repaint();
}

//==============================================================================
bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // ダイアログ表示中は Esc で閉じる（確認ダイアログでは「キャンセル」と同じ）だけ
    if (overlay.isShowing())
    {
        if (key == juce::KeyPress::escapeKey)
            overlay.close();
        return true;
    }

    const auto c = key.getTextCharacter();
    const auto& s = state();

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
    if (key == juce::KeyPress::escapeKey)
    {
        if (s.isRecording)
            confirmDiscardRecording();
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
                       [this, v] { session.setRecMode (project::RecMode::practice); session.setTempo (v); } },
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
                       [this, v] { session.setRecMode (project::RecMode::practice); session.setKey (v); } },
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
    overlay.show (std::move (screen), false);
    return raw;
}

void MainComponent::openSong (const juce::File& f)
{
    openStart()->openFile (f);
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    // ダイアログ表示中・録音中は受けない（起動画面は自分で受ける）
    return files.size() > 0 && ! overlay.isShowing() && ! state().isRecording;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    openSong (juce::File (files[0]));
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
    dlg->onCloseRequest = [this] { overlay.close(); };
    dlg->onExport = [this] { overlay.close(); showToast (tr ("export.mockToast")); };
    overlay.show (std::move (dlg), true);
}

void MainComponent::openSettings()
{
    auto dlg = std::make_unique<SettingsDialog> (session);
    dlg->onCloseRequest = [this] { overlay.close(); };
    // 入れ子のラムダで this を初期化キャプチャすると MSVC が外側のラムダと解釈するため、先に作っておく
    juce::Component::SafePointer<MainComponent> safe (this);
    dlg->onOpenSetup = [this, safe]
    {
        overlay.close();
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openSetup(); });
    };
    dlg->onLanguage = [this] (i18n::Language l) { if (hooks.changeLanguage) hooks.changeLanguage (l, ReopenScreen::settings); };
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

void MainComponent::openModelDownload (int stage, bool animate)
{
    auto dlg = std::make_unique<ModelDownloadDialog> ((ModelDownloadDialog::Stage) stage, animate);
    dlg->onCloseRequest = [this] { overlay.close(); };
    juce::Component::SafePointer<MainComponent> safe (this);
    dlg->onStage = [safe] (ModelDownloadDialog::Stage next)
    {
        // 開き直す（呼び出し元のダイアログを消すので次のメッセージで）
        juce::MessageManager::callAsync ([safe, next] { if (safe != nullptr) safe->openModelDownload ((int) next, true); });
    };
    overlay.show (std::move (dlg), false);
}
} // namespace vb
