#include "ui/MainComponent.h"
#include "ui/Gallery.h"
#include "ui/VoiceBoothLookAndFeel.h"

#if ! VOICEBOOTH_UI_MOCK
 #error "Phase A supports the UI_MOCK build only (DESIGN 11.1 / 16)"
#endif

/*  起動オプション（開発・スクリーンショット用）
      --gallery              部品ギャラリー
      --lang=ja|en           表示言語（保存された設定より優先）
      --screen=<name>        start / analyzing / setup / setup2 / setup3 / export / settings / confirm-rec
      --mode=easy|standard|pro
      --track=main|double|harm1
      --rec                  録音中の見た目で開く
      --play                 再生中で開く（再生ヘッドが動く） */

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
}

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (UiSession& s, std::function<void (i18n::Language)> changeLanguage)
        : DocumentWindow ("VoiceBooth", colours::bg0, DocumentWindow::allButtons),
          session (s), languageChanger (std::move (changeLanguage))
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
        auto* content = new MainComponent (session, languageChanger);
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
    std::function<void (i18n::Language)> languageChanger;

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

class VoiceBoothApplication : public juce::JUCEApplication
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

        const auto args = juce::StringArray::fromTokens (commandLine, true);

        // 言語：起動オプション > 保存した設定 > OS の表示言語
        auto lang = i18n::fromSystem();
        if (auto* p = properties.getUserSettings())
            lang = i18n::fromCode (p->getValue ("language"), lang);
        lang = i18n::fromCode (argValue (args, "lang"), lang);
        i18n::setLanguage (lang);

        if (args.contains ("--gallery"))
        {
            gallery = std::make_unique<GalleryWindow>();
            return;
        }

        session = std::make_unique<UiSession>();
        window = std::make_unique<MainWindow> (*session, [this] (i18n::Language l) { changeLanguage (l); });

        LaunchOptions o;
        o.screen = argValue (args, "screen");
        o.mode = argValue (args, "mode");
        o.track = argValue (args, "track");
        o.recording = args.contains ("--rec");
        o.playing = args.contains ("--play");
        if (auto* m = window->main())
            m->applyLaunchOptions (o);
    }

    void shutdown() override
    {
        window = nullptr;
        gallery = nullptr;
        session = nullptr;
        properties.closeFiles();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    void changeLanguage (i18n::Language l)
    {
        if (l == i18n::current()) return;

        i18n::setLanguage (l);
        if (auto* p = properties.getUserSettings())
        {
            p->setValue ("language", i18n::codeOf (l));
            p->saveIfNeeded();
        }

        // 設定ダイアログのコールバック中なので、作り直しは次のメッセージで
        juce::MessageManager::callAsync ([this]
        {
            if (window == nullptr) return;
            window->rebuild();
            if (auto* m = window->main())
                m->openSettings();   // 切り替えた結果をその場で見せる
        });
    }

    VoiceBoothLookAndFeel lookAndFeel;
    juce::ApplicationProperties properties;
    std::unique_ptr<UiSession> session;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<GalleryWindow> gallery;
};
} // namespace vb

START_JUCE_APPLICATION (vb::VoiceBoothApplication)
