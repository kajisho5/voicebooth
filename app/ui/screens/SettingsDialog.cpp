#include "SettingsDialog.h"
#include "../../system/AppCache.h"
#include "separation/SeparatorClient.h"
#include "models/ModelDownloader.h"

namespace vb
{
namespace
{
    // 文字の大きさはデジタル庁デザインシステムの目安に合わせる（本文 16 px 以上、14 px は補足だけ）。
    // 値は CSS と同じ「字の大きさ（em）」。JUCE の高さは字の上下の幅なので、IBM Plex Sans JP は ×1.5、Plex Mono は ×1.3。
    // 1920x1080 の窓で等倍。窓が小さい時は全体を縮める（scale）
    constexpr float sansEm = 1.5f, monoEm = 1.3f;
    constexpr float titlePx = 18.0f, labelPx = 16.0f, notePx = 14.0f, keyPx = 14.0f;
    constexpr int baseW = 1240, baseRowH = 58, baseCtrlH = 42, baseFooterKeyH = 44;
    constexpr float minScale = 0.6f;

    juce::StringArray languageNames()
    {
        juce::StringArray a;
        for (auto& l : i18n::available())
            a.add (juce::String::fromUTF8 (l.nativeName));
        return a;
    }

    /** 「このパソコン」の 1 行目：OS · CPU · コア · メモリ · 空き（DESIGN 11.6.1） */
    juce::String systemSummary (const system::Info& i)
    {
        juce::StringArray parts { i.osName, i.cpuModel + " (" + i.architecture + ")",
                                  tr ("settings.system.cores").replace ("{0}", juce::String (i.physicalCores)),
                                  tr ("settings.system.memory").replace ("{0}", juce::String ((double) i.memoryMB / 1024.0, 1)) };
        if (i.freeDiskMB >= 0)
            parts.add (tr ("settings.system.disk").replace ("{0}", juce::String (i.freeDiskMB / 1024)));
        return parts.joinIntoString (utf8 (" \xc2\xb7 "));
    }

    juce::String itemNames (const juce::Array<system::Item>& items)
    {
        juce::StringArray names;
        for (auto item : items)
            names.add (tr (item == system::Item::cores  ? "settings.system.item.cores"
                         : item == system::Item::memory ? "settings.system.item.memory"
                                                        : "settings.system.item.disk"));
        return names.joinIntoString (tr ("settings.system.sep"));
    }

    /** 内蔵は「Booth — 夜のブース」、自作は「名前 — 作者」（作者が無ければ「自作」） */
    juce::StringArray skinNames (const std::vector<skin::Skin>& skins)
    {
        juce::StringArray a;
        for (auto& s : skins)
        {
            const auto sub = s.builtIn ? tr (skin::subtitleKey (s.id).toRawUTF8())
                                       : (s.author.isNotEmpty() ? s.author : tr ("settings.skin.mine"));
            a.add (s.name + utf8 (" \xe2\x80\x94 ") + sub);
        }
        return a;
    }

    int skinIndex (const std::vector<skin::Skin>& skins, const juce::String& id)
    {
        for (size_t i = 0; i < skins.size(); ++i)
            if (skins[i].id == id)
                return (int) i;
        return 0;
    }

    int languageIndex (i18n::Language lang)
    {
        const auto& list = i18n::available();
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].id == lang)
                return (int) i;
        return 0;
    }
}

