#pragma once

#include "../UiSession.h"
#include "../parts/KeyButton.h"
#include "audio/SongLoader.h"

namespace vb
{
/** DESIGN 3-1 起動 / プロジェクト選択 / 曲読み込み
    - 2 つの枠（DESIGN 7.1.1）：オフボ（カラオケ。時間の基準）と、お手本（声入りの原曲。任意）。
      ドロップした位置の枠に入る。オフボを読み込んだら解析画面へ。お手本が入っていれば、オフボを開いた後に重ねる（B9）
      B1：形式・長さ・SR・チャンネルを読み、波形の概形を作るところまで本物。ほかの解析は SKIP
    - 最近のプロジェクト（B14 まではダミー）
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

    /** その曲の読み込みを始める（解析画面へ） */
    void openFile (const juce::File&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }   // ホバー

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragMove (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    enum class Phase { home, loading, loaded, failed };

    void onSessionChanged (juce::uint32) override {}
    void timerCallback() override { repaint(); }
    void setPhase (Phase);
    void chooseFile (bool guide = false);
    void setGuide (const juce::File&);
    void paintSlot (juce::Graphics&, juce::Rectangle<int>, Icon, const juce::String& title, const juce::String& sub,
                    const juce::String& note, bool dropping, bool done);
    void loadFinished (audio::LoadResult);

    void paintHome (juce::Graphics&);
    void paintAnalyzing (juce::Graphics&);
    juce::String songInfoLine() const;
    juce::String errorText() const;

    audio::SongLoader& loader;
    std::unique_ptr<juce::FileChooser> chooser;

    Phase phase = Phase::home;
    bool firstRun, dragHover = false;
    juce::Point<int> dragPos;
    juce::File guideFile;                      // お手本（声入りの原曲。B9）
    juce::File file;
    audio::SongInfo info;
    audio::LoadResult::Error error = audio::LoadResult::Error::none;

    juce::Rectangle<int> panel, dropArea, guideArea, recentArea, firstRunArea;
    std::vector<juce::Rectangle<int>> recentRows;
    juce::OwnedArray<KeyButton> firstRunKeys;
    KeyButton openFolder, continueKey, cancelKey, anotherKey;
};
} // namespace vb
