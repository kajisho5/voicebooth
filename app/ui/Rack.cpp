#include "Rack.h"
#include "audio/PlaybackCore.h"

namespace vb
{
void RackModule::paint (juce::Graphics& g)
{
    paint::sectionHeader (g, getLocalBounds().reduced (metrics::pad, 0).withHeight (headerH).withTrimmedTop (4), en, ja);
}

//==============================================================================
InputModule::InputModule (UiSession& u, Actions& a)
    : RackModule (tr ("rack.input"), tr ("rack.input.sub")), SessionView (u), actions (a)
{
    meter.setLevels (u->inputPeakDb, u->inputRmsDb, u->inputPeakHoldDb, u->inputClipped);
    meter.onClick = [this] { session.resetInputClip(); };
    meter.setTooltip (tr ("meter.clip.tooltip"));
    addAndMakeVisible (meter);

    buffer.setButtonText (juce::String (u->bufferSize) + " smp");
    buffer.withIcon (Icon::chevronDown).withFont (mono (10.5f));
    buffer.setTooltip (tr ("rack.input.buffer.tooltip"));
    buffer.onClick = [this] { if (actions.openSetup) actions.openSetup(); };   // バッファはデバイスの設定にある
    addChildComponent (buffer);
}

void InputModule::onSessionChanged (juce::uint32 c)
{
    const auto& s = state();
    if (c & (change::meter | change::device))
    {
        meter.setLevels (s.inputPeakDb, s.inputRmsDb, s.inputPeakHoldDb, s.inputClipped);
        repaint (readoutArea);
    }
    if (c & change::device)
    {
        buffer.setButtonText (juce::String (s.bufferSize) + " smp");
        repaint();
    }
    if (c & change::mode)
    {
        resized();
        repaint();
    }
}

void InputModule::resized()
{
    auto r = content();
    deviceArea = r.removeFromTop (20);

    // バッファ：プロは編集できるキー、標準は数値表示、簡単は「自動」（DESIGN 2）
    buffer.setVisible (state().mode == project::Mode::pro);
    if (buffer.isVisible())
    {
        buffer.setSize (10, 22);
        const auto w = buffer.idealWidth();
        buffer.setBounds (deviceArea.withLeft (deviceArea.getRight() - w).withSizeKeepingCentre (w, 22));
    }

    r.removeFromTop (6);
    meter.setBounds (r.removeFromTop (38));
    r.removeFromTop (4);
    readoutArea = r;
}

void InputModule::paint (juce::Graphics& g)
{
    RackModule::paint (g);
    const auto& s = state();

    // デバイス
    {
        auto r = deviceArea.toFloat();
        drawIcon (g, Icon::mic, r.removeFromLeft (16.0f).withSizeKeepingCentre (14.0f, 14.0f), colours::textDim);
        r.removeFromLeft (6.0f);

        if (buffer.isVisible())
            r.removeFromRight ((float) buffer.getWidth() + 6.0f);
        else
        {
            const auto drv = s.mode == project::Mode::easy ? tr ("rack.input.auto")
                                                           : s.driver + " " + juce::String (s.bufferSize);
            const auto df = mono (10.0f);
            g.setColour (colours::textMute);
            g.setFont (df);
            g.drawText (drv, r.removeFromRight (textWidth (df, drv) + 2.0f), juce::Justification::centredRight, false);
        }

        // 入力が無い時は警告色（DESIGN 4.1）。機器名はデータなので翻訳しない
        const bool missing = s.engineAttached && ! s.input.open;
        g.setColour (missing ? colours::warn : colours::text.withAlpha (0.9f));
        g.setFont (sansFor (inputDisplayName (s), 12.0f));
        g.drawText (inputDisplayName (s), r, juce::Justification::centredLeft, true);
    }

    // 数値：PEAK / RMS / 判定 / レイテンシ
    {
        auto r = readoutArea;
        auto row = r.removeFromTop (22).toFloat();
        const auto lf = mono (9.5f, Weight::medium, 0.12f);
        const auto vf = mono (13.0f, Weight::semibold);

        auto value = [&] (const juce::String& label, const juce::String& v)
        {
            paint::microLabel (g, row.removeFromLeft (textWidth (lf, label) + 6.0f), label, colours::textMute);
            g.setColour (colours::text);
            g.setFont (vf);
            g.drawText (v, row.removeFromLeft (textWidth (vf, v) + 2.0f), juce::Justification::centredLeft, false);
            row.removeFromLeft (14.0f);
        };
        value (tr ("meter.peak"), formatDb (s.inputPeakDb));
        value (tr ("meter.rms"), formatDb (s.inputRmsDb));

        // 適正 / 調整の札（入力が無い時は出さない）
        if (! s.engineAttached || s.input.open)
        {
            const bool inTarget = s.inputPeakDb >= LedMeter::targetLow && s.inputPeakDb <= LedMeter::targetHigh;
            const auto text = inTarget ? tr ("meter.ok") : tr ("meter.adjust");
            const auto cf = sans (11.0f, Weight::medium);
            const auto cw = textWidth (cf, text) + 26.0f;
            const auto chip = row.removeFromRight (cw).withSizeKeepingCentre (cw, 20.0f);
            const auto c = inTarget ? colours::signal : colours::warn;
            g.setColour (c.withAlpha (0.14f));
            g.fillRoundedRectangle (chip, 3.0f);
            paint::led (g, { chip.getX() + 9.0f, chip.getCentreY() }, 2.4f, c, true);
            g.setColour (c);
            g.setFont (cf);
            g.drawText (text, chip.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);
        }

        // レイテンシ（録音位置の補正に使う値）：申告値・手入力はそう書く。UI_MOCK はダミー
        r.removeFromTop (4);
        const auto ld = latencyDisplay (s);
        auto lat = r.removeFromTop (16).toFloat();
        paint::microLabel (g, lat.removeFromLeft (textWidth (lf, tr ("meter.latency")) + 10.0f), tr ("meter.latency"), colours::textMute);
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        const auto text = ! ld.known  ? juce::String ("-")
                        : ld.reported ? tr ("meter.latency.reported", juce::String (ld.ms, 1), ld.samples)
                        : ld.manual   ? tr ("meter.latency.manual", juce::String (ld.ms, 1), ld.samples)
                                      : tr ("meter.latency.value", juce::String (ld.ms, 1), ld.samples);
        g.drawText (text, lat, juce::Justification::centredLeft, true);
    }
}

//==============================================================================
PracticeModule::PracticeModule (UiSession& u, Actions& a)
    : RackModule (tr ("rack.practice"), tr ("rack.practice.sub")), SessionView (u), actions (a),
      tempo (tr ("practice.tempo"), 50.0, 150.0, u->tempoPercent, 1.0,
             [] (double v) { return juce::String (juce::roundToInt (v)); }, "%"),
      key (tr ("practice.key"), -6.0, 6.0, u->keyShift, 1.0,
           [] (double v) { const auto k = juce::roundToInt (v); return (k > 0 ? "+" : "") + juce::String (k); },
           {}, true, colours::ref)
{
    tempo.encoder().setDefaultValue (100.0);   // 原速で吸い付く・ダブルクリックで原速（DESIGN 4.10）
    key.encoder().setDefaultValue (0.0);
    tempo.onChange = [this] (double v) { if (! syncing && actions.requestTempo) actions.requestTempo (juce::roundToInt (v)); };
    key.onChange   = [this] (double v) { if (! syncing && actions.requestKey) actions.requestKey (juce::roundToInt (v)); };

    addAndMakeVisible (tempo);
    addAndMakeVisible (key);

    // 声域に合うキー：声域を測る → お手本の最高音・最低音が収まるキーを出す。押すとそのキーにする
    rangeKey.withIcon (Icon::mic).withFont (mono (10.5f));
    rangeKey.setTooltip (tr ("range.key.tooltip"));
    rangeKey.onClick = [this] { if (actions.openVoiceRange) actions.openVoiceRange(); };
    suggestKey.withFont (sans (11.0f, Weight::medium));
    suggestKey.onClick = [this] { session.applySuggestedKey(); };
    addChildComponent (rangeKey);
    addChildComponent (suggestKey);
    rangeKey.setVisible (u->engineAttached);
    onSessionChanged (change::all);
}

void PracticeModule::updateKeyHelp()
{
    const auto& s = state();
    if (! s.engineAttached)
        return;
    const bool hasRange = s.voiceLow >= 0 && s.voiceHigh > s.voiceLow;
    rangeKey.setButtonText (hasRange ? tr ("range.key.value", dummy::noteName ((float) s.voiceLow), dummy::noteName ((float) s.voiceHigh))
                                     : tr ("range.key.measure"));

    const auto k = session.keySuggestion();
    suggestKey.setVisible (k.ok);
    if (k.ok)
    {
        const auto v = (k.shift > 0 ? "+" : "") + juce::String (k.shift);
        auto text = k.octave < 0 ? tr ("range.suggest.octDown", v) : k.octave > 0 ? tr ("range.suggest.octUp", v) : tr ("range.suggest", v);
        suggestKey.setButtonText (text);
        const auto g = session.guideRange();
        juce::String tip = tr ("range.suggest.tooltip", dummy::noteName (g.low), dummy::noteName (g.high));
        if (! k.fits)
            tip << "\n" << tr ("range.suggest.over", juce::String (juce::roundToInt (k.overLow)), juce::String (juce::roundToInt (k.overHigh)));
        suggestKey.setTooltip (tip);
        const bool applied = k.shift == s.keyShift;
        suggestKey.withIconColour (k.fits ? colours::signal : colours::warn);
        suggestKey.withIcon (applied ? Icon::check : (k.fits ? Icon::chevronRight : Icon::warning));
        suggestKey.setEnabled (! session.deliveryLocked());
    }
    resized();
}

void PracticeModule::onSessionChanged (juce::uint32 changes)
{
    if ((changes & (change::practice | change::transport | change::songInfo | change::takes)) == 0)
        return;

    const auto& s = state();
    const juce::ScopedValueSetter<bool> guard (syncing, true);
    tempo.encoder().setValue (s.tempoPercent, juce::dontSendNotification);
    key.encoder().setValue (s.keyShift, juce::dontSendNotification);

    if (! s.tempoKnown())   // テンポが分からない間は BPM を出さない
        tempo.setCaption (s.tempoPercent == 100 ? tr ("practice.tempo.unknown") : tr ("practice.tempo.shifted"));
    else
        tempo.setCaption (s.tempoPercent == 100 ? tr ("practice.tempo.original", song::formatBpm (s.bpm()))
                                                : tr ("practice.tempo.bpm", song::formatBpm (s.bpm() * s.tempoPercent / 100.0)));
    key.setCaption (s.keyShift == 0 ? tr ("practice.key.original") : tr ("practice.key.shifted"));

    const bool lock = session.deliveryLocked();
    tempo.setLocked (lock);
    key.setLocked (lock);
    updateKeyHelp();
    repaint();
}

void PracticeModule::resized()
{
    auto r = content();
    if (rangeKey.isVisible())
    {
        auto row = r.removeFromBottom (26);
        r.removeFromBottom (6);
        rangeKey.setSize (10, 26);
        rangeKey.setBounds (row.removeFromLeft (juce::jmin (row.getWidth() / 2, juce::jmax (96, rangeKey.idealWidth()))).reduced (4, 0));
        suggestKey.setBounds (row.reduced (4, 0));
    }
    tempo.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (4, 0));
    key.setBounds (r.reduced (4, 0));
}

