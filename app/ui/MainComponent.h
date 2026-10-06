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
#include "screens/ExportDialog.h"
#include "audio/SongLoader.h"

namespace vb
{
/** 言語・スキンを変えた後に開き直す画面 */
enum class ReopenScreen { none, settings, welcome };

/** アプリ本体（設定の保存・画面の作り直し）への窓口 */
struct AppHooks
{
    std::function<void (i18n::Language, ReopenScreen)> changeLanguage;
    std::function<void()> firstRunDone;   // 初回の言語選択を確定した
    // 入力セットアップを済ませた機器（ドライバ | 入力の名前）。違う機器で曲を開いたら、セットアップを開く（DESIGN 5 / B13）
    std::function<juce::String()> setupDoneFor;
    std::function<void (const juce::String&)> setSetupDoneFor;

    // スキン（DESIGN 4.11）
    std::function<void (const juce::String& id, ReopenScreen)> changeSkin;   // 選んで保存し、画面を作り直す
    std::function<void (const skin::Skin&)> previewSkin;                       // 色だけ画面に反映（保存しない。エディタ用）
    std::function<juce::String()> currentSkin;                                 // いま選んでいるスキンの id
    skin::Library* skins = nullptr;                                            // 内蔵＋自作（Skins/）
};

/** 起動時の指定（開発・スクリーンショット用。--screen= など） */
struct LaunchOptions
{
    juce::String screen;          // start / setup / setup2 / setup3 / export / settings / skin-templates / skin-editor /
                                  // skin-editor-borrow / confirm-rec / compare /
                                  // update / update-notice / model-download / model-downloading / model-interrupted / model-done / model-failed
    juce::File open;              // この曲を開く（--open=）
    bool recording = false;
    bool playing = false;
    juce::String mode;            // easy / standard / pro
    juce::String track;           // main / double / harm1
    juce::File lyrics;            // 歌詞パッドをこのファイルで開く（--lyrics=。B4b）
    juce::File guide;             // 曲を開いたら、このお手本（声入りの原曲）を重ねる（--guide=。B9）
};

/** DESIGN 4 メイン画面（練習兼録音）。左にキャンバス、右にラック。
    ショートカット（DESIGN 4.2）と、確認ダイアログが要る操作（DESIGN 4.7 / 6）もここで扱う */
class StartScreen;

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::MenuBarModel,
                      private juce::Timer,
                      private SessionView
{
public:
    MainComponent (UiSession&, AppHooks&);
    ~MainComponent() override;

    void applyLaunchOptions (const LaunchOptions&);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

    /** 曲ファイルをメイン画面にドロップ → 起動画面で読み込む（B1） */
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    void openWelcome();
    StartScreen* openStart (bool firstRun = false);
    /** 本物のアプリで曲を開いていなければ起動画面を出す（見本の曲・ダミーの線を見せない。ほかの画面が出ていれば何もしない） */
    void openStartIfNoSong();
    /** 分離などのモデルがまだ無ければ、ダウンロードの確認を出す（起動した時・初回のモードを選んだ後。「あとで」なら次の起動でまた勧める） */
    void offerModelsIfMissing();
    /** 本物のアプリで、まだ曲を開いていない（起動画面を閉じるとダミーが見えてしまう） */
    bool needsSong() const;
    void openSong (const juce::File&);
    void openSetup (int step = 0);
    void openExport();
    void openExport (const ExportDialog::Choice& restore);   // 選んだものを戻して開き直す
    void openSettings();
    void openAbout (bool backToSettings = true);       // 閉じたら設定に戻る（設定から開いたとき）
    void openHelp();        // 困ったときのヘルプ（F1・ヘルプのメニュー）
    void openShortcuts (bool backToSettings = true);   // 1 文字のショートカット（#28）
    void openReport();      // 不具合を報告する（ヘルプのメニュー）

    /** ヘルプのメニュー（2026-10-05。上のバーの ? と、Mac のメニューバーの「ヘルプ」で同じもの） */
    juce::PopupMenu helpMenu();
    /** メニューから画面を開いてよいか（録音中・ほかの画面を出しているときは開かない） */
    bool canOpenFromMenu() const;
    /** モデルのダウンロードの確認を出してよいか（録音中・ほかの画面の途中は待つ） */
    bool canShowModelOffer() const;

    // Mac のメニューバー（ヘルプのメニューだけ。Windows はウィンドウにメニューバーがないので ? キーから）
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int, int) override {}
    void openSkinTemplates();
    void openSkinEditor (const skin::Skin* fromTemplate = nullptr);   // nullptr：いまのスキンを編集
    void openUpdate();
    /** 設定の「空にする」：大きさを見せて確かめてから、アプリのキャッシュを空にする（終わったら設定に戻る） */
    void confirmClearCache();
    void openModelDownload (int stage, bool animate, float from = -1.0f);   // ModelDownloadDialog::Stage、from：届いた割合