SettingsDialog::SettingsDialog (UiSession& u, std::vector<skin::Skin> skinList, const juce::String& currentSkin)
    : DialogPanel (tr ("settings.title"), tr ("settings.micro")), SessionView (u),
      skinChoices (std::move (skinList)),
      language (languageNames(), languageIndex (i18n::current())),
      skinPicker (skinNames (skinChoices), skinIndex (skinChoices, currentSkin)),
      editSkin (tr ("settings.skin.edit")),
      newSkin (tr ("settings.skin.new")),
      mode ({ tr ("mode.easy"), tr ("mode.standard"), tr ("mode.pro") }, (int) u->mode),
      tolerance ({ "20", "30", "50" }, u->pitchToleranceCents <= 20.0f ? 0 : (u->pitchToleranceCents <= 30.0f ? 1 : 2)),
      countIn ({ tr ("transport.countIn.off"), "1", "2" }, u->countInBars),
      crossfade ({ "0", "5", "8", "20" }, u->crossfadeMs <= 0.0 ? 0 : (u->crossfadeMs <= 5.0 ? 1 : (u->crossfadeMs <= 8.0 ? 2 : 3))),
      octaveAlign (tr ("pitch.octaveAlign")),
      showLyrics (tr ("common.off")),
      openSetup (tr ("settings.device.open")),
      cacheKey (tr ("settings.cache.change")),
      modelKey (tr ("model.getButton")),
      supportKey (tr ("settings.support.open")),
      cacheOpen (tr ("settings.cache.open")),
      cacheClear (tr ("settings.cache.clear")),
      updateAuto (tr ("common.on")),
      updateBetas (tr ("settings.update.betas")),
      updateNow (tr ("settings.update.now")),
      systemInfo (system::gather (juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("VoiceBooth")))
{
    language.onChange = [this] (int i) { if (onLanguage) onLanguage (i18n::available()[(size_t) i].id); };
    skinPicker.onChange = [this] (int i) { if (onSkin && juce::isPositiveAndBelow (i, (int) skinChoices.size())) onSkin (skinChoices[(size_t) i].id); };
    editSkin.withIcon (Icon::edit);
    editSkin.onClick = [this] { if (onEditSkin) onEditSkin(); };
    newSkin.withIcon (Icon::plus);
    newSkin.onClick = [this] { if (onNewSkin) onNewSkin(); };
    mode.onChange = [this] (int i) { session.setMode ((project::Mode) i); };
    tolerance.onChange = [this] (int i) { const float v[] = { 20.0f, 30.0f, 50.0f }; session.setPitchTolerance (v[i]); };
    countIn.onChange = [this] (int i) { session.setCountIn (i); };
    crossfade.onChange = [this] (int i) { const double v[] = { 0.0, 5.0, 8.0, 20.0 }; session.setCrossfade (v[i]); };

    octaveAlign.withLed().withToggle (false);
    octaveAlign.onClick = [this] { session.setOctaveAlign (! state().octaveAlign); };
    showLyrics.withLed().withToggle (false);
    showLyrics.onClick = [this] { session.setShowLyrics (! state().showLyrics); };
    openSetup.withIcon (Icon::mic);
    openSetup.onClick = [this] { if (onOpenSetup) onOpenSetup(); };
    // 分離モデル：どこからでも入れられるように（ダウンロードは押した時だけ。11.7）
    modelKey.withIcon (Icon::download).withLed().withToggle (false);
    modelKey.onClick = [this] { if (onInstallModels) onInstallModels(); };
    // キャッシュの場所：変更（フォルダを選ぶ）・開く（Finder / エクスプローラー）・空にする（確かめてから）
    cacheKey.withIcon (Icon::folder);
    cacheKey.setTooltip (tr ("settings.cache.change.tooltip"));
    cacheKey.onClick = [this] { chooseCacheFolder(); };
    cacheOpen.setTooltip (tr ("settings.cache.open.tooltip"));
    cacheOpen.onClick = [this]
    {
        const auto dir = session.cacheFolder();
        dir.createDirectory();   // まだ何も作っていなくても開けるように
        dir.startAsProcess();
    };
    cacheClear.setTooltip (tr ("settings.cache.clear.tooltip"));
    cacheClear.onClick = [this] { if (onClearCache) onClearCache(); };

    // 新しいバージョン：自動で確かめる（入 / 切）・ベータも知らせる・今すぐ確かめる
    updateAuto.withLed().withToggle (false);
    updateAuto.setTooltip (tr ("settings.update.auto.tooltip"));
    updateAuto.onClick = [this] { session.setUpdateAutoCheck (! state().updateAutoCheck); };
    updateBetas.withLed().withToggle (false);
    updateBetas.setTooltip (tr ("settings.update.betas.tooltip"));
    updateBetas.onClick = [this] { session.setUpdateBetas (! state().updateBetas); };
    updateNow.withIcon (Icon::download);
    updateNow.setTooltip (tr ("settings.update.now.tooltip"));
    updateNow.onClick = [this] { session.checkForUpdatesNow(); };
    supportKey.withIcon (Icon::globe);
    supportKey.onClick = [] { juce::URL ("https://github.com/sponsors/kajisho5").launchInDefaultBrowser(); };

    // このパソコンが動作環境を満たすか（DESIGN 11.6.1）。足りなくても止めない、知らせるだけ
    const auto verdict = system::evaluate (systemInfo);
    juce::String systemValue;
    juce::Colour systemLed;
    switch (verdict.level)
    {
        case system::Level::ok:
            systemValue = tr ("settings.system.ok");
            systemLed = colours::signal;
            break;
        case system::Level::belowRecommended:
            systemValue = tr ("settings.system.belowRec").replace ("{0}", itemNames (verdict.belowRecommended));
            systemLed = colours::warn;
            break;
        case system::Level::belowMinimum:
            systemValue = tr ("settings.system.belowMin").replace ("{0}", itemNames (verdict.belowMinimum));
            systemLed = colours::bad;
            break;
    }

    rows = {
        { tr ("settings.language"),   tr ("settings.language.note"),   &language,    300 },
        { tr ("settings.mode"),       tr ("settings.mode.note"),       &mode,        380 },
        { tr ("settings.tolerance"),  tr ("settings.tolerance.note"),  &tolerance,   290 },
        { tr ("settings.octave"),     tr ("settings.octave.note"),     &octaveAlign, 0 },
        { tr ("settings.lyrics"),     tr ("settings.lyrics.note"),     &showLyrics,  0 },
        { tr ("settings.countIn"),    tr ("settings.countIn.note"),    &countIn,     290 },
        { tr ("settings.crossfade"),  tr ("settings.crossfade.note"),  &crossfade,   360 },
        { tr ("settings.device"),     tr ("settings.device.note"),     &openSetup,   0 },
        { tr ("settings.models"),     tr ("settings.models.note"),     &modelKey,    0 },
        { tr ("settings.cache"),      {},                              &cacheKey,    0, {}, {}, { &cacheOpen, &cacheClear } },
        { tr ("settings.update"),     {},                              &updateAuto,  0, {}, {}, { &updateBetas, &updateNow } },
        { tr ("settings.system"),     systemSummary (systemInfo),      nullptr,      540, systemValue, systemLed },
        { tr ("settings.skin"),       tr ("settings.skin.note"),       &skinPicker,  300, {}, {}, { &newSkin, &editSkin } },
        { tr ("settings.support"),    tr ("settings.support.note"),    &supportKey,  0 },
    };

    for (auto& r : rows)
    {
        r.baseWidth = r.controlWidth;
        if (r.control != nullptr)
        {
            // 読み上げ（#28）：選ぶ部品は行の見出しを名前に、キーは自分の文字のまま見出しを説明に
            if (dynamic_cast<KeyButton*> (r.control) != nullptr)
                r.control->setDescription (r.label);
            else
                r.control->setTitle (r.label);
            addAndMakeVisible (r.control);
        }
        for (auto* k : r.extras)
            addAndMakeVisible (k);
    }

    closeFooter = &addFooterKey (tr ("common.close"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });
    aboutFooter = &addFooterKey (tr ("about.open"), KeyRole::normal, [this] { if (onAbout) onAbout(); });

    applyScale (1.0f);
    onSessionChanged (change::all);
}

