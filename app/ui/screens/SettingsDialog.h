#pragma once

#include "../Overlay.h"
#include "../UiSession.h"
#include "../parts/Dropdown.h"
#include "../../system/SystemCheck.h"

namespace vb
{
/** DESIGN 10 設定。言語はその場で切り替わる（画面を作り直す） */
class SettingsDialog : public DialogPanel, private SessionView
{
public:
    explicit SettingsDialog (UiSession&);

    std::function<void (i18n::Language)> onLanguage;
    std::function<void()> onOpenSetup;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override;

    // control が無い行は value（と LED）を右に描く
    struct Row { juce::String label, note; juce::Component* control; int controlWidth; juce::String value = {}; juce::Colour led = {}; };

    Dropdown language;
    SegmentedKeys mode, tolerance, countIn, crossfade;
    KeyButton octaveAlign, openSetup, cacheKey, supportKey;
    system::Info systemInfo;
    std::vector<Row> rows;
    std::vector<juce::Rectangle<int>> rowAreas;
};
} // namespace vb
