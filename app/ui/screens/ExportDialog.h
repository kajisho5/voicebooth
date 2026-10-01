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
    };

    std::vector<FileRow> rows;
    juce::OwnedArray<KeyButton> checks;
    SegmentedKeys packMode;
    juce::Rectangle<int> listArea, packArea, previewArea, formatArea, destArea;
};
} // namespace vb