    // 曲の情報（B4b。DESIGN 7.5）
    void openSongInfo();
    void openLyrics (const juce::File& file = {});
    void openSectionName (int index);

    /** テイク比較（B18c）。from >= to なら IN / OUT（無ければ曲全体）、そうでなければその区間 */
    void openTakeCompare (int64 from, int64 to);

    void showToast (const juce::String&);
    /** 押せるキー付きの知らせ（長めに出す。録り間違いの救済など） */
    void showToast (const juce::String&, const juce::String& actionLabel, std::function<void()> action);

    static constexpr int defaultWidth = 1440, defaultHeight = 900;
    static constexpr int preferredWidth = 1760, preferredHeight = 990;   // 1920x1080 の画面で開く大きさ（作業領域に収まらなければ縮める）
    // 1366x768・1280x720 の画面（1920x1080 を 150 % 表示にしたノートも同じ）でも窓が画面に収まるように（#18）。
    // 右のラックは高さが足りなければ縦にスクロールし、ダイアログは見えている範囲に合わせて縮める
    static constexpr int minWidth = 1180, minHeight = 640;

private:
    void timerCallback() override;
    void onSessionChanged (juce::uint32) override;
    juce::File pendingGuide;   // 曲が開いたら重ねるお手本（--guide=）

    void toggleRecord();
    void requestTempo (int);
    void requestKey (int);
    void confirmDiscardRecording();
    void showConfirm (const juce::String& title, const juce::String& message, std::vector<ConfirmDialog::Option>,
                      std::function<void()> onDismiss = {});   // onDismiss：どれも選ばずに閉じた（Esc）

    /** 曲の情報のショートカット（T / M / Enter / ↑ ↓ / Delete / Backspace / Esc。B4b）。扱ったら true */
    void runShortcut (shortcuts::Action, const juce::KeyPress&);   // 1 文字のショートカット（#28）
    bool songInfoKey (const juce::KeyPress&);
    void layoutToastLayer();   // キー付きの知らせの部品を今の画面の大きさに合わせる
    void tapTempo();
    /** 知らせ（録音中は出さない。DESIGN 4.10.1 TS） */
    void notice (const juce::String&);

    AppHooks& hooks;
    Actions actions;
    audio::SongLoader songLoader;   // 曲の読み込み（バックグラウンド）

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
    int heldKey = 0;            // 押したままのキー（自動リピートを受けない。離すと 0）
    int deviceLostSeen = 0;     // 「デバイスが外れました」を知らせた回数
    void maybeOpenSetup();      // 初めての機器なら入力セットアップを開く（B13）
    int modelDialogSeen = 0;       // 分離モデルのダウンロード画面を開いた回数（B16）
    int modelStageShown = -2;      // いま開いているダウンロード画面の段階（-2 = 開いていない）
    juce::File pendingOriginal;    // 原曲だけで始めたいがモデルが無かった：モデルが入ったら起動画面から続ける（B16）
    int modelStageBehind = -1;     // 画面を閉じた後に見た段階（裏で進んだダウンロードの終わりを知らせる）
    void openLiveModelDownload (int dialogStage);
    int noticeSeen = 0;         // UiSession の知らせ（録音・書き出しの結果など）を出した回数

    juce::String toastText;
    double toastUntil = 0.0;
    KeyButton toastKey;                       // 知らせの右のキー（ある時だけ）
    std::function<void()> toastAction;
    juce::Rectangle<float> toastBox() const;
    /** キー付きの知らせ：ほかの部品より上に置く板（キー以外のクリックは下へ通す） */
    struct ToastLayer : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    } toastLayer;
    void paintToast (juce::Graphics&, juce::Point<float> origin);

    juce::TooltipWindow tooltips { this, 500 };   // 少し待って出す。一度出たら隣へ移る時はすぐ（DESIGN 4.10.1 TT）

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace vb
