#include "ui/MainComponent.h"
#include "ui/Gallery.h"
#include "ui/VoiceBoothLookAndFeel.h"

#if ! VOICEBOOTH_UI_MOCK
 #error "Phase A は UI_MOCK ビルドのみ（DESIGN 11.1 / 16）"
#endif

namespace vb
{
class MainWindow : public juce::DocumentWindow
{
public:
    /** gallery = true で部品ギャラリー（開発用）を開く */
    explicit MainWindow (bool gallery)
        : DocumentWindow ("VoiceBooth", colours::bg0, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);

        if (gallery)
        {
            setName (jp ("VoiceBooth — 部品ギャラリー"));
            setContentOwned (new Gallery(), true);
            setResizable (true, true);
        }
        else
        {
            auto* content = new MainComponent();
            setName (jp ("VoiceBooth — ") + content->getSongName());
            setContentOwned (content, true);
            setResizable (true, true);
            setResizeLimits (MainComponent::minWidth, MainComponent::minHeight, 4096, 2160);
        }

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

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        window = std::make_unique<MainWindow> (commandLine.contains ("--gallery"));
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
