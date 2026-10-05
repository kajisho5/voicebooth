#include "SongInfoDialog.h"
#include "../SongMarks.h"
#include "../Timeline.h"

namespace vb
{
namespace
{
    constexpr int dialogW = 560, dialogH = 800;
    constexpr int headerRowH = 26, rowH = 34, labelW = 92, gap = 14;

    juce::StringArray tonicItems()
    {
        juce::StringArray a { tr ("songInfo.key.unknown") };
        for (int i = 0; i < 12; ++i)
            a.add (song::tonicName (i));   // 音名は記号（翻訳しない）
        return a;
    }

    juce::StringArray signatureItems()
    {
        juce::StringArray a;
        for (auto& t : song::timeSignatures())
            a.add (t.toString());
        return a;
    }

    int signatureIndex (song::TimeSignature sig)
    {
        const auto all = song::timeSignatures();
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i] == sig)
                return i;
        return 0;
    }

    /** 見出しの右端：確定 / 推定 / 未設定（LED と言葉） */
    void drawSourceChip (juce::Graphics& g, juce::Rectangle<int> header, bool known, song::Source source)
    {
        const auto text = ! known ? tr ("songInfo.unset")
                        : source == song::Source::confirmed ? tr ("songInfo.confirmed") : tr ("songInfo.estimated");
        const auto c = ! known ? colours::textMute : source == song::Source::confirmed ? colours::signal : colours::ref;
        const auto f = sans (11.0f, Weight::medium);
        auto r = header.toFloat();
        auto chip = r.removeFromRight (textWidth (f, text) + 16.0f);
        paint::led (g, { chip.getX() + 4.0f, chip.getCentreY() }, 2.6f, c, known);
        g.setColour (known ? colours::textDim : colours::textMute);
        g.setFont (f);
        g.drawText (text, chip.withTrimmedLeft (12.0f), juce::Justification::centredLeft, false);
    }

    void drawLabel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text)
    {
        g.setColour (colours::textDim);
        g.setFont (sans (12.0f));
        g.drawText (text, r, juce::Justification::centredLeft, true);
    }
}