//==============================================================================
MonitorModule::MonitorModule (UiSession& u)
    : RackModule (tr ("rack.monitor"), tr ("rack.monitor.sub")), SessionView (u)
{
    backingStrip = strips.add (new ChannelStrip (tr ("monitor.backing"), u->offVocalGain, 0.62f));
    mainStrip = strips.add (new ChannelStrip (tr ("monitor.refMain"), u->mainGain, 0.48f, colours::ref));
    harmStrip = strips.add (new ChannelStrip (tr ("monitor.refHarm"), u->harmonyGain, 0.22f, colours::ref));
    selfStrip = strips.add (new ChannelStrip (tr ("monitor.self"), u->monitorGain, 0.70f));
    reverbStrip = strips.add (new ChannelStrip (tr ("monitor.reverb"), u->monitorReverb, -1.0f, colours::textDim, false, tr ("monitor.reverb.note")));
    // クリック・カウントインの音量（2026-10-02）。入り切りは輸送バーの「クリック」なので M / S は持たない。耳だけ
    clickStrip = strips.add (new ChannelStrip (tr ("monitor.click"), u->clickLevel, 0.0f, colours::textDim, false, tr ("monitor.click.note")));

    for (auto* st : strips)
        addAndMakeVisible (st);

    // オフボ（B2）・自分の声とモニターリバーブ（B4）・お手本の声とクリック（2026-10-02）は音に効く。ハモリだけのお手本・自分の S は無いので、本物のアプリでは出さない（見本だけ）
    if (u->engineAttached)
    {
        // お手本：取り出した声（メインとハモリは分けられないので 1 本）。ハモリだけのお手本はまだ無い
        harmStrip->setVisible (false);
        selfStrip->setSoloShown (false);
        mainStrip->fader().setTooltip (tr ("monitor.guide.tooltip"));
        mainStrip->onFaderChange = [this] { session.setGuideLevel ((float) mainStrip->fader().getValue()); };
        mainStrip->muteKey().onClick = [this] { session.setGuideMuted (mainStrip->muteKey().getToggleState()); };
        mainStrip->soloKey().onClick = [this] { session.setGuideSolo (mainStrip->soloKey().getToggleState()); };
        mainStrip->soloKey().setTooltip (tr ("monitor.guide.solo"));
        backingStrip->soloKey().onClick = [this] { session.setBackingSolo (backingStrip->soloKey().getToggleState()); };
        backingStrip->soloKey().setTooltip (tr ("monitor.backing.solo"));
    }
    backingStrip->onFaderChange = [this] { session.setBackingLevel ((float) backingStrip->fader().getValue()); };
    backingStrip->muteKey().onClick = [this] { session.setBackingMuted (backingStrip->muteKey().getToggleState()); };
    selfStrip->onFaderChange = [this] { session.setSelfMonitorLevel ((float) selfStrip->fader().getValue()); };
    selfStrip->muteKey().onClick = [this] { session.setSelfMonitorMuted (selfStrip->muteKey().getToggleState()); };
    selfStrip->muteKey().setTooltip (tr ("monitor.self.mute.tooltip"));
    selfStrip->fader().setTooltip (tr ("monitor.self.tooltip"));
    reverbStrip->onFaderChange = [this] { session.setMonitorReverb ((float) reverbStrip->fader().getValue()); };
    reverbStrip->fader().setTooltip (tr ("monitor.reverb.tooltip"));
    clickStrip->onFaderChange = [this] { session.setClickLevel ((float) clickStrip->fader().getValue()); };
    clickStrip->fader().setTooltip (tr ("monitor.click.tooltip"));
    onSessionChanged (change::monitor | change::meter | change::takes);
}

