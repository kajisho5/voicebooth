#pragma once

#include "UiSession.h"
#include "Actions.h"
#include "TopBar.h"
#include "TransportBar.h"
#include "PitchLane.h"
#include "LyricsLane.h"
#include "WaveLane.h"
#include "TrackTabs.h"
#include "Rack.h"
#include "StatusBar.h"
#include "Overlay.h"

namespace vb
{
/** 言語を変えた後に開き直す画面 */
enum class ReopenScreen { none, settings, welcome };

/** アプリ本体（設定の保存・画面の作り直し）への窓口 */
struct AppHooks
{
    std::function<void (i18n::Language, ReopenScreen)> changeLanguage;
    std::function<void()> firstRunDone;   // 初回の言語選択を確定した
};

/** 起動時の指定（開発・スクリーンショット用。--screen= など） */
struct LaunchOptions
{
    juce::String screen;          // start / analyzing / setup / setup2 / setup3 / export / settings / confirm-rec
    bool recording = false;
    bool playing = false;
    juce::String mode;            // easy / standard / pro
    juce::String track;           // main / double / harm1
};

/** DESIGN 4 メイン画面（練習兼録音）。左にキャンバス、右にラック。
    ショートカット（DESIGN 4.2）と、確認ダイアログが要る操作（DESIGN 4.7 / 6）もここで扱う */
class MainComponent : public juce::Component, private juce::Timer, private SessionView
{
public:
    MainComponent (UiSession&, AppHooks&);
    ~MainComponent() override;

    void applyLaunchOptions (const LaunchOptions&);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void openWelcome();
    void openStart (bool analyzing = false, bool firstRun = false);
    void openSetup (int step = 0);
    void openExport();
    void openSettings();

    void showToast (const juce::String&);

    static constexpr int defaultWidth = 1440, defaultHeight = 900;
    static constexpr int minWidth = 1280, minHeight = 800;

private:
    void timerCallback() override;
    void onSessionChanged (juce::uint32) override;

    void toggleRecord();
    void requestTempo (int);
    void requestKey (int);
    void confirmDiscardRecording();
    void showConfirm (const juce::String& title, const juce::String& message, std::vector<ConfirmDialog::Option>);

    AppHooks& hooks;
    Actions actions;

    TopBar top;
    TransportBar transport;
    PitchLane pitch;
    LyricsLane lyrics;
    WaveLane wave;
    TrackTabs tracks;
    Rack rack;
    StatusBar status;
    OverlayHost overlay;

    juce::Rectangle<int> canvasArea;
    double lastTick = 0.0;
    bool clockFrozen = false;   // スクリーンショット用（--rec）

    juce::String toastText;
    double toastUntil = 0.0;

    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace vb
