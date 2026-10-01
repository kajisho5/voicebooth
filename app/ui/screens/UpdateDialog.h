#pragma once

#include "../Overlay.h"

namespace vb
{
/** 新しいバージョンのお知らせ（DESIGN 11.7。静的モック）
    ステータスバーの知らせを押すと開く。勝手に入れ替えない：ユーザーが押したら更新
    実際の確認・ダウンロード・署名確認は配布前に WinSparkle / Sparkle で作る */
class UpdateDialog : public DialogPanel
{
public:
    UpdateDialog();

    std::function<void()> onInstall, onSkip;

protected:
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;
};

/** モデルの初回ダウンロード（DESIGN 11.7。静的モック）
    初めて分離・ピッチ・歌詞を使う時に、サイズ・ライセンス・保存先を見せて確認してから取る。
    状態ごとに開き直す（確認 → ダウンロード中 → 完了 / 失敗） */
class ModelDownloadDialog : public DialogPanel, private juce::Timer
{
public:
    enum class Stage { confirm, downloading, done, failed };

    /** animate：ダウンロード中の進み具合を動かす（スクリーンショット用は false で 62% に止める） */
    ModelDownloadDialog (Stage, bool animate);
    ~ModelDownloadDialog() override;

    /** 次の状態へ（開き直してもらう） */
    std::function<void (Stage)> onStage;

protected:
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void timerCallback() override;

    Stage stage;
    float progress = 0.62f;
};
} // namespace vb
