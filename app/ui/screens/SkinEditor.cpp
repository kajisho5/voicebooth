#include "SkinEditor.h"

namespace vb
{
namespace
{
    using skin::Token;

    constexpr int fieldH = 30, swatchH = 28, groupHeaderH = 24, swatchColW = 220, colGap = 16, pickerW = 224;

    /** 色の名前（画面の言葉で） */
    const char* tokenNameKey (Token t)
    {
        static const char* keys[skin::numTokens] = {
            "skinEditor.token.bg0", "skinEditor.token.bgDeep", "skinEditor.token.panel", "skinEditor.token.raised",
            "skinEditor.token.raisedHi", "skinEditor.token.grid", "skinEditor.token.line", "skinEditor.token.lineHi",
            "skinEditor.token.text", "skinEditor.token.textDim", "skinEditor.token.textMute",
            "skinEditor.token.signal", "skinEditor.token.ref", "skinEditor.token.warn", "skinEditor.token.bad", "skinEditor.token.rec"
        };
        return keys[(int) t];
    }

    struct GroupInfo { const char* label; std::vector<Token> tokens; };

    /** 背景 / 線 / 文字 / 意味の色（左の列に背景と線、右の列に文字と意味の色） */
    const std::vector<GroupInfo>& groups()
    {
        static const std::vector<GroupInfo> g {
            { "skinEditor.group.background", { Token::bg0, Token::bgDeep, Token::panel, Token::raised, Token::raisedHi } },
            { "skinEditor.group.lines",      { Token::grid, Token::line, Token::lineHi } },
            { "skinEditor.group.text",       { Token::text, Token::textDim, Token::textMute } },
            { "skinEditor.group.meaning",    { Token::signal, Token::ref, Token::warn, Token::bad, Token::rec } },
        };
        return g;
    }

    bool sameGroup (const skin::Skin& a, const skin::Skin& b, int group)
    {
        for (auto t : groups()[(size_t) group].tokens)
            if (a.get (t) != b.get (t))
                return false;
        return true;
    }

    juce::String subtitleOf (const skin::Skin& s)
    {
        if (s.builtIn)
            return tr (skin::subtitleKey (s.id).toRawUTF8());
        return s.author.isNotEmpty() ? s.author : tr ("settings.skin.mine");
    }

    /** メニューに添える色の並び（そのグループの色だけ） */
    std::unique_ptr<juce::Drawable> groupSwatches (const skin::Skin& s, int group)
    {
        const auto& tokens = groups()[(size_t) group].tokens;
        const int cell = 14, h = 14, scale = 2;   // 高解像度の画面でもにじまないよう 2 倍で作る
        juce::Image img (juce::Image::ARGB, (int) tokens.size() * cell * scale, h * scale, true);
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale ((float) scale));
            for (size_t i = 0; i < tokens.size(); ++i)
            {
                const auto r = juce::Rectangle<float> ((float) i * (float) cell, 0.0f, (float) cell - 2.0f, (float) h).reduced (0.0f, 1.0f);
                g.setColour (juce::Colour (s.get (tokens[i])));
                g.fillRoundedRectangle (r, 2.0f);
                g.setColour (colours::lineHi);
                g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
            }
        }
        auto d = std::make_unique<juce::DrawableImage>();
        d->setImage (img);
        d->setBoundingBox (juce::Rectangle<float> (0.0f, 0.0f, (float) (tokens.size() * (size_t) cell), (float) h));
        return d;
    }

    juce::String readErrorText (skin::ReadError e)
    {
        switch (e)
        {
            case skin::ReadError::tooLarge:     return tr ("skinEditor.error.tooLarge");
            case skin::ReadError::notJson:      return tr ("skinEditor.error.notJson");
            case skin::ReadError::notSkin:      return tr ("skinEditor.error.notSkin");
            case skin::ReadError::newerVersion: return tr ("skinEditor.error.newerVersion");
            case skin::ReadError::unreadable:
            case skin::ReadError::none:         break;
        }
        return tr ("skinEditor.error.unreadable");
    }

    juce::File defaultFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    }

    juce::StringArray namesOf (const std::vector<skin::Skin>& list)
    {
        juce::StringArray a;
        for (auto& s : list)
            a.add (s.name);
        return a;
    }
}