void SettingsDialog::applyScale (float k)
{
    scale = k;
    const auto sansH = [k] (float px) { return px * sansEm * k; };
    rowH = juce::roundToInt ((float) baseRowH * k);
    ctrlH = juce::roundToInt ((float) baseCtrlH * k);
    titleHeight = sansH (titlePx);
    titleExact = true;
    footerKeyHeight = juce::roundToInt ((float) baseFooterKeyH * k);

    // 幅は下の idealWidth が文字から測るので、先に文字を決める
    for (auto* sk : { &tolerance, &countIn, &crossfade })
        sk->setFont (monoExact (keyPx * monoEm * k, Weight::medium));
    mode.setFont (sansExact (sansH (keyPx), Weight::medium));
    for (auto* d : { &language, &skinPicker })
        d->setFont (sansExact (sansH (keyPx), Weight::medium));
    for (auto* key : { &octaveAlign, &showLyrics, &openSetup, &cacheKey, &supportKey, &cacheOpen, &cacheClear,
                       &updateAuto, &updateBetas, &updateNow, &editSkin, &newSkin, &modelKey, closeFooter, aboutFooter })
        key->withFont (sansExact (sansH (keyPx), Weight::medium));

    const auto px = [k] (int base) { return juce::roundToInt ((float) base * k); };
    for (auto& r : rows)
    {
        r.controlWidth = px (r.baseWidth);
        if (r.control == &language || r.control == &skinPicker)
            r.controlWidth = juce::jmax (r.controlWidth, dynamic_cast<Dropdown*> (r.control)->idealWidth());
        if (auto* key = dynamic_cast<KeyButton*> (r.control); key != nullptr && ! r.extras.empty())
        {
            key->setSize (10, ctrlH);
            r.controlWidth = juce::jmax (px (120), key->idealWidth());   // キー自身の幅（左に並べる extras の分は下で足す）
        }
        for (auto* key : r.extras)
        {
            // control の左に置くキーの分だけ、文言の幅を詰める
            key->setSize (10, ctrlH);
            r.controlWidth += juce::jmax (px (92), key->idealWidth()) + px (8);
        }
    }

    setSize (px (baseW), headerH + 14 + rowH * (int) rows.size() + footerH + 12);
    resized();
    repaint();
}