//==============================================================================
SectionRow::SectionRow (UiSession& u, Actions& a, int i) : session (u), actions (a), index (i)
{
    const auto& list = u->project.sections;
    nameKey.setButtonText (marks::sectionName (list, index));
    nameKey.withIcon (Icon::chevronDown).withFont (sans (12.5f, Weight::medium));
    nameKey.setTooltip (tr ("songInfo.section.name.tooltip"));
    nameKey.onClick = [this] { marks::showNameMenu (session, actions, index, nameKey.getScreenBounds()); };

    loopKey.withIcon (Icon::loop);
    loopKey.setTooltip (tr ("section.menu.loop"));
    loopKey.onClick = [this] { session.loopSection (index); };

    removeKey.withIcon (Icon::close);
    removeKey.setTooltip (tr ("section.menu.remove"));
    removeKey.onClick = [this] { session.removeSection (index); };

    for (auto* k : { &nameKey, &loopKey, &removeKey })
        addAndMakeVisible (k);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SectionRow::resized()
{
    auto r = getLocalBounds().reduced (0, 3);
    removeKey.setBounds (r.removeFromRight (26).withSizeKeepingCentre (26, 26));
    r.removeFromRight (4);
    loopKey.setBounds (r.removeFromRight (26).withSizeKeepingCentre (26, 26));
    r.removeFromRight (8);
    posArea = r.removeFromLeft (128);
    nameKey.setSize (10, 26);
    nameKey.setBounds (r.withWidth (juce::jmin (r.getWidth(), nameKey.idealWidth())).withSizeKeepingCentre (juce::jmin (r.getWidth(), nameKey.idealWidth()), 26));
}

void SectionRow::paint (juce::Graphics& g)
{
    const auto& s = session.get();
    if (! juce::isPositiveAndBelow (index, (int) s.project.sections.size()))
        return;
    const auto& sec = s.project.sections[(size_t) index];
    const bool selected = index == s.selectedSection;

    if (selected || isMouseOver (true))
    {
        g.setColour (selected ? colours::signal.withAlpha (0.08f) : colours::highlight (0.03f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.0f, 1.0f), 3.0f);
    }

    auto r = posArea.toFloat();
    // LED：確定は点灯、推定は薄く（DESIGN 7.5）
    const bool confirmed = sec.source == song::Source::confirmed;
    paint::led (g, { r.getX() + 9.0f, r.getCentreY() }, 2.6f, confirmed ? colours::signal : colours::ref, confirmed);
    r.removeFromLeft (20.0f);

    g.setFont (mono (12.0f, Weight::medium));
    g.setColour (selected ? colours::signal : colours::text);
    const auto bar = marks::positionText (s, sec.startSample);
    g.drawText (bar, r.removeFromLeft (44.0f), juce::Justification::centredLeft, false);
    g.setColour (colours::textMute);
    g.setFont (mono (10.5f));
    g.drawText (s.tempoKnown() ? formatTime (sec.startSample, s.sampleRate(), false) : juce::String(),
                r, juce::Justification::centredLeft, false);
}

void SectionRow::mouseUp (const juce::MouseEvent& e)
{
    // 行の左（位置）を押す：その区間の頭へ
    if (! e.mouseWasDraggedSinceMouseDown() && posArea.contains (e.getPosition()))
        session.goToSection (index);
}

//==============================================================================
SongInfoDialog::SongInfoDialog (UiSession& u, Actions& a)
    : DialogPanel (tr ("songInfo.title"), tr ("songInfo.micro")), SessionView (u), actions (a),
      doubleKey (tr ("songInfo.double")), halfKey (tr ("songInfo.half")), tapKey (tr ("songInfo.tap")),
      signature (signatureItems(), signatureIndex (u->project.tempo.signature)),
      downbeatKey (tr ("songInfo.downbeat.here")), earlierKey (tr ("songInfo.downbeat.earlier")), laterKey (tr ("songInfo.downbeat.later")),
      tonic (tonicItems(), u->project.key.tonic + 1),
      keyMode ({ tr ("songInfo.key.major"), tr ("songInfo.key.minor") }, u->project.key.minor ? 1 : 0),
      addSectionKey (tr ("songInfo.section.add"))
{
    // テンポ
    styleField (bpmField);
    bpmField.setFont (mono (17.0f, Weight::semibold));
    bpmField.setInputRestrictions (6, "0123456789.,");
    bpmField.setTextToShowWhenEmpty ("-", colours::textMute);
    bpmField.onReturnKey = [this] { commitBpm(); grabKeyboardFocus(); };
    bpmField.onEscapeKey = [this] { refresh(); grabKeyboardFocus(); };
    bpmField.onFocusLost = [this] { commitBpm(); };
    bpmField.setTooltip (tr ("songInfo.bpm.tooltip"));
    addAndMakeVisible (bpmField);

    doubleKey.setTooltip (tr ("songInfo.double.tooltip"));
    halfKey.setTooltip (tr ("songInfo.half.tooltip"));
    doubleKey.onClick = [this] { session.doubleBpm(); };
    halfKey.onClick = [this] { session.halveBpm(); };

    tapKey.withLed().withToggle (false);
    tapKey.setTooltip (tr ("songInfo.tap.tooltip"));
    tapKey.withShortcut (session.get().shortcuts.keyName (shortcuts::Action::tapTempo));   // 設定で変えられる（#28）
    tapKey.onClick = [this] { tap(); };

    clearTempoKey.setButtonText (tr ("songInfo.clearTempo"));
    clearTempoKey.withFont (sans (11.5f, Weight::medium));
    clearTempoKey.setTooltip (tr ("songInfo.clearTempo.tooltip"));
    clearTempoKey.onClick = [this] { session.setBpm (0.0); };

    signature.setFont (mono (12.0f, Weight::medium));
    signature.onChange = [this] (int i) { session.setTimeSignature (song::timeSignatures()[(size_t) i]); };

    downbeatKey.setTooltip (tr ("songInfo.downbeat.tooltip"));
    downbeatKey.onClick = [this] { session.setDownbeatAtPlayhead(); };
    earlierKey.setTooltip (tr ("songInfo.downbeat.shift.tooltip"));
    laterKey.setTooltip (tr ("songInfo.downbeat.shift.tooltip"));
    earlierKey.onClick = [this] { session.shiftDownbeat (-1); };
    laterKey.onClick = [this] { session.shiftDownbeat (1); };

    // キー
    tonic.onChange = [this] (int i) { session.setSongKey (i - 1, keyMode.getSelected() == 1); };
    keyMode.onChange = [this] (int i) { session.setSongKey (state().project.key.tonic, i == 1); };
    // 読み上げの名前（#28）
    signature.setTitle (tr ("songInfo.signature"));
    tonic.setTitle (tr ("songInfo.key"));
    keyMode.setTitle (tr ("songInfo.key.major") + " / " + tr ("songInfo.key.minor"));

    // 区間
    addSectionKey.withIcon (Icon::plus);
    addSectionKey.setTooltip (tr ("songInfo.section.add.tooltip"));
    addSectionKey.withShortcut (session.get().shortcuts.keyName (shortcuts::Action::addSection));
    addSectionKey.onClick = [this] { session.addSectionAtPlayhead(); };

    sectionView.setViewedComponent (&sectionList, false);
    sectionView.setScrollBarsShown (true, false);
    sectionView.setScrollBarThickness (8);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &doubleKey, &halfKey, &tapKey, &clearTempoKey, &signature, &downbeatKey, &earlierKey, &laterKey,
             &tonic, &keyMode, &addSectionKey, &sectionView })
        addAndMakeVisible (c);

    addFooterKey (tr ("common.close"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });

    setSize (dialogW, dialogH);
    refresh();
    handleAsyncUpdate();
}