//==============================================================================
SkinEditor::SkinEditor (skin::Library& lib, const skin::Skin& active, const skin::Skin& source, bool copy)
    : DialogPanel (tr (copy || source.builtIn ? "skinEditor.titleNew" : "skinEditor.title"), tr ("skinEditor.micro")),
      library (lib), templates (lib.all()), original (active),
      templatePicker (namesOf (templates), 0)
{
    addAndMakeVisible (templatePicker);
    templatePicker.onChange = [this] (int i) { setTemplate (i); };
    templatePicker.setTitle (tr ("skinEditor.template"));   // 読み上げの名前（#28）

    for (auto* f : { &nameField, &authorField, &hexField })
    {
        styleField (*f);
        addAndMakeVisible (f);
    }
    nameField.setInputRestrictions (skin::maxStringLength);
    authorField.setInputRestrictions (skin::maxStringLength);
    authorField.setTextToShowWhenEmpty (tr ("skinEditor.author.empty"), colours::textMute);
    nameField.onTextChange = [this] { working.name = nameField.getText().trim(); };
    authorField.onTextChange = [this] { working.author = authorField.getText().trim(); };

    hexField.setFont (mono (13.0f));
    hexField.setInputRestrictions (7, "#0123456789abcdefABCDEF");
    hexField.onTextChange = [this]
    {
        juce::uint32 c = 0;
        if (skin::parseHex (hexField.getText(), c))
            setTokenColour (selected, c, false, true);
    };
    hexField.onReturnKey = [this] { refreshFields(); };
    hexField.onFocusLost = [this] { refreshFields(); };

    picker.onChange = [this] (juce::Colour c) { setTokenColour (selected, c.getARGB(), true, false); };
    addAndMakeVisible (picker);

    // グループごとの「テンプレート」メニュー（借りる / 戻す）
    for (int g = 0; g < (int) groups().size(); ++g)
    {
        auto* k = groupKeys.add (new KeyButton (tr ("skinEditor.group.template"), KeyButton::Kind::ghost));
        k->withIcon (Icon::chevronDown).withFont (sans (11.5f, Weight::medium));
        k->setTooltip (tr ("skinEditor.group.borrow"));
        k->onClick = [this, g] { showGroupMenu (g); };
        addAndMakeVisible (k);
    }

    // キーは右から：保存 / キャンセル / 書き出し / 読み込み / 削除（自作スキンのみ）
    addFooterKey (tr ("skinEditor.save"), KeyRole::primary, [this] { save(); });
    addFooterKey (tr ("common.cancel"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });
    addFooterKey (tr ("skinEditor.export"), KeyRole::normal, [this] { exportFile(); });
    addFooterKey (tr ("skinEditor.import"), KeyRole::normal, [this] { importFile(); });
    deleteKey = &addFooterKey (tr ("skinEditor.delete"), KeyRole::danger, [this] { deletePressed(); });

    setSize (2 * padding + 2 * swatchColW + colGap + 16 + pickerW, headerH + 14 + 476 + footerH + 6);

    // 内蔵スキンは書き換えない：コピーを作る（テンプレートから作る時も同じ）
    auto start = source;
    if (copy || source.builtIn)
    {
        templateSkin = source;
        start.builtIn = false;
        start.base = source.builtIn ? source.id : source.base;
        start.name = tr ("skinEditor.copyName", source.name);
        start.author = {};
        start.id = library.uniqueId (source.id + "-copy");
        loadWorking (start, false);
    }
    else
    {
        const auto* b = skin::findBuiltIn (source.base);
        templateSkin = b != nullptr ? *b : skin::defaultSkin();
        loadWorking (start, true);
    }
    templatePicker.setSelected (templateIndex (templateSkin.id), juce::dontSendNotification);
    selectToken (Token::signal);
}

SkinEditor::~SkinEditor()
{
    // キャンセル・閉じる・Esc：開く前の色に戻す（保存・削除した時はそのまま）
    if (previewed && ! committed && onPreview)
        onPreview (original);
}

int SkinEditor::templateIndex (const juce::String& id) const
{
    for (size_t i = 0; i < templates.size(); ++i)
        if (templates[i].id == id)
            return (int) i;
    return 0;
}

//==============================================================================
void SkinEditor::styleField (juce::TextEditor& f)
{
    f.setFont (sans (13.0f));
    f.setIndents (8, 6);
    f.setJustification (juce::Justification::centredLeft);
    f.setSelectAllWhenFocused (false);
    f.setScrollbarsShown (false);
}

