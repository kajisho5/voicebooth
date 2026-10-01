#pragma once

#include "../Overlay.h"
#include "../UiSession.h"
#include "../parts/Dropdown.h"
#include "../../system/SystemCheck.h"

namespace vb
{
/** DESIGN 10 設定。言語・スキンはその場で切り替わる（画面を作り直す） */
class SettingsDialog : public DialogPanel, private SessionView
{
public:
    /** skins：内蔵＋自作（選べるスキン）、currentSkin：いま選んでいる id */
    SettingsDialog (UiSession&, std::vector<skin::Skin> skins, const juce::String& currentSkin);

    std::function<void (i18n::Language)> onLanguage;
    std::function<void()> onOpenSetup;
    std::function<void (const juce::String& skinId)> onSkin;   // DESIGN 4.11
    std::function<void()> onEditSkin;
    std::function<void()> onNewSkin;   // テンプレートから作る

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override;

    // control が無い行は value（と LED）を右に描く。extra は control の左に並べるキー
    struct Row { juce::String label, note; juce::Component* control; int controlWidth; juce::String value = {}; juce::Colour led = {};
                 std::vector<KeyButton*> extras = {}; };

    std::vector<skin::Skin> skinChoices;
    Dropdown language, skinPicker;
    KeyButton editSkin, newSkin;
    SegmentedKeys mode, tolerance, countIn, crossfade;
    KeyButton octaveAlign, openSetup, cacheKey, supportKey;
    system::Info systemInfo;
    std::vector<Row> rows;
    std::vector<juce::Rectangle<int>> rowAreas;
};
} // namespace vb
