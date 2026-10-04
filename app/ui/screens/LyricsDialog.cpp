#include "LyricsDialog.h"

namespace vb
{
namespace
{
    constexpr int maxFileBytes = 4 * 1024 * 1024;   // 歌詞のファイルとしては十分（song::loadLyricsFile と同じ）

    juce::String encodingDisplay (song::TextEncoding e)
    {
        switch (e)
        {
            case song::TextEncoding::utf8:
            case song::TextEncoding::utf8Bom:  return "UTF-8";
            case song::TextEncoding::utf16le:
            case song::TextEncoding::utf16be:  return "UTF-16";
            case song::TextEncoding::shiftJis: return "Shift_JIS";
        }
        return "UTF-8";
    }
}

bool LyricsDialog::isLyricsFile (const juce::File& f)
{
    return f.hasFileExtension ("txt;lrc");
}

LyricsDialog::LyricsDialog (UiSession& u)
    : DialogPanel (tr ("lyrics.dialog.title"), tr ("lyrics.dialog.micro")), SessionView (u),
      openKey (tr ("lyrics.dialog.open")), sectionsKey (tr ("lyrics.dialog.toSections"))
{
    text.setMultiLine (true, false);
    text.setReturnKeyStartsNewLine (true);
    text.setScrollbarsShown (true);
    text.setIndents (12, 10);
    text.setFont (sansIn (i18n::Language::ja, 14.5f));   // かなの字形（歌詞はデータ。DESIGN 10.1）
    text.setTextToShowWhenEmpty (tr ("lyrics.dialog.placeholder"), colours::textMute);
    text.onTextChange = [this] { reparse(); };
    addAndMakeVisible (text);

    openKey.withIcon (Icon::folder);
    openKey.setTooltip (tr ("lyrics.dialog.open.tooltip"));
    openKey.onClick = [this] { chooseFile(); };
    addAndMakeVisible (openKey);

    sectionsKey.withLed().withToggle (true).withFont (sans (12.0f, Weight::medium));
    sectionsKey.setTooltip (tr ("lyrics.dialog.toSections.tooltip"));
    addAndMakeVisible (sectionsKey);

    // 右から：使う / キャンセル / 歌詞を消す（読み込んである時だけ）
    addFooterKey (tr ("lyrics.dialog.apply"), KeyRole::primary, [this] { apply(); });
    addFooterKey (tr ("common.cancel"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });
    auto& clearKey = addFooterKey (tr ("lyrics.dialog.clear"), KeyRole::danger, [this]
    {
        session.clearLyrics();
        if (onApplied) onApplied (tr ("lyrics.cleared"));
    });
    clearKey.setVisible (! u->project.lyrics.empty());

    // いまの歌詞を出す（時刻は [mm:ss.xxx]、見出しは【】の行）
    const auto& ly = u->project.lyrics;
    sectionsKey.setToggleState (ly.empty() || ly.sectionsFromHeadings, juce::dontSendNotification);
    if (! ly.empty())
    {
        fileName = ly.sourceFileName;
        encodingLabel = ly.encoding;
        text.setText (song::toLyricText (ly, u->sampleRate()), false);
    }

    setSize (920, 660);
    reparse();
}

void LyricsDialog::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("lyrics.dialog.choose"),
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                   "*.txt;*.lrc");
    juce::Component::SafePointer<LyricsDialog> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f != juce::File())
                                  safe->loadFile (f);
                          });
}

void LyricsDialog::loadFile (const juce::File& f)
{
    error = {};
    fileBytes.reset();
    if (! f.existsAsFile() || f.getSize() > maxFileBytes || ! f.loadFileAsData (fileBytes))
    {
        error = tr ("lyrics.dialog.readError", f.getFileName());
        repaint();
        return;
    }

    fileName = f.getFileName();
    detected = song::detectEncoding (fileBytes.getData(), fileBytes.getSize());
    rebuildEncodingPicker();
    decodeWith (detected);
}

void LyricsDialog::rebuildEncodingPicker()
{
    // UTF-8 / Shift_JIS は選び直せる。UTF-16 は印（BOM）がある時だけ
    encodingChoices.clear();
    const bool bom8 = detected == song::TextEncoding::utf8Bom;
    encodingChoices.push_back (bom8 ? song::TextEncoding::utf8Bom : song::TextEncoding::utf8);
    if (detected == song::TextEncoding::utf16le || detected == song::TextEncoding::utf16be)
        encodingChoices.push_back (detected);
    encodingChoices.push_back (song::TextEncoding::shiftJis);

    juce::StringArray items;
    int selected = 0;
    for (size_t i = 0; i < encodingChoices.size(); ++i)
    {
        const auto e = encodingChoices[i];
        items.add (e == detected ? tr ("lyrics.dialog.encodingAuto", encodingDisplay (e)) : encodingDisplay (e));
        if (e == detected)
            selected = (int) i;
    }

    encodingPicker = std::make_unique<Dropdown> (items, selected);
    encodingPicker->setFont (mono (12.0f, Weight::medium));
    encodingPicker->setTitle (tr ("lyrics.dialog.encoding"));   // 読み上げの名前（#28）
    encodingPicker->onChange = [this] (int i)
    {
        if (juce::isPositiveAndBelow (i, (int) encodingChoices.size()))
            decodeWith (encodingChoices[(size_t) i]);
    };
    addAndMakeVisible (*encodingPicker);
    resized();
}