MonitorModule::Notice MonitorModule::noticeFor (const dummy::Session& s)
{
    // スピーカーから自分の声を返すとハウリングする。ミュート中は理由を、鳴らしている時は注意を出す
    if (s.output.open && s.speakerOutput)
        return s.selfMuted ? Notice { tr ("monitor.notice.speakerMuted"), colours::warn }
                           : Notice { tr ("monitor.notice.speakerLive"), colours::bad };

    // 自分の声の遅れ（往復。実測・手入力があればそれ、無ければデバイスの申告値）。歌いにくいほど遅い時だけ
    if (s.inputLive() && ! s.selfMuted)
    {
        const auto ld = latencyDisplay (s);
        if (ld.known && ld.ms > lateMonitorMs)
            return { tr ("monitor.notice.late", juce::String (juce::roundToInt (ld.ms))), colours::warn };
    }
    return {};
}

void MonitorModule::updateMeters()
{
    // 自分のフェーダーの横：耳に返っている量（入力のピーク＋フェーダー）。-48〜0 dBFS を 0..1 に
    const auto& s = state();
    float level = -1.0f;
    if (s.engineAttached)
    {
        level = 0.0f;
        if (s.inputLive() && ! s.selfMuted && s.monitorGain > 0.0f)
        {
            const auto gainDb = juce::Decibels::gainToDecibels (audio::PlaybackCore::faderToGain (s.monitorGain), -100.0f);
            level = audio::meterFraction (s.inputPeakDb + gainDb);
        }
    }
    else
    {
        level = 0.70f;   // UI_MOCK：見本の値
    }
    selfStrip->fader().setMeter (level);

    // オフボ・お手本・クリック：エンジンがフェーダーの後で測った量（2026-10-02。ミュート・ソロ込み）。UI_MOCK は見本の値
    backingStrip->fader().setMeter (audio::meterFraction (s.backingMeterDb));
    mainStrip->fader().setMeter (audio::meterFraction (s.guideMeterDb));
    clickStrip->fader().setMeter (audio::meterFraction (s.clickMeterDb));
}