void SettingsDialog::fitToParent()
{
    auto* parent = getParentComponent();
    if (parent == nullptr || parent->getHeight() <= 0)
        return;
    // 窓が画面より大きい時（小さい画面・拡大 125 % 以上）は、画面に見えている所に収める
    auto visible = parent->getLocalBounds();
    if (parent->isShowing())
        if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (parent->getScreenBounds()))
            visible = visible.getIntersection (parent->getLocalArea (nullptr, d->userArea));
    if (visible.isEmpty())
        visible = parent->getLocalBounds();

    // 1920x1080 の窓で等倍。収まらなければ全体を縮める（上下左右に 16 ずつ残す）
    const auto fixed = headerH + 14 + footerH + 12;
    const auto byW = (float) (visible.getWidth() - 32) / (float) baseW;
    const auto byH = (float) (visible.getHeight() - 32 - fixed) / (float) (baseRowH * (int) rows.size());
    const auto k = juce::jlimit (minScale, 1.0f, juce::jmin (byW, byH));
    if (std::abs (k - scale) > 0.005f)
        applyScale (k);
    setCentrePosition (visible.getCentre());
}

void SettingsDialog::onSessionChanged (juce::uint32 changes)
{
    const auto& s = state();
    mode.setSelected ((int) s.mode, juce::dontSendNotification);
    countIn.setSelected (s.countInBars, juce::dontSendNotification);
    countIn.setTooltip (s.tempoKnown() ? tr ("transport.countIn.tooltip") : tr ("transport.countIn.noTempo"));
    octaveAlign.setToggleState (s.octaveAlign, juce::dontSendNotification);
    octaveAlign.setButtonText (s.octaveAlign ? tr ("common.on") : tr ("common.off"));
    showLyrics.setToggleState (s.showLyrics, juce::dontSendNotification);
    showLyrics.setButtonText (s.showLyrics ? tr ("common.on") : tr ("common.off"));

    // 録音中はスキンを切り替えない（DESIGN 4.11）
    for (auto* c : { (juce::Component*) &skinPicker, (juce::Component*) &editSkin, (juce::Component*) &newSkin })
    {
        c->setEnabled (! s.isRecording);
        c->setAlpha (s.isRecording ? 0.45f : 1.0f);
    }

    // 分離モデル：3 つ（分離・リードボーカル・音程）が入っていれば「入っています」。受け取り中は進み具合
    {
        using SC = separation::SeparatorClient;
        using DS = models::DownloadStatus::Stage;
        const bool installed = SC::modelInstalled() && SC::karaokeInstalled() && SC::pitchModelFile().existsAsFile();
        const bool busy = s.modelDl.stage == (int) DS::downloading || s.modelDl.stage == (int) DS::verifying;
        if (busy)
            modelKey.setButtonText (tr ("settings.models.downloading",
                                        juce::String (s.modelDl.size > 0 ? (int) (s.modelDl.received * 100 / s.modelDl.size) : 0)));
        else
            modelKey.setButtonText (installed ? tr ("settings.models.installed") : tr ("model.getButton"));
        modelKey.setToggleState (installed, juce::dontSendNotification);
        const bool canPress = ! installed && ! busy && s.engineAttached && ! s.isRecording;
        modelKey.setEnabled (canPress);
        modelKey.setAlpha (canPress || installed ? 1.0f : 0.45f);
        if (getWidth() > 0) resized();   // 文言で幅が変わる
    }

    // 新しいバージョンの確認
    updateAuto.setToggleState (s.updateAutoCheck, juce::dontSendNotification);
    updateAuto.setButtonText (s.updateAutoCheck ? tr ("common.on") : tr ("common.off"));
    updateBetas.setToggleState (s.updateBetas, juce::dontSendNotification);
    // 録音・再生中は確かめない（DESIGN 11.7）
    updateNow.setEnabled (! s.updateChecking && ! s.isPlaying && ! s.isRecording);
    updateNow.setAlpha (s.isPlaying || s.isRecording ? 0.45f : 1.0f);
    updateNow.setButtonText (s.updateChecking ? tr ("settings.update.checking") : tr ("settings.update.now"));

    // 分離の途中（オフボを作っている）はキャッシュの場所を変えない・空にしない
    for (auto* k : { &cacheKey, &cacheClear })
    {
        k->setEnabled (! s.separating);
        k->setAlpha (s.separating ? 0.45f : 1.0f);
    }
    // キャッシュの大きさはフォルダを数えるので、場所・分離（オフボを作った）が変わった時だけ
    if (changes & (change::prefs | change::view))
        refreshNotes();

    // クロスフェードはプロのみ編集（DESIGN 6.4）
    crossfade.setEnabled (s.mode == project::Mode::pro);
    crossfade.setAlpha (crossfade.isEnabled() ? 1.0f : 0.45f);
    repaint();
}

