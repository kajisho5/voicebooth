#pragma once

#include "../Overlay.h"
#include "../UiSession.h"

/*  共有用の動画（DESIGN 9.1）
    左に実際のコマの見本、右に 形（縦 / 正方形 / 横）・区間（曲全体 / 範囲）・音程の線・背景の画像・投稿文・権利の注意。
    ［書き出す］でプロジェクトフォルダの share/ に MP4 を書く（バックグラウンド。進み具合と［中止］を表示）。
    書き終えたら［フォルダを開く］。アプリから直接は投稿しない（投稿文をコピーして、使う人が載せる） */

namespace vb
{
class ShareVideoDialog : public DialogPanel,
                         private SessionView
{
public:
    explicit ShareVideoDialog (UiSession&);
    ~ShareVideoDialog() override;

    std::function<void()> onFinished;

    /** 選んでいる形などから、書き出しの頼み */
    UiSession::ShareRequest request() const;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override;
    void optionsChanged();
    void refreshPreview();
    void refreshState();
    void chooseBackground();
    void loadBackground (const juce::File&);

    SegmentedKeys shapeKeys, spanKeys;
    KeyButton pitchKey, chooseKey, clearKey, copyKey, openKey, cancelKey;
    KeyButton* exportKey = nullptr;
    juce::TextEditor post;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Image background;          // 背景の画像（縮めたもの。無効ならスキンの色）
    juce::String backgroundName, backgroundError;
    juce::Image preview;             // 見本のコマ（実寸）
    bool copied = false;             // 投稿文をコピーした（文を変えるまで「コピーしました」）

    juce::Rectangle<int> previewArea, shapeRow, spanRow, pitchRow, backRow, postLabel, rightsArea, statusArea;
};
} // namespace vb