void MonitorModule::onSessionChanged (juce::uint32 c)
{
    if (c & change::mode)
        resized();

    if (c & change::monitor)
    {
        const auto& s = state();
        backingStrip->fader().setValue (s.offVocalGain, juce::dontSendNotification);
        backingStrip->muteKey().setToggleState (s.backingMuted, juce::dontSendNotification);
        selfStrip->fader().setValue (s.monitorGain, juce::dontSendNotification);
        selfStrip->muteKey().setToggleState (s.selfMuted, juce::dontSendNotification);
        reverbStrip->fader().setValue (s.monitorReverb, juce::dontSendNotification);
        clickStrip->fader().setValue (s.clickLevel, juce::dontSendNotification);
        if (s.engineAttached)
        {
            mainStrip->fader().setValue (s.mainGain, juce::dontSendNotification);
            mainStrip->muteKey().setToggleState (s.guideMuted, juce::dontSendNotification);
            mainStrip->soloKey().setToggleState (s.guideSolo, juce::dontSendNotification);
            backingStrip->soloKey().setToggleState (s.backingSolo, juce::dontSendNotification);
        }
    }

    if (state().engineAttached && (c & (change::monitor | change::takes | change::view)) != 0)
    {
        // お手本の声がまだ無い：触れない（お手本を入れると聴ける）
        const bool has = session.hasGuideVocals();
        mainStrip->setEnabled (has);
        mainStrip->setAlpha (has ? 1.0f : 0.4f);
    }

    if (c & (change::meter | change::monitor | change::device))
        updateMeters();

    if (c & (change::monitor | change::device))
    {
        // 知らせが出る・消える時は並べ直す
        const bool has = noticeFor (state()).text.isNotEmpty();
        if (has != hadNotice)
        {
            resized();
            repaint();   // 消えた知らせの跡も消す
        }
        else
        {
            repaint (noticeArea);
        }
    }
}