void SkinEditor::loadWorking (const skin::Skin& s, bool existing)
{
    working = s;
    editingExisting = existing;
    deleteArmed = false;

    deleteKey->setVisible (existing);
    nameField.setText (working.name, false);
    authorField.setText (working.author, false);
    resized();
    coloursChanged();
}

void SkinEditor::selectToken (Token t)
{
    selected = t;
    refreshFields();
    repaint();
}

void SkinEditor::refreshFields()
{
    const auto argb = working.get (selected);
    picker.setCurrentColour (juce::Colour (argb), juce::dontSendNotification);
    hexField.setText (skin::toHex (argb), false);
}

void SkinEditor::coloursChanged()
{
    refreshFields();
    warnings = skin::check (working);
    preview();
    repaint();
}

void SkinEditor::setTokenColour (Token t, juce::uint32 argb, bool fromPicker, bool fromHex)
{
    argb |= 0xff000000u;
    if (working.get (t) == argb)
        return;

    working.set (t, argb);
    if (t == selected)
    {
        if (! fromPicker) picker.setCurrentColour (juce::Colour (argb), juce::dontSendNotification);
        if (! fromHex)    hexField.setText (skin::toHex (argb), false);
    }

    warnings = skin::check (working);
    preview();
    repaint();
}

void SkinEditor::setTemplate (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) templates.size()))
        return;

    const auto& t = templates[(size_t) index];

    // 名前がまだ「○○ のコピー」のままなら、新しいテンプレートに合わせる
    if (! editingExisting && working.name == tr ("skinEditor.copyName", templateSkin.name))
    {
        working.name = tr ("skinEditor.copyName", t.name);
        nameField.setText (working.name, false);
    }

    templateSkin = t;
    working.base = t.builtIn ? t.id : t.base;
    working.colours = t.colours;
    coloursChanged();
}

void SkinEditor::copyGroup (int group, const skin::Skin& from)
{
    if (! juce::isPositiveAndBelow (group, (int) groups().size()))
        return;

    for (auto t : groups()[(size_t) group].tokens)
        working.set (t, from.get (t));
    coloursChanged();
}

void SkinEditor::showGroupMenu (int group)
{
    if (! juce::isPositiveAndBelow (group, (int) groups().size()))
        return;

    static constexpr int resetId = 1, borrowBase = 100;   // static：MSVC はラムダから暗黙に使えない

    juce::PopupMenu menu;
    menu.addSectionHeader (tr (groups()[(size_t) group].label));
    {
        juce::PopupMenu::Item reset (tr ("skinEditor.group.reset", templateSkin.name));
        reset.itemID = resetId;
        reset.isEnabled = ! sameGroup (working, templateSkin, group);
        reset.image = groupSwatches (templateSkin, group);
        menu.addItem (std::move (reset));
    }
    menu.addSeparator();
    menu.addSectionHeader (tr ("skinEditor.group.borrow"));
    for (size_t i = 0; i < templates.size(); ++i)
    {
        const auto& t = templates[i];
        juce::PopupMenu::Item item (t.name + utf8 (" \xe2\x80\x94 ") + subtitleOf (t));
        item.itemID = borrowBase + (int) i;
        item.isTicked = sameGroup (working, t, group);   // いまと同じ色
        item.image = groupSwatches (t, group);
        menu.addItem (std::move (item));
    }

    auto* key = groupKeys[group];
    key->setToggleState (true, juce::dontSendNotification);
    juce::Component::SafePointer<SkinEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (key)
                            .withParentComponent (getTopLevelComponent())   // ウィンドウからはみ出さない
                            .withMinimumWidth (380)
                            .withStandardItemHeight (28),
                        [safe, group] (int result)
                        {
                            if (safe == nullptr) return;
                            safe->groupKeys[group]->setToggleState (false, juce::dontSendNotification);
                            if (result == resetId)
                                safe->copyGroup (group, safe->templateSkin);
                            else if (result >= borrowBase && result - borrowBase < (int) safe->templates.size())
                                safe->copyGroup (group, safe->templates[(size_t) (result - borrowBase)]);
                        });
}