SongInfoDialog::~SongInfoDialog()
{
    // 消える途中で入力欄がフォーカスを失っても、もう無い部品に触らない
    bpmField.onFocusLost = nullptr;
    cancelPendingUpdate();
}

void SongInfoDialog::styleField (juce::TextEditor& f)
{
    f.setIndents (10, 6);
    f.setJustification (juce::Justification::centredLeft);
    f.setSelectAllWhenFocused (true);
    f.setScrollbarsShown (false);
}

void SongInfoDialog::onSessionChanged (juce::uint32 c)
{
    if (c & (change::songInfo | change::song))
    {
        refresh();
        triggerAsyncUpdate();
    }
}

void SongInfoDialog::refresh()
{
    const auto& s = state();
    const auto& t = s.project.tempo;

    if (! bpmField.hasKeyboardFocus (true))
        bpmField.setText (t.known() ? song::formatBpm (t.bpm) : juce::String(), false);

    for (auto* k : { &doubleKey, &halfKey, &downbeatKey, &earlierKey, &laterKey })
        k->setEnabled (t.known());
    clearTempoKey.setVisible (t.known());
    signature.setSelected (signatureIndex (t.signature), juce::dontSendNotification);

    tonic.setSelected (s.project.key.tonic + 1, juce::dontSendNotification);
    keyMode.setSelected (s.project.key.minor ? 1 : 0, juce::dontSendNotification);
    keyMode.setEnabled (s.project.key.known());
    repaint();
}

void SongInfoDialog::handleAsyncUpdate()
{
    const auto& list = state().project.sections;
    rows.clear();
    for (int i = 0; i < (int) list.size(); ++i)
        sectionList.addAndMakeVisible (rows.add (new SectionRow (session, actions, i)));

    const auto w = sectionView.getMaximumVisibleWidth() > 0 ? sectionView.getMaximumVisibleWidth() : sectionView.getWidth();
    sectionList.setSize (juce::jmax (1, w), (int) list.size() * SectionRow::height);
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, i * SectionRow::height, sectionList.getWidth(), SectionRow::height);

    // 選んだ区間が見えるように
    const auto sel = state().selectedSection;
    if (juce::isPositiveAndBelow (sel, rows.size()))
    {
        const auto y = sel * SectionRow::height;
        const auto top = sectionView.getViewPositionY();
        if (y < top || y + SectionRow::height > top + sectionView.getHeight())
            sectionView.setViewPosition (0, juce::jmax (0, y - sectionView.getHeight() / 2));
    }
    repaint();
}

