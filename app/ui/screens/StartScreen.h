#pragma once

#include "../UiSession.h"
#include "../parts/KeyButton.h"

namespace vb
{
/** DESIGN 3-1 起動 / プロジェクト選択 / 曲読み込み（静的モック）
    - 曲を読み込む（ドロップ / クリック）→ 解析プログレス（DESIGN 7.1）
    - 最近のプロジェクト
    - 初回だけ「まず歌う / 録って渡す / 細かくやる」（DESIGN 2） */
class StartScreen : public juce::Component, private SessionView
{
public:
    StartScreen (UiSession&, bool analyzing);

    std::function<void()> onDone;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }   // 最近のプロジェクトのホバー

private:
    void onSessionChanged (juce::uint32) override {}
    void paintHome (juce::Graphics&);
    void paintAnalyzing (juce::Graphics&);

    bool analyzing;
    juce::Rectangle<int> panel, dropArea, recentArea, firstRunArea;
    std::vector<juce::Rectangle<int>> recentRows;
    juce::OwnedArray<KeyButton> firstRunKeys;
    KeyButton openFolder, continueKey, cancelKey;
};
} // namespace vb