void SkinEditor::preview()
{
    if (! onPreview)
        return;

    onPreview (working);
    previewed = true;

    // 文字入力は入れた時の色を覚えているので、今の色で塗り直す
    for (auto* f : { &nameField, &authorField, &hexField })
    {
        f->applyColourToAllText (colours::text, true);
        f->repaint();
    }
    authorField.setTextToShowWhenEmpty (tr ("skinEditor.author.empty"), colours::textMute);
}

void SkinEditor::setStatus (const juce::String& text, juce::Colour c)
{
    status = text;
    statusColour = c;
    repaint();
}

//==============================================================================
void SkinEditor::save()
{
    working.name = nameField.getText().trim();
    working.author = authorField.getText().trim();
    if (! editingExisting)
        working.id = library.uniqueId (working.id);
    if (working.name.isEmpty())
        working.name = working.id;

    if (! library.save (working))
    {
        setStatus (tr ("skinEditor.saveFailed"), colours::bad);
        return;
    }

    committed = true;
    if (onSaved) onSaved (working.id);
}

void SkinEditor::exportFile()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("skinEditor.chooseExport"),
                                                   defaultFolder().getChildFile (working.id + skin::fileExtension),
                                                   juce::String ("*") + skin::fileExtension);
    juce::Component::SafePointer<SkinEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (safe == nullptr || f == juce::File())
                                  return;
                              if (! f.hasFileExtension (skin::fileExtension))
                                  f = f.withFileExtension (skin::fileExtension);

                              if (skin::writeFile (safe->working, f))
                                  safe->setStatus (tr ("skinEditor.exported", f.getFileName()), colours::textDim);
                              else
                                  safe->setStatus (tr ("skinEditor.exportFailed"), colours::bad);
                          });
}

void SkinEditor::importFile()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("skinEditor.chooseImport"), defaultFolder(),
                                                   juce::String ("*") + skin::fileExtension);
    juce::Component::SafePointer<SkinEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe == nullptr || f == juce::File())
                                  return;

                              const auto r = skin::readFile (f);
                              if (! r.ok())
                              {
                                  safe->setStatus (readErrorText (r.error), colours::bad);
                                  return;
                              }

                              // 読み込んだものは新しいスキン（保存すると一覧に加わる）。戻り先は書いてあった base
                              auto s = r.skin;
                              s.builtIn = false;
                              s.id = safe->library.uniqueId (s.id);
                              const auto* b = skin::findBuiltIn (s.base);
                              safe->templateSkin = b != nullptr ? *b : skin::defaultSkin();
                              safe->templatePicker.setSelected (safe->templateIndex (safe->templateSkin.id), juce::dontSendNotification);
                              safe->loadWorking (s, false);
                              safe->setStatus (tr ("skinEditor.imported", f.getFileName()), colours::textDim);
                          });
}

void SkinEditor::deletePressed()
{
    // 押し間違いを防ぐ：もう一度押すと消す（3 秒で戻る）
    if (! deleteArmed)
    {
        deleteArmed = true;
        deleteKey->setButtonText (tr ("skinEditor.deleteConfirm"));
        resized();
        juce::Component::SafePointer<SkinEditor> safe (this);
        juce::Timer::callAfterDelay (3000, [safe]
        {
            if (safe == nullptr || ! safe->deleteArmed) return;
            safe->deleteArmed = false;
            safe->deleteKey->setButtonText (tr ("skinEditor.delete"));
            safe->resized();
        });
        return;
    }

    if (! library.remove (working.id))
    {
        setStatus (tr ("skinEditor.deleteFailed"), colours::bad);
        return;
    }

    committed = true;
    if (onDeleted) onDeleted (working.id);
}

