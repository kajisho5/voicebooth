#pragma once

#include "../Overlay.h"
#include "../UiSession.h"
#include "../parts/Dropdown.h"

/*  歌詞パッド（B4b。DESIGN 4.4 / 7.5.3）：.txt / .lrc の読み込み・貼り付け・修正
    - 文字コードは自動判定（UTF-8 / UTF-16 / Shift_JIS）。化けたら選び直せる
    - 1 行＝1 フレーズ、空行＝区切り。【サビ】のような見出しの行は区間の名前に、同じ歌詞のくり返しはサビの候補に
    - 時刻を合わせた歌詞は [mm:ss.xxx] 付きで出す（直しても時刻が残る）
    - 読むだけ。どこにも送らない */

namespace vb
{
class LyricsDialog : public DialogPanel, private SessionView
{
public:
    explicit LyricsDialog (UiSession&);

    /** ファイルを読む（ドロップ・ファイル選択・--lyrics=） */
    void loadFile (const juce::File&);

    /** 「使う」の後（トーストに出す文） */
    std::function<void (const juce::String&)> onApplied;

    static bool isLyricsFile (const juce::File&);

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override {}
    void chooseFile();
    void reparse();
    void apply();
    void decodeWith (song::TextEncoding);
    void rebuildEncodingPicker();

    juce::TextEditor text;
    KeyButton openKey;
    std::unique_ptr<Dropdown> encodingPicker;   // 読んだファイルの時だけ（選べる文字コードはファイルで変わる）
    KeyButton sectionsKey;

    juce::MemoryBlock fileBytes;            // 読んだファイル（文字コードを選び直す時に読み直す）
    juce::String fileName, encodingLabel;
    song::TextEncoding detected = song::TextEncoding::utf8, current = song::TextEncoding::utf8;
    std::vector<song::TextEncoding> encodingChoices;
    song::LyricsDoc doc;
    juce::String error;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> toolbar, fileArea, encodingLabelArea, summaryArea, hintArea;
};
} // namespace vb