void SongInfoDialog::commitBpm()
{
    const auto text = bpmField.getText().trim();
    if (text.isEmpty())
    {
        refresh();
        return;
    }
    const auto bpm = song::parseBpm (text);
    if (bpm > 0.0 && std::abs (bpm - state().bpm()) > 0.0001)
        session.setBpm (bpm);
    else
        refresh();   // 範囲外・読めない：元の値に戻す
}

void SongInfoDialog::tap()
{
    tapKey.flash();
    session.tapTempo (juce::Time::getMillisecondCounterHiRes() / 1000.0);
}

bool SongInfoDialog::keyPressed (const juce::KeyPress& key)
{
    // パネルを開いたままでも：Space 再生 / 一時停止、タップ、区間の頭（キーは設定の割り当てどおり。#28。バグチェック 2026-10-05）
    if (key == juce::KeyPress::spaceKey)
    {
        session.setPlaying (! state().isPlaying);
        return true;
    }
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
        return false;
    const auto& map = state().shortcuts;
    auto typed = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    // Alt / Option と一緒だと別の文字になる（Mac の Option+M は µ）：キーの場所で見る（メイン画面と同じ）
    if (! map.actionFor (typed) && key.getModifiers().isAltDown())
        typed = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) key.getKeyCode());
    const auto action = map.actionFor (typed);
    if (action == shortcuts::Action::tapTempo)
    {
        tap();
        return true;
    }
    if (action == shortcuts::Action::addSection)
    {
        addSectionKey.flash();
        session.addSectionAtPlayhead (! key.getModifiers().isAltDown());
        return true;
    }
    return false;
}

//==============================================================================
void SongInfoDialog::layoutBody (juce::Rectangle<int> r)
{
    auto centreH = [] (juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre (a.getWidth(), h); };
    auto place = [&] (juce::Rectangle<int>& row, KeyButton& k, int h = 30)
    {
        k.setSize (10, h);
        const auto w = k.idealWidth();
        k.setBounds (centreH (row.removeFromLeft (w), h));
        row.removeFromLeft (6);
    };

    // テンポ
    tempoHeader = r.removeFromTop (headerRowH);
    {
        auto row = r.removeFromTop (rowH + 6);
        bpmLabel = row.removeFromLeft (labelW);
        bpmField.setBounds (centreH (row.removeFromLeft (104), 34));
        row.removeFromLeft (10);
        place (row, halfKey);
        place (row, doubleKey);
        row.removeFromLeft (6);
        place (row, tapKey);
    }
    tapHint = r.removeFromTop (20).withTrimmedLeft (labelW);
    r.removeFromTop (4);
    {
        auto row = r.removeFromTop (rowH);
        sigLabel = row.removeFromLeft (labelW);
        signature.setBounds (centreH (row.removeFromLeft (juce::jmax (150, signature.idealWidth())), 30));
        clearTempoKey.setSize (10, 26);
        clearTempoKey.setBounds (centreH (row.removeFromRight (clearTempoKey.idealWidth()), 26));
    }
    r.removeFromTop (6);
    {
        auto row = r.removeFromTop (rowH);
        downbeatLabel = row.removeFromLeft (labelW);
        downbeatValue = row.removeFromLeft (92);
        place (row, downbeatKey);
    }
    {
        auto row = r.removeFromTop (rowH).withTrimmedLeft (labelW + 92);
        place (row, earlierKey, 26);
        place (row, laterKey, 26);
    }

    // キー
    r.removeFromTop (gap);
    keyHeader = r.removeFromTop (headerRowH);
    {
        auto row = r.removeFromTop (rowH + 4);
        keyLabel = row.removeFromLeft (labelW);
        tonic.setBounds (centreH (row.removeFromLeft (juce::jmax (120, tonic.idealWidth())), 32));
        row.removeFromLeft (8);
        keyMode.setBounds (centreH (row.removeFromLeft (juce::jmax (160, keyMode.idealWidth())), 30));
    }

    // 区間
    r.removeFromTop (gap);
    {
        auto row = r.removeFromTop (headerRowH + 4);
        sectionHeader = row;
        addSectionKey.setSize (10, 26);
        addSectionKey.setBounds (centreH (row.removeFromRight (addSectionKey.idealWidth()), 26));
        sectionHeader.setRight (addSectionKey.getX() - 8);
    }
    r.removeFromTop (4);
    hintArea = r.removeFromBottom (34);
    sectionView.setBounds (r);
    emptyArea = r;
    handleAsyncUpdate();
}

void SongInfoDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    const auto& s = state();
    const auto& t = s.project.tempo;

    paint::sectionHeader (g, tempoHeader, tr ("songInfo.tempo"), tr ("songInfo.tempo.sub"));
    drawSourceChip (g, tempoHeader, t.known(), t.source);
    drawLabel (g, bpmLabel, tr ("songInfo.bpm"));
    drawLabel (g, sigLabel, tr ("songInfo.signature"));
    drawLabel (g, downbeatLabel, tr ("songInfo.downbeat"));

    // タップの様子：叩いた回数 / 出た BPM（ばらつき）
    {
        const auto n = session.tapCount();
        juce::String hint;
        if (n > 0 && n < song::TapTempo::minTaps)
            hint = tr ("songInfo.tap.count", n, song::TapTempo::minTaps);
        else if (n >= song::TapTempo::minTaps && t.known())
            hint = tr ("songInfo.tap.result", song::formatBpm (t.bpm), n);
        else
            hint = tr ("songInfo.bpm.hint");
        g.setColour (colours::textMute);
        g.setFont (sans (11.0f));
        g.drawText (hint, tapHint, juce::Justification::centredLeft, true);
    }

    // 1 小節目の頭（位置そのもの）
    g.setColour (t.known() ? colours::text : colours::textMute);
    g.setFont (mono (13.0f, Weight::medium));
    g.drawText (t.known() ? formatTime (t.downbeatSample, s.sampleRate(), true) : juce::String ("-"),
                downbeatValue, juce::Justification::centredLeft, false);

    paint::sectionHeader (g, keyHeader, tr ("songInfo.key"), tr ("songInfo.key.sub"));
    drawSourceChip (g, keyHeader, s.project.key.known(), s.project.key.source);
    drawLabel (g, keyLabel, tr ("songInfo.key.label"));

    paint::sectionHeader (g, sectionHeader, tr ("songInfo.sections"), tr ("songInfo.sections.sub", (int) s.project.sections.size()));

    if (s.project.sections.empty())
    {
        g.setColour (colours::textMute);
        g.setFont (sans (12.5f));
        g.drawFittedText (tr ("songInfo.sections.empty"), emptyArea.reduced (4, 10), juce::Justification::topLeft, 3, 1.0f);
    }

    g.setColour (colours::textMute);
    g.setFont (sans (11.0f));
    g.drawFittedText (tr ("songInfo.sections.hint"), hintArea, juce::Justification::centredLeft, 2, 1.0f);
}

//==============================================================================
SectionNameDialog::SectionNameDialog (UiSession& u, int i)
    : DialogPanel (tr ("sectionName.title"), tr ("sectionName.micro")), session (u), index (i)
{
    field.setFont (sans (15.0f));
    field.setIndents (10, 8);
    field.setJustification (juce::Justification::centredLeft);
    field.setInputRestrictions (24);
    field.setTextToShowWhenEmpty (tr ("sectionName.placeholder"), colours::textMute);
    const auto& list = u->project.sections;
    if (juce::isPositiveAndBelow (index, (int) list.size()) && list[(size_t) index].isCustom())
        field.setText (list[(size_t) index].name, false);
    field.onReturnKey = [this] { commit(); };
    addAndMakeVisible (field);

    addFooterKey (tr ("sectionName.ok"), KeyRole::primary, [this] { commit(); });
    addFooterKey (tr ("common.cancel"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });

    setSize (440, headerH + 14 + 84 + footerH + 6);
}

void SectionNameDialog::layoutBody (juce::Rectangle<int> r)
{
    field.setBounds (r.removeFromTop (36));
}

void SectionNameDialog::paintBody (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawFittedText (tr ("sectionName.hint"), r.withTrimmedTop (44), juce::Justification::topLeft, 2, 1.0f);
}

void SectionNameDialog::commit()
{
    const auto name = field.getText().trim();
    if (name.isNotEmpty())
        session.renameSection (index, song::kind::custom, name);
    if (onDone)
        onDone();
}
} // namespace vb
