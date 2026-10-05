#include "ui/MainComponent.h"
#include "ui/Gallery.h"
#include "ui/VoiceBoothLookAndFeel.h"

#if ! VOICEBOOTH_UI_MOCK
 #include "audio/PlaybackEngine.h"
#endif

/*  起動オプション（開発・スクリーンショット用）
      --gallery              部品ギャラリー
      --lang=ja|en|ko|zh-Hans|zh-Hant|es|pt-BR|id|vi|tr|de|fr  表示言語（保存された設定より優先）
      --skin=<id>            スキン（booth / studio-day / … / 自作の id。保存された設定より優先、保存はしない）
      --screen=<name>        start / setup / setup2 / setup3 / export / settings / skin-templates / skin-editor /
                             skin-editor-borrow / confirm-rec / song-info / lyrics / compare（テイク比較。B18c）
      --open=<path>          その曲を開く（起動画面で読み込み → 波形。B1）
      --lyrics=<path>        歌詞パッドをその .txt / .lrc で開く（B4b）
      --guide=<path>         --open の曲を開いたら、このお手本（声入りの原曲）を重ねる（B9）。--open が無ければ起動画面のお手本の枠に入れる
      --mode=easy|standard|pro
      --track=main|double|harm1
      --rec                  録音中の見た目で開く
      --play                 再生中で開く（再生ヘッドが動く）
      --first-run            初回起動の流れ（言語選択 → モードの質問）を必ず出す
      --no-first-run         初回起動の流れを出さない
      --reduce-motion        動きを減らす（OS の設定に関係なく。ばね・明滅・揺れを止めて最終状態だけ。DESIGN 4.10）
      --motion               動きを出す（OS で動きを減らす設定でも。確認用）
      --no-update-check      起動時に新しいバージョンを確かめない（--screen= を付けた時も確かめない。スクリーンショット用）

    アプリ設定（PropertiesFile）に保存するもの
      language      表示言語（初回に選び、以後は設定から変更）
      mode          最後に使ったモード（簡単 / 標準 / プロ）
      firstRunDone  初回の言語選択を終えたか
      skin          スキンの id（DESIGN 4.11。無い・消えた時は booth）。自作スキンは同じフォルダの Skins/ に .vbskin で置く
      recordRate / recordFloat  録音形式（SR・32bit float）
      recentProjects  最近のプロジェクト（.vbooth のフルパス。1 行に 1 つ、新しい順。B14）
      latencyProfiles  往復の遅れ（B6）。機器の組み合わせ（ドライバ|入力|出力|SR|バッファ）ごとの実測（サンプル）と手入力（ms）。JSON
      audioDevice   オーディオデバイスの設定（AudioDeviceManager の XML。ドライバ・入出力の機器・入力チャンネル・SR・バッファ）。
                    戻せなければ既定のデバイスで開く
      updateAutoCheck / updateBetas  起動時に新しいバージョンを確かめるか（既定は入）・ベータも知らせるか（DESIGN 11.7）
      updateLastCheck  最後に確かめられた時刻（ms。24 時間に 1 回まで）
      updateSkipped    「このバージョンを飛ばす」で飛ばしたバージョン
      updateFound      見つけたバージョン（JSON。次の起動でも知らせを出す）
      pitchToleranceCents / octaveAlign / fullRange  判定の幅（セント）・オクターブ合わせ・音域を全部表示
      cacheFolder   アプリ共通のキャッシュの場所（空 = 既定のアプリのデータ/VoiceBooth/Cache。DESIGN 8） */

namespace vb
{
namespace
{
    juce::String argValue (const juce::StringArray& args, const juce::String& name)
    {
        for (auto& a : args)
            if (a.startsWith ("--" + name + "="))
                return a.fromFirstOccurrenceOf ("=", false, false);
        return {};
    }