void SettingsDialog::refreshNotes()
{
    // 行の下の小さな文字：キャッシュは場所と大きさ、更新は送る物と最後に確かめた時刻
    const auto& s = state();
    for (auto& row : rows)
    {
        if (row.control == &cacheKey)
        {
            const auto dir = session.cacheFolder();
            // 曲ごとのキャッシュ（<プロジェクト>/Cache/）はここに入らない（空にしても消えない）ことも書く
            row.note = system::displayPath (dir) + utf8 (" \xc2\xb7 ") + system::formatSize (system::cacheSize (dir))
                     + utf8 (" \xc2\xb7 ") + tr ("settings.cache.note");
        }
        else if (row.control == &updateAuto)
        {
            row.note = tr ("settings.update.note");
            if (s.updateVersion.isNotEmpty())
                row.note << utf8 (" \xc2\xb7 ") << tr ("update.notice", s.updateVersion);
            else if (s.updateLastCheck > 0)
                row.note << utf8 (" \xc2\xb7 ") << tr ("settings.update.last", juce::Time (s.updateLastCheck).formatted ("%Y-%m-%d %H:%M"));
        }
    }
}

void SettingsDialog::chooseCacheFolder()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("settings.cache.chooser"), session.cacheFolder());
    juce::Component::SafePointer<SettingsDialog> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (safe != nullptr && dir != juce::File())   // 空ならやめた
                                  safe->session.setCacheFolder (dir);   // 書けない場所なら知らせて変えない
                          });
}