void MonitorModule::paint (juce::Graphics& g)
{
    RackModule::paint (g);

    const auto n = noticeFor (state());
    if (n.text.isEmpty() || noticeArea.isEmpty())
        return;

    auto r = noticeArea.toFloat();
    paint::led (g, { r.getX() + 3.0f, r.getCentreY() }, 2.6f, n.tone, true);
    g.setColour (n.tone);
    g.setFont (sans (10.5f));
    g.drawFittedText (n.text, r.withTrimmedLeft (11.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
}

void MonitorModule::resized()
{
    // 簡単モードはハモリのお手本を出さない（DESIGN 2）。本物のアプリではお手本の帯そのものを出さない（まだ鳴らせない）
    harmStrip->setVisible (state().mode != project::Mode::easy && ! state().engineAttached);

    int visible = 0;
    for (auto* st : strips) visible += st->isVisible() ? 1 : 0;

    auto r = content();
    hadNotice = noticeFor (state()).text.isNotEmpty();
    noticeArea = hadNotice ? r.removeFromBottom (28).withTrimmedTop (4) : juce::Rectangle<int>();
    const auto w = r.getWidth() / juce::jmax (1, visible);
    for (auto* st : strips)
        if (st->isVisible())
            st->setBounds (r.removeFromLeft (w).reduced (3, 0));
}

//==============================================================================
RecordModule::RecordModule (UiSession& u)
    : RackModule (tr ("rack.record"), tr ("rack.record.sub")), SessionView (u),
      recMode ({ tr ("record.delivery"), tr ("record.practice") }, (int) u->recMode, colours::rec)
{
    recMode.onChange = [this] (int i) { session.setRecMode ((project::RecMode) i); };
    addAndMakeVisible (recMode);
}

void RecordModule::onSessionChanged (juce::uint32 changes)
{
    if (changes & (change::practice | change::transport | change::mode))
    {
        recMode.setSelected ((int) state().recMode, juce::dontSendNotification);
        repaint();
    }
}

void RecordModule::resized()
{
    auto r = content();
    recMode.setBounds (r.removeFromTop (34));
    r.removeFromTop (10);
    targetArea = r.removeFromTop (52);
    r.removeFromTop (8);
    lockArea = r.removeFromTop (16);
}

void RecordModule::paint (juce::Graphics& g)
{
    RackModule::paint (g);
    const auto& s = state();
    const bool delivery = s.recMode == project::RecMode::delivery;
    const bool rec = s.isRecording;

    auto t = targetArea.toFloat();
    paint::inset (g, t);
    if (rec)
    {
        g.setColour (colours::rec.withAlpha (0.08f));
        g.fillRoundedRectangle (t.reduced (1.0f), metrics::windowRadius);
    }

    auto inner = t.reduced (10.0f, 6.0f);
    auto top = inner.removeFromTop (14.0f);
    paint::led (g, { top.getX() + 3.0f, top.getCentreY() }, 2.6f, colours::rec, rec);
    paint::microLabel (g, top.withTrimmedLeft (11.0f), rec ? tr ("record.recordingTo") : tr ("record.target"),
                       rec ? colours::rec : colours::textMute);

    g.setColour (colours::text);
    g.setFont (sans (13.5f, Weight::semibold));
    const auto target = ! delivery ? tr ("record.target.practice")
                      : (s.mode == project::Mode::easy ? tr ("record.target.deliveryWhole") : tr ("record.target.delivery"));
    g.drawText (target, inner.removeFromTop (18.0f), juce::Justification::centredLeft, true);

    g.setColour (colours::textDim);
    g.setFont (mono (10.5f));
    g.drawText (tr ("record.format", formatKhz (s.sampleRate()), formatBits (s.project.bitDepthExport),
                    formatTime (s.project.lengthSamples, s.sampleRate(), false)),
                inner, juce::Justification::centredLeft, true);

    auto l = lockArea.toFloat();
    const auto lc = session.deliveryLocked() ? colours::warn : colours::textMute;
    drawIcon (g, Icon::lock, l.removeFromLeft (14.0f).withSizeKeepingCentre (11.0f, 11.0f), lc);
    l.removeFromLeft (6.0f);
    g.setColour (lc);
    g.setFont (sans (10.5f));
    g.drawText (tr ("record.lockNote"), l, juce::Justification::centredLeft, true);
}

//==============================================================================
Rack::Rack (UiSession& u, Actions& a)
    : SessionView (u), input (u, a), practice (u, a), monitor (u), record (u)
{
    addAndMakeVisible (input);
    addAndMakeVisible (practice);
    addAndMakeVisible (monitor);
    addAndMakeVisible (record);
}

void Rack::resized()
{
    // 既定の高さ（合計 756）と最小の高さ。足りない分は (既定 - 最小) の比で各段から削る
    constexpr int desired[] = { 156, 214, 218, 168 };   // 練習：声域とおすすめのキーの段（2026-10-02）
    constexpr int minimum[] = { 140, 190, 170, 146 };
    constexpr int desiredSum = 156 + 214 + 218 + 168, slackSum = 16 + 24 + 48 + 22;

    auto r = getLocalBounds().withTrimmedLeft (1);
    const auto deficit = juce::jlimit (0, slackSum, desiredSum - r.getHeight());
    int h[4];
    for (int i = 0; i < 4; ++i)
        h[i] = desired[i] - deficit * (desired[i] - minimum[i]) / slackSum;

    input.setBounds (r.removeFromTop (h[0]));
    practice.setBounds (r.removeFromTop (h[1]));
    record.setBounds (r.removeFromBottom (h[3]));
    monitor.setBounds (r);   // 余りはすべてモニター
}

void Rack::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    paint::vline (g, 0.0f, 0.0f, (float) getHeight(), state().isRecording ? colours::rec : colours::line);

    for (auto* m : std::initializer_list<juce::Component*> { &practice, &monitor, &record })
        paint::hline (g, (float) m->getY(), 1.0f, (float) getWidth());
}
} // namespace vb
