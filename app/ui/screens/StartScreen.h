#pragma once

#include "../UiSession.h"
#include "../parts/KeyButton.h"
#include "WaitGame.h"
#include "audio/SongLoader.h"

namespace vb
{
/** DESIGN 3-1 起動 / プロジェクト選択 / 曲読み込み
    - 2 つの枠（DESIGN 7.1.1）：オフボ（カラオケ。時間の基準）と、お手本（声入りの原曲。任意）。
      ドロップした位置の枠に入る。オフボを読み込んだら解析画面へ。お手本が入っていれば、オフボを開いた後に重ねる（B9）
      B1：形式・長さ・SR・チャンネルを読み、波形の概形を作るところまで本物。ほかの解析は SKIP
    - 最近のプロジェクト（B14：アプリ設定の一覧。見本（UI_MOCK）はダミー）。「プロジェクトを開く」で .vbooth を選ぶ
    - 初回だけ「まず歌う / 録って渡す / 細かくやる」（DESIGN 2） */
class StartScreen : public juce::Component,
                    public juce::FileDragAndDropTarget,
                    private SessionView,
                    private juce::Timer
{
public:
    StartScreen (UiSession&, audio::SongLoader&, bool firstRun);
    ~StartScreen() override;

    std::function<void()> onDone;
    /** 原曲だけで始めたいが分離モデルが無い（入れられる）：呼び出し側がモデルの確認を出す（B16） */
    std::function<void (const juce::File& original)> onNeedModel;
    /** 原曲だけで始める前に、分離の見込み時間を表示して確認する（#27）。確認したら startFromOriginal (original, true) で戻る */
    std::function<void (const juce::File& original)> onConfirmOriginal;
    std::function<void()> onInstallModels;   // 「分離モデルを入れる」（起動画面は開いたまま。確かめたらダウンロードの確認に替わる）
    std::function<void()> onModeChosen;      // 初回のモードの質問に答えた

    /** その曲の読み込みを始める（解析画面へ） */
    void openFile (const juce::File&);
    /** 原曲だけ（B16。DESIGN 7.1.1）：原曲を分離してオフボを作り、それを開いて原曲をお手本に重ねる */
    /** confirmed：見込み時間の確認が済んでいる（onConfirmOriginal が無ければ確認せずに始める） */
    void startFromOriginal (const juce::File& original, bool confirmed = false);
    /** お手本（声入りの原曲）の枠に入れる（読むのはオフボを開いた後） */
    void setGuide (const juce::File&);
    /** お手本は開いた後に重ねる（--open と --guide を一緒に渡した時）。解析の「お手本ピッチ」を「開いた後」と出す */
    void markGuideAfterOpen() { guideAfterOpen = true; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }   // ホバー
    bool keyPressed (const juce::KeyPress&) override;   // 読み込みが終わったら Enter でメイン画面へ

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragMove (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    enum class Phase { home, separating, loading, loaded, failed };

    void onSessionChanged (juce::uint32 c) override { if (c & change::project) { refreshRecents(); resized(); repaint(); } }
    void timerCallback() override { repaint(); }
    void setPhase (Phase);
    void chooseFile (bool guide = false);
    void chooseProject();
    bool openProject (const juce::File&);
    void refreshRecents();
    void paintSlot (juce::Graphics&, juce::Rectangle<int>, Icon, const juce::String& title, const juce::String& sub,
                    const juce::String& note, bool dropping, bool done, int textRightInset = 0);
    void loadFinished (audio::LoadResult);

    void paintHome (juce::Graphics&);
    void paintAnalyzing (juce::Graphics&);
    juce::String songInfoLine() const;
    juce::String errorText() const;

    audio::SongLoader& loader;
    std::unique_ptr<juce::FileChooser> chooser;

    Phase phase = Phase::home;
    bool firstRun, dragHover = false, guideAfterOpen = false;
    juce::Point<int> dragPos;
    juce::File guideFile;                      // お手本（声入りの原曲。B9）
    juce::File file;
    audio::SongInfo info;
    audio::LoadResult::Error error = audio::LoadResult::Error::none;
    juce::String projectError;                 // .vbooth を開けなかった理由（翻訳キー）
    bool fromOriginal = false;                 // 原曲だけで始めた（分離の行が本物）
    juce::String separationError;              // 分離できなかった理由（翻訳済み）

    struct RecentRow { juce::String name, date, length; project::Mode mode = project::Mode::standard; juce::File file; };
    std::vector<RecentRow> recents;

    juce::Rectangle<int> panel, dropArea, guideArea, localNoteArea, recentArea, firstRunArea;
    std::vector<juce::Rectangle<int>> recentRows;
    juce::OwnedArray<KeyButton> firstRunKeys;
    KeyButton openFolder, continueKey, cancelKey, anotherKey, originalKey, modelKey;
    WaitGame game { session };                 // 原曲から分離している間に遊べる（音程あて・リズムタップ）
    int analyzingRowsTop() const;              // 解析画面の 1 行目の上端（描画と部品の置き場所をそろえる）
    bool canInstallModels() const;
};
} // namespace vb
