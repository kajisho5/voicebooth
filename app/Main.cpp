#include "ui/MainComponent.h"
#include "ui/Gallery.h"
#include "ui/VoiceBoothLookAndFeel.h"

#if ! VOICEBOOTH_UI_MOCK
 #error "Phase A supports the UI_MOCK build only (DESIGN 11.1 / 16)"
#endif

/*  起動オプション（開発・スクリーンショット用）
      --gallery              部品ギャラリー
      --lang=ja|en|ko|zh-Hans|zh-Hant   表示言語（保存された設定より優先）
      --screen=<name>        start / analyzing / setup / setup2 / setup3 / export / settings / confirm-rec
      --mode=easy|standard|pro
      --track=main|double|harm1
      --rec                  録音中の見た目で開く
      --play                 再生中で開く（再生ヘッドが動く）
      --first-run            初回起動の流れ（言語選択 → モードの質問）を必ず出す
      --no-first-run         初回起動の流れを出さない

    アプリ設定（PropertiesFile）に保存するもの
      language      表示言語（初回に選び、以後は設定から変更）
      mode          最後に使ったモード（簡単 / 標準 / プロ）
      firstRunDone  初回の言語選択を終えたか */

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
        setName (tr ("app.name") + juce::String::fromUTF8 (" \xe2\x80\x94 ") + session->songName);
        setContentOwned (content, false);
        focusContent();
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

        const auto args = juce::StringArray::fromTokens (commandLine, true);

        // 言語：起動オプション > 保存した設定 > OS の表示言語
        auto lang = i18n::fromSystem();
        lang = i18n::fromCode (stored->getValue ("language"), lang);
        lang = i18n::fromCode (argValue (args, "lang"), lang);
        i18n::setLanguage (lang);

        if (args.contains ("--gallery"))
        {
            gallery = std::make_unique<GalleryWindow>();
            return;
        }

        hooks.changeLanguage = [this] (i18n::Language l, ReopenScreen reopen) { changeLanguage (l, reopen); };
        hooks.firstRunDone = [this]
        {
            settings()->setValue ("language", i18n::codeOf (i18n::current()));
            settings()->setValue ("firstRunDone", true);
            settings()->saveIfNeeded();
        };

        session = std::make_unique<UiSession>();

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
        if (auto* m = window->main())
            m->applyLaunchOptions (o);

        // 初回起動：言語を選ぶ → モードの質問
        const bool firstRun = args.contains ("--first-run")
                           || (! stored->getBoolValue ("firstRunDone", false) && ! args.contains ("--no-first-run"));
        if (firstRun && o.screen.isEmpty())
            if (auto* m = window->main())
                m->openWelcome();
    }

    void shutdown() override
    {
        if (session != nullptr)
            session->removeListener (this);
        window = nullptr;
        gallery = nullptr;
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
        if ((changes & change::mode) == 0 || session == nullptr) return;
        settings()->setValue ("mode", modeKey (session->get().mode));
        settings()->saveIfNeeded();
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
    AppHooks hooks;
    std::unique_ptr<UiSession> session;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<GalleryWindow> gallery;
};
} // namespace vb

START_JUCE_APPLICATION (vb::VoiceBoothApplication)
