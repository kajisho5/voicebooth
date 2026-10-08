#pragma once

#include "../Overlay.h"
#include "../UiSession.h"

namespace vb
{
/** DESIGN 9 書き出し（静的モック）。ファイル名・形式・警告・パック内容を確認してから書き出す
    実際の書き出しは ExportService（Phase B）。UI からは wav を書かない */
class ExportDialog : public DialogPanel, private SessionView
{
public:
    explicit ExportDialog (UiSession&);

    std::function<void()> onExport;
    std::function<void()> onShareVideo;   // 共有用の動画（DESIGN 9.1）を開く

    /** 書き出すトラック（チェックが入っていて、録ってあるボーカル）。確認用ミックスは B15 */
    std::vector<project::TrackType> selectedTracks() const;

    /** 書き出すビット数（16 / 24 / 32 = float）。既定は録音形式と同じ */
    int selectedBitDepth() const;

    /** 納品パック（B15）を選んでいる（簡単モードは個別 WAV だけ） */
    bool packSelected() const;
    /** 納品パックに確認用ミックスを入れる */
    bool refmixSelected() const;

    /** 選んだもの（「未録音の箇所」の確認から［戻る］で開き直すときに戻す） */
    struct Choice
    {
        std::vector<project::TrackType> tracks;
        int bits = 24;
        bool pack = false, refmix = false;
    };
    Choice choice() const { return { selectedTracks(), selectedBitDepth(), packSelected(), refmixSelected() }; }
    void restore (const Choice&);

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override {}

    struct FileRow
    {
        project::TrackType type;
        juce::String file;
        bool available;     // 録音がある
        bool clip;          // 採用区間にクリップ
        juce::String peak;
        juce::String clipTakes;   // クリップしたテイク（"take2, take4"。曲を開いた時）
        bool packOnly = false;    // 納品パックにだけ入る（確認用ミックス）
        juce::String packFile;    // パックの中の名前（vocal_dry.wav など）
        bool hiddenByMode = false;   // いまのモードでは隠れているが、録ってある（入れる。監査 2026-10-04）
    };

    std::vector<FileRow> rows;
    juce::OwnedArray<KeyButton> checks;
    SegmentedKeys packMode;
    SegmentedKeys bitKeys;   // 16bit（ディザー）/ 24bit / 32bit float
    juce::Rectangle<int> listArea, packArea, previewArea, formatArea, destArea;
};
} // namespace vb