void LyricsDialog::decodeWith (song::TextEncoding e)
{
    current = e;
    encodingLabel = song::encodingName (e);
    const auto decoded = song::decodeText (fileBytes.getData(), fileBytes.getSize(), e);
    text.setFont (sansFor (decoded, 14.5f));
    text.setText (decoded, false);
    text.applyFontToAllText (sansFor (decoded, 14.5f));
    reparse();
}

void LyricsDialog::reparse()
{
    doc = song::parseLyrics (text.getText());
    const bool canSection = ! doc.sections.isEmpty() || ! doc.chorusCandidateBlocks.isEmpty();
    sectionsKey.setEnabled (canSection);
    repaint();
}

void LyricsDialog::apply()
{
    if (doc.lines.isEmpty())
    {
        // 空にした：歌詞を消す
        if (! state().project.lyrics.empty())
        {
            session.clearLyrics();
            if (onApplied) onApplied (tr ("lyrics.cleared"));
        }
        else if (onCloseRequest)
            onCloseRequest();
        return;
    }

    const auto& s = state();
    auto ly = song::lyricsFromDoc (doc, s.sampleRate(), s.project.lengthSamples);
    ly.sourceFileName = fileName;
    ly.encoding = encodingLabel.isNotEmpty() ? encodingLabel : juce::String (song::encodingName (song::TextEncoding::utf8));
    ly.sectionsFromHeadings = sectionsKey.isEnabled() && sectionsKey.getToggleState();

    const auto before = s.project.sections.size();
    const auto timed = ly.numTimed();
    const auto total = (int) ly.lines.size();
    session.setLyrics (std::move (ly));
    const auto added = (int) (state().project.sections.size() - before);

    juce::String msg = timed == 0 ? tr ("lyrics.applied.untimed", total) : tr ("lyrics.applied.timed", total, timed);
    if (added > 0)
        msg << "  " << tr ("lyrics.applied.sections", added);
    if (onApplied) onApplied (msg);
}

//==============================================================================
void LyricsDialog::layoutBody (juce::Rectangle<int> r)
{
    auto centreH = [] (juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre (a.getWidth(), h); };

    toolbar = r.removeFromTop (36);
    {
        auto row = toolbar;
        openKey.setSize (10, 32);
        openKey.setBounds (centreH (row.removeFromLeft (openKey.idealWidth()), 32));
        row.removeFromLeft (16);

        if (encodingPicker != nullptr)
        {
            const auto w = juce::jmax (150, encodingPicker->idealWidth());
            encodingPicker->setBounds (centreH (row.removeFromRight (w), 30));
            encodingLabelArea = row.removeFromRight (textWidth (sans (12.0f), tr ("lyrics.dialog.encoding")) + 12);
        }
        else
            encodingLabelArea = {};
        fileArea = row;
    }
    r.removeFromTop (10);

    hintArea = r.removeFromBottom (20);
    r.removeFromBottom (6);
    {
        auto row = r.removeFromBottom (32);
        sectionsKey.setSize (10, 30);
        sectionsKey.setBounds (centreH (row.removeFromRight (sectionsKey.idealWidth()), 30));
        summaryArea = row;
    }
    r.removeFromBottom (8);
    text.setBounds (r);
}

void LyricsDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    // ファイル名（データ）
    g.setColour (fileName.isNotEmpty() ? colours::textDim : colours::textMute);
    g.setFont (sansFor (fileName, 12.0f));
    g.drawText (fileName.isNotEmpty() ? fileName : tr ("lyrics.dialog.noFile"), fileArea, juce::Justification::centredLeft, true);

    if (! encodingLabelArea.isEmpty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (12.0f));
        g.drawText (tr ("lyrics.dialog.encoding"), encodingLabelArea, juce::Justification::centredLeft, false);
    }

    // 中身のまとめ：行数・時刻・見出し・サビの候補
    {
        auto r = summaryArea.toFloat();
        const auto f = sans (12.0f);
        const bool ok = error.isEmpty();
        const auto lines = doc.lines.size();
        const auto c = ! ok ? colours::bad : lines > 0 ? colours::signal : colours::textMute;
        paint::led (g, { r.getX() + 5.0f, r.getCentreY() }, 2.8f, c, lines > 0 || ! ok);
        r.removeFromLeft (16.0f);

        juce::String summary;
        if (! ok)
            summary = error;
        else if (lines == 0)
            summary = tr ("lyrics.dialog.summary.empty");
        else
        {
            int timedLines = 0;
            for (auto& l : doc.lines)
                timedLines += l.hasTime() ? 1 : 0;
            summary = tr ("lyrics.dialog.summary.lines", lines);
            summary << "  /  " << (timedLines > 0 ? tr ("lyrics.dialog.summary.timed", timedLines) : tr ("lyrics.dialog.summary.untimed"));
            if (! doc.sections.isEmpty())
                summary << "  /  " << tr ("lyrics.dialog.summary.headings", doc.sections.size());
            if (! doc.chorusCandidateBlocks.isEmpty())
                summary << "  /  " << tr ("lyrics.dialog.summary.chorus", doc.chorusCandidateBlocks.size());
        }
        g.setColour (ok ? colours::textDim : colours::bad);
        g.setFont (f);
        g.drawText (summary, r, juce::Justification::centredLeft, true);
    }

    g.setColour (colours::textMute);
    g.setFont (sans (11.0f));
    g.drawText (tr ("lyrics.dialog.hint"), hintArea, juce::Justification::centredLeft, true);
}
} // namespace vb
