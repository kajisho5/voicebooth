#pragma once

#include "../UiSession.h"
#include "../parts/KeyButton.h"
#include "../parts/Dropdown.h"

namespace vb
{
/** 初回起動の最初の画面：表示言語を選ぶ（以後は記憶。設定からいつでも変更可）
    選ぶとその場で画面の言語が切り替わる */
class WelcomeScreen : public juce::Component
{
public:
    WelcomeScreen();

    std::function<void (i18n::Language)> onLanguage;
    std::function<void()> onContinue;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Dropdown language;
    KeyButton continueKey;
    juce::Rectangle<int> column, languageLabelArea;
};
} // namespace vb
