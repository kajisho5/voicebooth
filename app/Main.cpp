#include "ui/MainComponent.h"
#include "ui/Gallery.h"
#include "ui/VoiceBoothLookAndFeel.h"

#if ! VOICEBOOTH_UI_MOCK
 #include "audio/PlaybackEngine.h"
#endif

/*  起動オプション（開発・スクリーンショット用）
      --gallery              部品ギャラリー
      --lang=ja|en|ko|zh-Hans|zh-Hant   表示言語（保存された設定より優先）
      --skin=<id>            スキン（booth / studio-day / … / 自作の id。保存された設定より優先、保存はしない）
      --screen=<name>        start / setup / setup2 / setup3 / export / settings / skin-templates / skin-editor /
                             skin-editor-borrow / confirm-rec
      --open=<path>          その曲を開く（起動画面で読み込み → 波形。B1）
      --mode=easy|standard|pro
      --track=main|double|harm1
      --rec                  録音中の見た目で開く
      --play                 再生中で開く（再生ヘッドが動く）
      --first-run            初回起動の流れ（言語選択 → モードの質問）を必ず出す
      --no-first-run         初回起動の流れを出さない
      --reduce-motion        動きを減らす（OS の設定に関係なく。ばね・明滅・揺れを止めて最終状態だけ。DESIGN 4.10）
      --motion               動きを出す（OS で動きを減らす設定でも。確認用）

    アプリ設定（PropertiesFile）に保存するもの
      language      表示言語（初回に選び、以後は設定から変更）
      mode          最後に使ったモード（簡単 / 標準 / プロ）
      firstRunDone  初回の言語選択を終えたか
      skin          スキンの id（DESIGN 4.11。無い・消えた時は booth）。自作スキンは同じフォルダの Skins/ に .vbskin で置く
      audioDevice   オーディオデバイスの設定（AudioDeviceManager の XML。ドライバ・入出力の機器・入力チャンネル・SR・バッファ）。
                    戻せなければ既定のデバイスで開く */

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
        centreWithSize (MainComponent::defaultWidth, MainComponent::defaultHeight);
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

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

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

        // 最後に使ったモード
        const auto savedMode = stored->getValue ("mode");
        if (savedMode == "easy")     session->setMode (project::Mode::easy);
        if (savedMode == "standard") session->setMode (project::Mode::standard);
        if (savedMode == "pro")      session->setMode (project::Mode::pro);
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
        if (auto* m = window->main())
            m->applyLaunchOptions (o);

        // 初回起動：言語を選ぶ → モードの質問
        const bool firstRun = args.contains ("--first-run")
                           || (! stored->getBoolValue ("firstRunDone", false) && ! args.contains ("--no-first-run"));
        if (firstRun && o.screen.isEmpty() && o.open == juce::File())
            if (auto* m = window->main())
                m->openWelcome();
    }

    void shutdown() override
    {
        if (session != nullptr)
            session->removeListener (this);
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

        if ((changes & change::mode) == 0) return;
        settings()->setValue ("mode", modeKey (session->get().mode));
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