//==============================================================================
void SkinEditor::layoutBody (juce::Rectangle<int> r)
{
    // 名前・作者 / テンプレート
    const auto labelFont = sans (12.5f, Weight::medium);
    const auto labelW = (int) std::ceil (juce::jmax (textWidth (labelFont, tr ("skinEditor.name")),
                                                     textWidth (labelFont, tr ("skinEditor.template")))) + 12;
    const auto authorW = (int) std::ceil (textWidth (labelFont, tr ("skinEditor.author"))) + 12;

    auto row = r.removeFromTop (fieldH);
    nameLabel = row.removeFromLeft (labelW);
    authorField.setBounds (row.removeFromRight (pickerW));
    authorLabel = row.removeFromRight (authorW).withTrimmedLeft (4);
    row.removeFromRight (12);
    nameField.setBounds (row);

    r.removeFromTop (8);
    row = r.removeFromTop (fieldH);
    templateLabel = row.removeFromLeft (labelW);
    const auto pickerWidth = juce::jlimit (180, 260, templatePicker.idealWidth());
    templatePicker.setBounds (row.removeFromLeft (pickerWidth));
    row.removeFromLeft (12);
    noteArea = row;

    r.removeFromTop (12);

    // 色見本（左に 2 列）と色の面（右）
    auto main = r.removeFromTop (2 * groupHeaderH + 8 * swatchH + 8);
    auto picking = main.removeFromRight (pickerW);

    auto col = main.removeFromLeft (swatchColW);
    for (size_t g = 0; g < groups().size(); ++g)
    {
        if (g == 2)
            col = main.withTrimmedLeft (colGap).withWidth (swatchColW);
        else if (g == 1 || g == 3)
            col.removeFromTop (8);

        groupAreas[g] = col.removeFromTop (groupHeaderH);
        auto* key = groupKeys[(int) g];
        key->setSize (10, 22);
        const auto w = key->idealWidth();
        key->setBounds (groupAreas[g].withTrimmedLeft (groupAreas[g].getWidth() - w).withSizeKeepingCentre (w, 22).translated (0, -1));

        for (auto t : groups()[g].tokens)
            swatchAreas[(size_t) t] = col.removeFromTop (swatchH);
    }

    pickerLabelArea = picking.removeFromTop (groupHeaderH);
    auto hexRow = picking.removeFromBottom (fieldH);
    hexLabel = hexRow.removeFromLeft (40);
    hexField.setBounds (hexRow);
    picking.removeFromBottom (8);
    picker.setBounds (picking);

    r.removeFromTop (12);
    checksArea = r;
}

int SkinEditor::swatchAt (juce::Point<int> p) const
{
    for (int i = 0; i < skin::numTokens; ++i)
        if (swatchAreas[(size_t) i].contains (p))
            return i;
    return -1;
}

void SkinEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto h = swatchAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void SkinEditor::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void SkinEditor::mouseDown (const juce::MouseEvent& e)
{
    if (const auto i = swatchAt (e.getPosition()); i >= 0)
        selectToken ((Token) i);
}

juce::String SkinEditor::warningText (const skin::Warning& w) const
{
    const auto a = tr (tokenNameKey (w.a)), b = tr (tokenNameKey (w.b));
    switch (w.kind)
    {
        case skin::Warning::Kind::contrast:
            return tr ("skinEditor.warn.contrast", a, b, juce::String (w.value, 1), juce::String (w.required, 1));
        case skin::Warning::Kind::deltaE:
            return tr ("skinEditor.warn.deltaE", a, b, juce::String (juce::roundToInt (w.value)), juce::String (juce::roundToInt (w.required)));
        case skin::Warning::Kind::recHue:
            break;
    }
    return tr ("skinEditor.warn.recHue", juce::String (juce::roundToInt (w.value)));
}