    const char* modeKey (project::Mode m)
    {
        switch (m)
        {
            case project::Mode::easy:     return "easy";
            case project::Mode::standard: return "standard";
            case project::Mode::pro:      return "pro";
        }
        return "standard";
    }
}

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (UiSession& s, AppHooks& h)
        : DocumentWindow ("VoiceBooth", colours::bg0, DocumentWindow::allButtons),
          session (s), hooks (h)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setResizeLimits (MainComponent::minWidth, MainComponent::minHeight, 4096, 2160);
        rebuild();
        // 1920x1080 の画面を基準にする：作業領域（タスクバー・Dock を除く）いっぱいに近い大きさで開く。
        // 画面より大きくは開かない（拡大 125 % 以上や小さい画面）。窓枠・タイトルバーの分を残す
        auto width = MainComponent::defaultWidth, height = MainComponent::defaultHeight;
        if (const auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            width = juce::jmin (MainComponent::preferredWidth, d->userArea.getWidth() - 16);
            height = juce::jmin (MainComponent::preferredHeight, d->userArea.getHeight() - 48);
        }
        centreWithSize (width, height);   // 最小（minWidth / minHeight）より小さくはならない
        setVisible (true);
        focusContent();
    }

    /** 言語を変えた時は中身を作り直す（全部品の文言を確実に入れ替える） */
    void rebuild()
    {
        auto* content = new MainComponent (session, hooks);
        updateTitle();
        setContentOwned (content, false);
        focusContent();
    }

    void updateTitle()
    {
        setName (tr ("app.name") + juce::String::fromUTF8 (" \xe2\x80\x94 ") + session->songName);
    }

    /** ショートカットを受けられるようにする（表示されてから） */
    void focusContent()
    {
        if (auto* c = getContentComponent(); c != nullptr && c->isShowing())
            c->grabKeyboardFocus();
    }

    MainComponent* main() { return dynamic_cast<MainComponent*> (getContentComponent()); }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    UiSession& session;
    AppHooks& hooks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

