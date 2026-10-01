#include "ui/MainComponent.h"
#include "ui/VoiceBoothLookAndFeel.h"

#if ! VOICEBOOTH_UI_MOCK
 #error "Phase A は UI_MOCK ビルドのみ（DESIGN 11.1 / 16）"
#endif

namespace vb
{
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow()
        : DocumentWindow ("VoiceBooth", colours::bg0, DocumentWindow::allButtons)
    {
        auto* content = new MainComponent();
        setName (juce::String::fromUTF8 ("VoiceBooth — ") + content->getSongName());

        setUsingNativeTitleBar (true);
        setContentOwned (content, true);
        setResizable (true, true);
        setResizeLimits (1280, 760, 4096, 2160);
        centreWithSize (MainComponent::defaultWidth, MainComponent::defaultHeight);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

class VoiceBoothApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        window = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        window = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    VoiceBoothLookAndFeel lookAndFeel;
    std::unique_ptr<MainWindow> window;
};
} // namespace vb

START_JUCE_APPLICATION (vb::VoiceBoothApplication)