void SkinEditor::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    // ラベル
    g.setColour (colours::textDim);
    g.setFont (sans (12.5f, Weight::medium));
    g.drawText (tr ("skinEditor.name"), nameLabel, juce::Justification::centredLeft, false);
    g.drawText (tr ("skinEditor.author"), authorLabel, juce::Justification::centredLeft, false);
    g.drawText (tr ("skinEditor.template"), templateLabel, juce::Justification::centredLeft, false);

    // テンプレートのコピーか、保存先
    g.setColour (colours::textMute);
    g.setFont (sans (11.0f));
    const auto note = editingExisting ? tr ("skinEditor.savedAs", library.fileFor (working.id).getFileName())
                                      : tr ("skinEditor.copyNote");
    g.drawFittedText (note, noteArea, juce::Justification::centredLeft, 2, 1.0f);

    // 色見本
    for (size_t gi = 0; gi < groups().size(); ++gi)
    {
        auto head = groupAreas[gi].toFloat();
        g.setColour (colours::text.withAlpha (0.88f));
        g.setFont (sans (12.5f, Weight::semibold));
        g.drawText (tr (groups()[gi].label), head.withTrimmedRight ((float) groupKeys[(int) gi]->getWidth() + 4.0f),
                    juce::Justification::centredLeft, true);
        paint::hline (g, head.getBottom() - 2.0f, head.getX(), head.getRight(), colours::grid);

        for (auto t : groups()[gi].tokens)
        {
            const auto i = (int) t;
            auto a = swatchAreas[(size_t) i].toFloat().reduced (0.0f, 2.0f);
            const bool sel = t == selected;

            if (sel || hover == i)
            {
                g.setColour (sel ? colours::raisedHi : colours::raised.withAlpha (0.6f));
                g.fillRoundedRectangle (a, 4.0f);
                if (sel)
                {
                    g.setColour (colours::lineHi);
                    g.drawRoundedRectangle (a.reduced (0.5f), 4.0f, 1.0f);
                }
            }

            a.removeFromLeft (6.0f);
            const auto chip = a.removeFromLeft (28.0f).withSizeKeepingCentre (28.0f, 16.0f);
            g.setColour (juce::Colour (working.get (t)));
            g.fillRoundedRectangle (chip, 3.0f);
            g.setColour (colours::lineHi);
            g.drawRoundedRectangle (chip.reduced (0.5f), 3.0f, 1.0f);
            a.removeFromLeft (8.0f);

            const auto hexArea = a.removeFromRight (64.0f);
            g.setColour (sel ? colours::text : colours::textDim);
            g.setFont (mono (10.5f));
            g.drawText (skin::toHex (working.get (t)), hexArea.withTrimmedRight (6.0f), juce::Justification::centredRight, false);

            g.setColour (sel ? colours::text : colours::text.withAlpha (0.85f));
            g.setFont (sans (12.0f, sel ? Weight::medium : Weight::regular));
            g.drawText (tr (tokenNameKey (t)), a, juce::Justification::centredLeft, true);
        }
    }

    // 選んでいる色の名前（と JSON のキー）
    {
        auto a = pickerLabelArea.toFloat();
        g.setColour (colours::text);
        g.setFont (sans (12.5f, Weight::semibold));
        const auto name = tr (tokenNameKey (selected));
        g.drawText (name, a, juce::Justification::centredLeft, true);
        a.removeFromLeft (textWidth (g.getCurrentFont(), name) + 8.0f);
        paint::microLabel (g, a, skin::tokenKey (selected), colours::textMute);
    }
    paint::microLabel (g, hexLabel.toFloat(), tr ("skinEditor.hex"), colours::textMute);

    // 見やすさの点検（保存は止めない。アンバーで知らせる）
    {
        auto a = checksArea.toFloat();
        paint::hline (g, a.getY(), a.getX(), a.getRight(), colours::grid);
        a.removeFromTop (6.0f);
        paint::sectionHeader (g, a.removeFromTop (20.0f).toNearestInt(), tr ("skinEditor.checks.micro"), tr ("skinEditor.checks"));

        const auto statusArea = status.isNotEmpty() ? a.removeFromBottom (18.0f) : juce::Rectangle<float>();
        const auto lineH = 17.0f;
        const auto maxLines = juce::jmax (1, (int) (a.getHeight() / lineH));

        auto lineArea = [&] { return a.removeFromTop (lineH); };
        g.setFont (sans (12.0f));

        if (warnings.empty())
        {
            auto l = lineArea();
            paint::led (g, { l.getX() + 4.0f, l.getCentreY() }, 3.0f, colours::signal, true);
            g.setColour (colours::textDim);
            g.drawText (tr ("skinEditor.checks.ok"), l.withTrimmedLeft (16.0f), juce::Justification::centredLeft, true);
        }
        else
        {
            const auto shown = (int) warnings.size() > maxLines ? maxLines - 1 : (int) warnings.size();
            for (int i = 0; i < shown; ++i)
            {
                auto l = lineArea();
                paint::led (g, { l.getX() + 4.0f, l.getCentreY() }, 3.0f, colours::warn, true);
                g.setColour (colours::warn);
                g.drawText (warningText (warnings[(size_t) i]), l.withTrimmedLeft (16.0f), juce::Justification::centredLeft, true);
            }
            if (shown < (int) warnings.size())
            {
                g.setColour (colours::warn);
                g.drawText (tr ("skinEditor.warn.more", (int) warnings.size() - shown), lineArea().withTrimmedLeft (16.0f),
                            juce::Justification::centredLeft, true);
            }
        }

        if (status.isNotEmpty())
        {
            g.setColour (statusColour);
            g.setFont (sans (12.0f));
            g.drawText (status, statusArea, juce::Justification::centredLeft, true);
        }
    }
}
} // namespace vb