class GalleryWindow : public juce::DocumentWindow
{
public:
    GalleryWindow() : DocumentWindow ("VoiceBooth", colours::bg0, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setName (tr ("gallery.title"));
        setContentOwned (new Gallery(), true);
        setResizable (true, true);
        centreWithSize (MainComponent::defaultWidth, MainComponent::defaultHeight);
        setVisible (true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class VoiceBoothApplication : public juce::JUCEApplication, private UiSession::Listener
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    /** 起動中に .vbooth / 曲をダブルクリックした：いまのウィンドウで開く（B14） */
    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        for (auto& a : juce::StringArray::fromTokens (commandLine, true))
        {
            const juce::File f (a.unquoted());
            if (! a.startsWith ("-") && f.existsAsFile())
                if (window != nullptr)
                    if (auto* m = window->main())
                    {
                        m->openSong (f);
                        return;
                    }
        }
    }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        // 文字の大きさ：1920x1080 の画面で満額。小さい画面・拡大 125 % 以上ではほとんど大きくしない（部品が収まるように）
        if (const auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            setTextBoost (textBoostFor (d->userArea.getHeight()));

        juce::PropertiesFile::Options opts;
        opts.applicationName = "VoiceBooth";
        opts.filenameSuffix = ".settings";
       #if JUCE_LINUX
        opts.folderName = ".config/VoiceBooth";     // 開発用（配布対象外）
       #else
        opts.folderName = "VoiceBooth";             // Win: %APPDATA%  Mac: ~/Library/Application Support
       #endif
        opts.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters (opts);
        auto* stored = properties.getUserSettings();

        juce::ignoreUnused (commandLine);
        const auto args = getCommandLineParameterArray();   // 空白を含むパスも 1 つの引数のまま

        // 言語：起動オプション > 保存した設定 > OS の表示言語
        auto lang = i18n::fromSystem();
        lang = i18n::fromCode (stored->getValue ("language"), lang);
        lang = i18n::fromCode (argValue (args, "lang"), lang);
        i18n::setLanguage (lang);

        // スキン：起動オプション > 保存した設定 > booth（見つからなければ booth。DESIGN 4.11）
        skins = std::make_unique<skin::Library> (stored->getFile().getParentDirectory().getChildFile ("Skins"));
        {
            auto id = stored->getValue ("skin", skin::defaultSkin().id);
            if (const auto opt = argValue (args, "skin"); opt.isNotEmpty())
                id = opt;
            applySkinById (id);
        }

        // 動きを減らす（DESIGN 4.10）：起動オプション > OS の設定
        if (args.contains ("--reduce-motion")) motion::setReducedMotionOverride (true);
        if (args.contains ("--motion"))        motion::setReducedMotionOverride (false);

        if (args.contains ("--gallery"))
        {
            gallery = std::make_unique<GalleryWindow>();
            return;
        }

        hooks.changeLanguage = [this] (i18n::Language l, ReopenScreen reopen) { changeLanguage (l, reopen); };
        hooks.changeSkin = [this] (const juce::String& id, ReopenScreen reopen) { changeSkin (id, reopen); };
        hooks.previewSkin = [this] (const skin::Skin& s) { vb::applySkin (s); refreshSkinChrome(); };
        hooks.currentSkin = [this] { return skinId; };
        hooks.skins = skins.get();
        hooks.firstRunDone = [this]
        {
            settings()->setValue ("language", i18n::codeOf (i18n::current()));
            settings()->setValue ("firstRunDone", true);
            settings()->saveIfNeeded();
        };

        hooks.setupDoneFor = [this] { return settings()->getValue ("setupDoneFor"); };
        hooks.setSetupDoneFor = [this] (const juce::String& key)
        {
            settings()->setValue ("setupDoneFor", key);
            settings()->saveIfNeeded();
        };

        session = std::make_unique<UiSession>();

       #if ! VOICEBOOTH_UI_MOCK
        // B2 / B3：前回のデバイス（無ければ既定）を開く。入力は 1 ch でメーターだけ。開けなくても画面は出す（ステータスバーに理由）
        engine = std::make_unique<audio::PlaybackEngine>();
        {
            const auto saved = stored->getXmlValue ("audioDevice");
            if (const auto err = engine->openDevices (saved.get()); err.isNotEmpty())
            {
                DBG ("Audio device: " << err);
            }
        }
        session->attachEngine (engine.get());
       #endif

        // 録音形式（SR：0 = 曲に合わせる、ビット数：24 / 32bit float）
        session->setRecordFormat (stored->getDoubleValue ("recordRate", 0.0), stored->getBoolValue ("recordFloat", false));
        session->restoreLatencyProfiles (stored->getValue ("latencyProfiles"));
        session->restoreRecentProjects (juce::StringArray::fromLines (stored->getValue ("recentProjects")));

        // 最後に使ったモード
        const auto savedMode = stored->getValue ("mode");
        if (savedMode == "easy")     session->setMode (project::Mode::easy);
        if (savedMode == "standard") session->setMode (project::Mode::standard);
        if (savedMode == "pro")      session->setMode (project::Mode::pro);
        session->setShowLyrics (stored->getBoolValue ("showLyrics", false));
        session->setCrossfade (stored->getDoubleValue ("crossfadeMs", 8.0));
        // 判定の幅・オクターブ合わせ（設定）と音域の表示（2026-10-04：保存していなかった）
        session->setPitchTolerance ((float) stored->getDoubleValue ("pitchToleranceCents", 30.0));
        session->setOctaveAlign (stored->getBoolValue ("octaveAlign", true));
        session->restoreShortcuts (stored->getValue ("shortcuts"));   // 1 文字のショートカット（#28）
        session->setFullRange (stored->getBoolValue ("fullRange", false));
        // クリック・カウントイン（2026-10-02）。クリックの入り切りは、曲を開いてテンポが分かってから効く（setClick はテンポを見るので、値だけ戻す）
        session->setCountIn (stored->getIntValue ("countInBars", 1));
        session->setClickLevel ((float) stored->getDoubleValue ("clickLevel", 0.62));
        session->restoreClickOn (stored->getBoolValue ("clickOn", false));
        session->setVoiceRange (stored->getIntValue ("voiceLow", -1), stored->getIntValue ("voiceHigh", -1));   // 声域（おすすめのキー）   // 歌詞レーン（既定は出さない）
        {
            // 新しいバージョンの確認（DESIGN 11.7）とアプリ共通のキャッシュの場所（DESIGN 8）
            const auto cache = stored->getValue ("cacheFolder");
            session->restoreAppPrefs (stored->getBoolValue ("updateAutoCheck", true), stored->getBoolValue ("updateBetas", false),
                                      stored->getValue ("updateSkipped"), stored->getValue ("updateLastCheck").getLargeIntValue(),
                                      stored->getValue ("updateFound"),
                                      juce::File::isAbsolutePath (cache) ? juce::File (cache) : juce::File());
        }
        session->addListener (this);

        window = std::make_unique<MainWindow> (*session, hooks);

        LaunchOptions o;
        o.screen = argValue (args, "screen");
        o.mode = argValue (args, "mode");
        o.track = argValue (args, "track");
        o.recording = args.contains ("--rec");
        o.playing = args.contains ("--play");
        if (const auto path = argValue (args, "open"); path.isNotEmpty())
            o.open = juce::File::getCurrentWorkingDirectory().getChildFile (path);
        for (auto& a : args)   // .vbooth をダブルクリック：パスだけが渡される（B14）
            if (! a.startsWith ("-") && a.unquoted().endsWithIgnoreCase (project::fileExtension))
                o.open = juce::File::getCurrentWorkingDirectory().getChildFile (a.unquoted());
        if (const auto path = argValue (args, "lyrics"); path.isNotEmpty())
            o.lyrics = juce::File::getCurrentWorkingDirectory().getChildFile (path);
        if (const auto path = argValue (args, "guide"); path.isNotEmpty())
            o.guide = juce::File::getCurrentWorkingDirectory().getChildFile (path);
        if (auto* m = window->main())
            m->applyLaunchOptions (o);

        // 新しいバージョン（DESIGN 11.7）：24 時間に 1 回まで、裏で確かめる。見つかればステータスバーに知らせ（失敗しても黙っている）
        if (o.screen.isEmpty() && ! args.contains ("--no-update-check"))
            session->checkForUpdatesIfDue();

        // 初回起動：言語を選ぶ → モードの質問
        const bool firstRun = args.contains ("--first-run")
                           || (! stored->getBoolValue ("firstRunDone", false) && ! args.contains ("--no-first-run"));
        if (auto* m = window->main())
        {
            if (firstRun && o.screen.isEmpty() && o.open == juce::File())
                m->openWelcome();
            else if (o.screen.isEmpty())
            {
                m->openStartIfNoSong();   // 2 回目からも、曲を開いていなければ起動画面（最近のプロジェクト）から
                // 分離などのモデルがまだ無ければ、ダウンロードを勧める（初回はモードを選んだ後。--no-update-check の時は起動時にネットへ行かない）
                if (! args.contains ("--no-update-check"))
                    m->offerModelsIfMissing();
            }
        }
    }

    void shutdown() override
    {
        if (session != nullptr)
        {
            session->closeForQuit();   // 録音中なら、そこまでをテイクとして入れてから保存（B14。監査 2026-10-03）
            session->removeListener (this);
        }
        window = nullptr;
        gallery = nullptr;
        if (session != nullptr)
            session->attachEngine (nullptr);
       #if ! VOICEBOOTH_UI_MOCK
        engine = nullptr;   // 出力デバイスを閉じる
       #endif
        session = nullptr;
        properties.closeFiles();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    juce::PropertiesFile* settings() { return properties.getUserSettings(); }

    /** モードが変わったら覚えておく（次回もそのモードで開く） */
    void sessionChanged (juce::uint32 changes) override
    {
        if (session == nullptr) return;

        if ((changes & change::song) && window != nullptr)
            window->updateTitle();

       #if ! VOICEBOOTH_UI_MOCK
        // 選んだデバイスを覚えておく（値が変わった時だけ書く）
        if ((changes & change::device) && engine != nullptr)
            if (auto xml = engine->createDeviceStateXml())
            {
                settings()->setValue ("audioDevice", xml.get());
                settings()->saveIfNeeded();
            }
       #endif

        if (changes & change::recordFormat)
        {
            settings()->setValue ("recordRate", session->get().recordRate);
            settings()->setValue ("recordFloat", session->get().recordFloat);
            settings()->saveIfNeeded();
        }

        if (changes & change::project)
        {
            const auto list = session->get().recentProjects.joinIntoString ("\n");
            if (list != settings()->getValue ("recentProjects"))
            {
                settings()->setValue ("recentProjects", list);
                settings()->saveIfNeeded();
            }
        }

        if (changes & change::latency)
        {
            const auto json = session->latencyProfilesJson();
            if (json != settings()->getValue ("latencyProfiles"))
            {
                settings()->setValue ("latencyProfiles", json);
                settings()->saveIfNeeded();
            }
        }

        if (changes & change::practice)
        {
            const auto& st = session->get();
            if (st.voiceLow != settings()->getIntValue ("voiceLow", -1) || st.voiceHigh != settings()->getIntValue ("voiceHigh", -1))
            {
                settings()->setValue ("voiceLow", st.voiceLow);
                settings()->setValue ("voiceHigh", st.voiceHigh);
                settings()->saveIfNeeded();
            }
        }

        // クリック・カウントイン（2026-10-02）：値が変わった時だけ書く
        if (changes & (change::transport | change::monitor))
        {
            const auto& st = session->get();
            if (st.countInBars != settings()->getIntValue ("countInBars", 1) || st.clickOn != settings()->getBoolValue ("clickOn", false)
                || std::abs (st.clickLevel - (float) settings()->getDoubleValue ("clickLevel", 0.62)) > 1.0e-4f)
            {
                settings()->setValue ("countInBars", st.countInBars);
                settings()->setValue ("clickOn", st.clickOn);
                settings()->setValue ("clickLevel", st.clickLevel);
                settings()->saveIfNeeded();
            }
        }

        if (changes & change::prefs)
        {
            // 値が同じなら PropertiesFile は書かない
            const auto& st = session->get();
            settings()->setValue ("updateAutoCheck", st.updateAutoCheck);
            settings()->setValue ("updateBetas", st.updateBetas);
            settings()->setValue ("updateSkipped", st.updateSkipped);
            settings()->setValue ("updateLastCheck", juce::String (st.updateLastCheck));
            if (! st.updateRelease.sample)   // 見本（--screen=update）は覚えない
                settings()->setValue ("updateFound", st.updateRelease.found ? st.updateRelease.toJson() : juce::String());
            settings()->setValue ("cacheFolder", st.cacheFolder.getFullPathName());
            settings()->setValue ("shortcuts", st.shortcuts.isDefault() ? juce::String() : st.shortcuts.toString());
            settings()->saveIfNeeded();
        }

        // 判定の幅・オクターブ合わせ・音域の表示（change::view。スクロールでも来るので、値が変わった時だけ書く）
        if (changes & change::view)
        {
            const auto& st = session->get();
            if (std::abs (st.pitchToleranceCents - (float) settings()->getDoubleValue ("pitchToleranceCents", 30.0)) > 1.0e-3f
                || st.octaveAlign != settings()->getBoolValue ("octaveAlign", true)
                || st.fullRange != settings()->getBoolValue ("fullRange", false))
            {
                settings()->setValue ("pitchToleranceCents", st.pitchToleranceCents);
                settings()->setValue ("octaveAlign", st.octaveAlign);
                settings()->setValue ("fullRange", st.fullRange);
                settings()->saveIfNeeded();
            }
        }

        if ((changes & change::mode) == 0) return;
        settings()->setValue ("mode", modeKey (session->get().mode));
        settings()->setValue ("showLyrics", session->get().showLyrics);
        settings()->setValue ("crossfadeMs", session->get().crossfadeMs);
        settings()->saveIfNeeded();
    }

    /** id のスキンを使う（無ければ booth） */
    void applySkinById (const juce::String& id)
    {
        const auto* s = skins != nullptr ? skins->find (id) : nullptr;
        if (s == nullptr) s = &skin::defaultSkin();
        skinId = s->id;
        vb::applySkin (*s);
        refreshSkinChrome();
    }

    /** JUCE 側が覚えている色（LookAndFeel・ウィンドウの地）を入れ直して描き直す */
    void refreshSkinChrome()
    {
        lookAndFeel.applySkinColours();
        for (auto* w : { (juce::DocumentWindow*) window.get(), (juce::DocumentWindow*) gallery.get() })
        {
            if (w == nullptr) continue;
            w->setBackgroundColour (colours::bg0);
            w->repaint();
        }
    }

    /** スキンを選んだ・保存した：言語と同じく画面を作り直す（同じ id でも色が変わっていれば作り直す） */
    void changeSkin (const juce::String& id, ReopenScreen reopen)
    {
        skins->reload();
        applySkinById (id);
        settings()->setValue ("skin", skinId);
        settings()->saveIfNeeded();

        juce::MessageManager::callAsync ([this, reopen]
        {
            if (window == nullptr) return;
            window->rebuild();
            if (auto* m = window->main())
                if (reopen == ReopenScreen::settings) m->openSettings();
        });
    }

    void changeLanguage (i18n::Language l, ReopenScreen reopen)
    {
        if (l == i18n::current()) return;

        i18n::setLanguage (l);
        settings()->setValue ("language", i18n::codeOf (l));
        settings()->saveIfNeeded();

        // ダイアログのコールバック中なので、作り直しは次のメッセージで
        juce::MessageManager::callAsync ([this, reopen]
        {
            if (window == nullptr) return;
            window->rebuild();
            if (auto* m = window->main())
            {
                if (reopen == ReopenScreen::settings) m->openSettings();   // 切り替えた結果をその場で見せる
                if (reopen == ReopenScreen::welcome)  m->openWelcome();
            }
        });
    }

    VoiceBoothLookAndFeel lookAndFeel;
    juce::ApplicationProperties properties;
    std::unique_ptr<skin::Library> skins;   // 自作スキン（設定と同じフォルダの Skins/）
    juce::String skinId { "booth" };
    AppHooks hooks;
   #if ! VOICEBOOTH_UI_MOCK
    std::unique_ptr<audio::PlaybackEngine> engine;   // session より先に作り、後に消す
   #endif
    std::unique_ptr<UiSession> session;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<GalleryWindow> gallery;
};
} // namespace vb

START_JUCE_APPLICATION (vb::VoiceBoothApplication)
