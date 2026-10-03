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
    std::function<void()> onAbout;        // このアプリについて・ライセンス
    std::function<void()> onClearCache;   // キャッシュを空にする（確認は MainComponent が出す）

    void parentHierarchyChanged() override { fitToParent(); }
    void parentSizeChanged() override { fitToParent(); }

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override;

    /** 行の高さを窓の高さに合わせる（広い窓ではゆったり、狭い窓でも全部の行が収まる） */
    void fitToParent();
    int rowH = 56;

    // control が無い行は value（と LED）を右に描く。extra は control の左に並べるキー
    struct Row { juce::String label, note; juce::Component* control; int controlWidth; juce::String value = {}; juce::Colour led = {};
                 std::vector<KeyButton*> extras = {}; };

    std::vector<skin::Skin> skinChoices;
    Dropdown language, skinPicker;
    KeyButton editSkin, newSkin;
    SegmentedKeys mode, tolerance, countIn, crossfade;
    KeyButton octaveAlign, showLyrics, openSetup, cacheKey, supportKey;
    KeyButton cacheOpen, cacheClear;               // キャッシュの場所（DESIGN 8）：開く・空にする
    KeyButton updateAuto, updateBetas, updateNow;  // 新しいバージョンの確認（DESIGN 11.7）
    std::unique_ptr<juce::FileChooser> chooser;
    void chooseCacheFolder();
    void refreshNotes();
    system::Info systemInfo;
    std::vector<Row> rows;
    std::vector<juce::Rectangle<int>> rowAreas;
};
} // namespace vb
