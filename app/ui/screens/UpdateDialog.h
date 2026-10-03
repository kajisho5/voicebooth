#pragma once

#include "../Overlay.h"
#include "../UiSession.h"

namespace vb
{
/** 新しいバージョンのお知らせ（DESIGN 11.7）
    ステータスバーの知らせを押すと開く。GitHub のリリース（UiSession::checkForUpdates*）で見つけた版の中身を見せる。
    ビルドは署名していないので自分では入れ替えない：主のキーはブラウザでこの OS のインストーラー（無ければリリースのページ）を開く */
class UpdateDialog : public DialogPanel
{
public:
    explicit UpdateDialog (const update::Release&);

    /** onOpen：主のキー（インストーラーかページ）、onOpenPage：リリースのページ（インストーラーがある時だけキーを出す）、
        onSkip：この版を飛ばす。「あとで」は閉じるだけ（onCloseRequest） */
    std::function<void()> onOpen, onOpenPage, onSkip;

protected:
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    update::Release release;
    juce::String notes;   // 本文の平文（update::plainNotes）
};

/** モデルの初回ダウンロード（DESIGN 11.7。静的モック）
    初めて分離・ピッチ・歌詞を使う時に、サイズ・ライセンス・保存先を見せて確認してから取る。
    状態ごとに開き直す（確認 → ダウンロード中 → 完了 / 失敗、途中で切れたら「続きから再開」）

    動き（DESIGN 4.10「ダウンロード」「失敗と再開」）：
    - LED の列が埋まる。先頭の 1 つが明滅。タリーで状態（琥珀＝取得中、青＝照合中、緑＝完了、赤＝失敗）
    - 完了で全体が一度光る、失敗で小さく揺れる。数値と残り時間はなめらかに追う
    - 切れた時：届いた LED は点いたまま（少し暗く）、切れた所の粒が琥珀で明滅し「N 秒後に続きから再開（k / 3 回目）」
    通信はしない（進み具合・速度はダミー） */
class ModelDownloadDialog : public DialogPanel, private motion::Animated
{
public:
    enum class Stage { confirm, downloading, interrupted, done, failed };

    /** animate：ダウンロード中・再開待ちを進める（スクリーンショット用は false で止める）
        from：届いている割合（0..1。再開の時の続き）。負なら状態ごとの見本の値 */
    ModelDownloadDialog (Stage, bool animate, float from = -1.0f);
    /** 本物（B16）：live の状態（UiSession の modelDl）を表示し、キーで本当に始める・止める */
    ModelDownloadDialog (Stage, UiSession& live);
    ~ModelDownloadDialog() override;

    /** 次の状態へ（開き直してもらう）。progress：届いている割合 */
    std::function<void (Stage, float progress)> onStage;

    void visibilityChanged() override;
    void parentHierarchyChanged() override;

protected:
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    bool advanceAnimation (float dt) override;
    void simulate (float dt);
    void handOff (Stage next);
    void paintLadder (juce::Graphics&, juce::Rectangle<float>);
    void paintTally (juce::Graphics&, juce::Rectangle<float>& headingRow, const juce::String& text);

    Stage stage;
    bool animate;
    UiSession* live = nullptr;
    void readLive();
    void build (float from);
    double modelMB = 210.0;
    juce::String modelName { "BS-RoFormer ft1 + BS-RoFormer karaoke + RMVPE" }, modelLicense { "GPL-3.0 / MIT" };
    bool verifying = false;     // ダウンロード中の最後：照合（青が走る）
    bool handedOff = false;

    // 進み具合（MB）。got は届いた量、shown は表示（なめらかに追う）
    double gotMB = 0.0, shownMB = 0.0;
    double speed = 8.4, chunkMB = 0.0, chunkDt = 0.2, nextChunk = 0.0;
    int remainShown = -1;
    double remainChangedAt = 0.0;
    float verify = 0.0f;        // 0..1：照合した所
    float waitLeft = 3.0f;      // 再開までの秒
    int retry = 1;

    double clock = 0.0;         // 明滅の位相
    float flash = 0.0f;         // 完了で一度光る
    double shakeT = -1.0;       // 失敗で揺れる
    juce::Random rng { 7 };
};
} // namespace vb