void SettingsDialog::layoutBody (juce::Rectangle<int> r)
{
    rowAreas.clear();
    for (auto& row : rows)
    {
        auto a = r.removeFromTop (rowH);
        rowAreas.push_back (a);

        if (row.control == nullptr) continue;

        // キーだけの行：キーの文字が長い（フランス語の「Obtenir le modèle de séparation…」など）ときは、その幅を取る。
        // 取らないと、キーが中央寄せで広がって右にはみ出していた（説明の文字の幅も controlWidth で決まる。#19）
        auto* key = dynamic_cast<KeyButton*> (row.control);
        if (key != nullptr && row.extras.empty())
        {
            key->setSize (10, ctrlH);
            row.controlWidth = juce::jmax (row.controlWidth, key->idealWidth());
        }

        auto c = a.removeFromRight (juce::jmax (juce::roundToInt (260 * scale), row.controlWidth));
        if (auto* k = key)
        {
            k->setSize (10, ctrlH);
            const auto w = juce::jmin (c.getWidth(), juce::jmax (juce::roundToInt (120 * scale), k->idealWidth()));
            k->setBounds (c.removeFromRight (w).withSizeKeepingCentre (w, ctrlH));
            // キーの左に並べるキー（並びは extras の順）
            for (auto it = row.extras.rbegin(); it != row.extras.rend(); ++it)
            {
                c.removeFromRight (juce::roundToInt (8 * scale));
                const auto keyW = juce::jmax (juce::roundToInt (92 * scale), (*it)->idealWidth());
                (*it)->setBounds (c.removeFromRight (keyW).withSizeKeepingCentre (keyW, ctrlH));
            }
        }
        else if (! row.extras.empty())
        {
            auto area = c.removeFromRight (row.controlWidth);
            for (auto* key : row.extras)
            {
                const auto keyW = juce::jmax (juce::roundToInt (92 * scale), key->idealWidth());
                key->setBounds (area.removeFromLeft (keyW).withSizeKeepingCentre (keyW, ctrlH));
                area.removeFromLeft (juce::roundToInt (8 * scale));
            }
            row.control->setBounds (area.withSizeKeepingCentre (area.getWidth(), ctrlH));
        }
        else
        {
            row.control->setBounds (c.removeFromRight (row.controlWidth).withSizeKeepingCentre (row.controlWidth, ctrlH));
        }
    }
}

void SettingsDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto a = rowAreas[i].toFloat();
        if (i + 1 < rows.size())
            paint::hline (g, a.getBottom() - 1.0f, a.getX(), a.getRight(), colours::grid);

        auto text = a.withTrimmedRight ((float) juce::jmax (juce::roundToInt (260 * scale), rows[i].controlWidth) + 24.0f * scale);
        g.setColour (colours::text);
        g.setFont (sansExact (labelPx * sansEm * scale, Weight::medium));
        g.drawText (rows[i].label, text.removeFromTop (rows[i].note.isEmpty() ? text.getHeight() : text.getHeight() * 0.54f),
                    rows[i].note.isEmpty() ? juce::Justification::centredLeft : juce::Justification::bottomLeft, true);
        if (rows[i].note.isNotEmpty())
        {
            // 説明は暗すぎると読めない（textMute は地との差が小さい）。textDim で一段明るく
            g.setColour (colours::textDim);
            g.setFont (sansExact (notePx * sansEm * scale));
            g.drawText (rows[i].note, text.withTrimmedTop (2.0f), juce::Justification::topLeft, true);
        }

        if (rows[i].control == nullptr && rows[i].value.isNotEmpty())
        {
            auto v = a.removeFromRight ((float) juce::jmax (juce::roundToInt (260 * scale), rows[i].controlWidth));
            g.setFont (sansExact (keyPx * sansEm * scale));
            const auto textW = juce::jmin (v.getWidth() - 18.0f, textWidth (g.getCurrentFont(), rows[i].value) + 2.0f);
            if (! rows[i].led.isTransparent())
                paint::led (g, { v.getRight() - textW - 12.0f, v.getCentreY() }, 4.0f, rows[i].led, true);
            g.setColour (rows[i].led.isTransparent() ? colours::textDim : colours::text);
            g.drawText (rows[i].value, v, juce::Justification::centredRight, true);
        }
    }

    // ピッチ許容・クロスフェードの単位
    for (auto* c : { (juce::Component*) &tolerance, (juce::Component*) &crossfade })
    {
        const auto b = c->getBounds();
        paint::microLabel (g, juce::Rectangle<float> ((float) b.getX() - 52.0f * scale, (float) b.getY(), 46.0f * scale, (float) b.getHeight()),
                           c == &tolerance ? tr ("unit.cent") : tr ("unit.ms"), colours::textMute, juce::Justification::centredRight);
    }
}
} // namespace vb
