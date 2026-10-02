#include "SetupWizard.h"
#include "audio/Resample.h"
#include "../Timeline.h"
#include "audio/DeviceRules.h"
#include "audio/InputMeter.h"

namespace vb
{
namespace
{
    constexpr int stepperH = 44;
    constexpr int listRowH = 40, listGap = 4, listRows = 3;
    constexpr int infoRowH = 30, micRowH = 40, labelW = 150;

    struct DeviceRow { const char* name; const char* detail; bool selected; bool warnLatency; };

    // UI_MOCK の固定データ。機器名はデータ（翻訳しない）
    const DeviceRow inputs[] = {
        { "USB Audio Interface \xe2\x80\x94 In 1", "2ch  48kHz", true,  false },
        { "Built-in Microphone",                    "1ch  48kHz", false, false },
        { "Bluetooth Headset",                      "1ch  16kHz", false, true  },
    };
    const DeviceRow outputs[] = {
        { "USB Audio Interface \xe2\x80\x94 Out 1/2", "2ch  48kHz", true,  false },
        { "Built-in Output",                          "2ch  48kHz", false, false },
    };

    /** デバイスの 1 行（選択中はキーキャップ＋LED、Bluetooth らしければ遅延警告） */
    void paintDeviceRow (juce::Graphics& g, juce::Rectangle<float> row, const juce::String& name, const juce::String& detail,
                         bool selected, bool warnLatency, bool hover)
    {
        if (selected)
        {
            paint::keycap (g, row, { hover, false, true, true });
        }
        else
        {
            paint::inset (g, row);
            if (hover)
            {
                g.setColour (colours::raisedHi.withAlpha (0.5f));
                g.fillRoundedRectangle (row.reduced (1.0f), metrics::windowRadius);
            }
        }

        auto c = row.reduced (12.0f, 0.0f);
        paint::led (g, { c.getX() + 4.0f, c.getCentreY() }, 3.0f, colours::signal, selected);
        c.removeFromLeft (16.0f);

        if (warnLatency)
        {
            const auto wt = tr ("setup.device.btWarning");
            const auto wf = sans (10.5f, Weight::medium);
            auto chip = c.removeFromRight (textWidth (wf, wt) + 30.0f).withSizeKeepingCentre (textWidth (wf, wt) + 30.0f, 20.0f);
            g.setColour (colours::warn.withAlpha (0.14f));
            g.fillRoundedRectangle (chip, 3.0f);
            drawIcon (g, Icon::warning, chip.removeFromLeft (22.0f).withSizeKeepingCentre (12.0f, 12.0f), colours::warn);
            g.setColour (colours::warn);
            g.setFont (wf);
            g.drawText (wt, chip, juce::Justification::centredLeft, false);
        }

        if (detail.isNotEmpty())
        {
            g.setColour (colours::textMute);
            g.setFont (mono (10.5f));
            g.drawText (detail, c.removeFromRight (90.0f), juce::Justification::centredRight, false);
        }
        g.setColour (selected ? colours::text : colours::textDim);
        g.setFont (sansFor (name, 12.5f, selected ? Weight::medium : Weight::regular));
        g.drawText (name, c, juce::Justification::centredLeft, true);
    }

    juce::String firstLine (const juce::String& s)
    {
        return s.upToFirstOccurrenceOf ("\n", false, false).trim();
    }

    // 実デバイスの画面の配置（layoutBody と paintBody で同じものを使う）
    /** レイテンシ（Step 3）の並び。layoutBody と paintBody で同じ形にする */
    constexpr int latencyLabelW = 74;
    struct LatencyLayout
    {
        juce::Rectangle<int> instruction, sub, button, status, box, manual, note;

        explicit LatencyLayout (juce::Rectangle<int> r)
        {
            instruction = r.removeFromTop (26);
            sub = r.removeFromTop (34);
            r.removeFromTop (12);
            status = r.removeFromTop (34);
            button = status;
            r.removeFromTop (16);
            box = r.removeFromTop (78);
            r.removeFromTop (14);
            manual = r.removeFromTop (30);
            r.removeFromTop (14);
            note = r.removeFromTop (40);
        }
    };

    enum InfoRow { rowDriver, rowChannel, rowRate, rowFormat, rowBuffer, rowLatency, rowMic, numInfoRows };

    struct LiveLayout
    {
        juce::Rectangle<int> inHead, outHead, inList, outList, error;
        juce::Rectangle<int> rows[numInfoRows];

        explicit LiveLayout (juce::Rectangle<int> r)
        {
            r.removeFromTop (stepperH + 18);
            auto lists = r.removeFromTop (18 + 4 + listRows * listRowH + (listRows - 1) * listGap);
            auto left = lists.removeFromLeft (lists.getWidth() / 2 - 10);
            lists.removeFromLeft (20);
            inHead = left.removeFromTop (18);
            left.removeFromTop (4);
            inList = left;
            outHead = lists.removeFromTop (18);
            lists.removeFromTop (4);
            outList = lists;

            r.removeFromTop (14);
            for (int i = 0; i < numInfoRows; ++i)
                rows[i] = r.removeFromTop (i == rowMic ? micRowH : infoRowH);
            r.removeFromTop (4);
            error = r.removeFromTop (22);
        }

        /** 行の値の位置に置く部品（幅 w、高さ 26） */
        juce::Rectangle<int> control (int row, int w) const
        {
            auto a = rows[row].withTrimmedLeft (labelW);
            return a.removeFromLeft (juce::jmin (w, a.getWidth())).withSizeKeepingCentre (juce::jmin (w, a.getWidth()), 26);
        }
    };
}

//==============================================================================
/** 入力 / 出力デバイスの一覧（実デバイス）。クリックで選ぶ。入りきらなければホイールで送る */
class DeviceListView : public juce::Component
{
public:
    struct Row
    {
        juce::String name, detail;
        bool selected = false, bluetooth = false;
    };

    explicit DeviceListView (std::vector<Row> r) : rows (std::move (r)) {}

    std::function<void (int)> onSelect;

    void resized() override
    {
        // 選んでいる行が見えるように
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].selected)
                scroll = juce::jlimit (0, maxScroll(), i * (listRowH + listGap) - (getHeight() - listRowH) / 2);
        scroll = juce::jlimit (0, maxScroll(), scroll);
    }

    void paint (juce::Graphics& g) override
    {
        if (rows.empty())
        {
            paint::inset (g, getLocalBounds().withHeight (listRowH).toFloat());
            g.setColour (colours::textMute);
            g.setFont (sans (12.0f));
            g.drawText (tr ("setup.device.none"), getLocalBounds().withHeight (listRowH).reduced (14, 0), juce::Justification::centredLeft, true);
            return;
        }

        for (int i = 0; i < (int) rows.size(); ++i)
        {
            const auto rb = rowBounds (i);
            if (rb.getBottom() < 0.0f || rb.getY() > (float) getHeight())
                continue;
            const auto& row = rows[(size_t) i];
            paintDeviceRow (g, rb, row.name, row.detail, row.selected, row.bluetooth, i == hover);
        }

        // 入りきらない時だけ細いつまみ
        if (maxScroll() > 0)
        {
            const auto h = (float) getHeight();
            const auto total = (float) contentHeight();
            const auto thumbH = juce::jmax (16.0f, h * h / total);
            const auto y = (h - thumbH) * (float) scroll / (float) maxScroll();
            g.setColour (colours::lineHi);
            g.fillRoundedRectangle ({ (float) getWidth() - 4.0f, y, 3.0f, thumbH }, 1.5f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (indexAt (e.y)); }
    void mouseExit (const juce::MouseEvent&) override   { setHover (-1); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto i = indexAt (e.y);
        if (e.mouseWasClicked() && i >= 0 && ! rows[(size_t) i].selected && onSelect)
            onSelect (i);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (maxScroll() <= 0)
        {
            Component::mouseWheelMove (e, w);
            return;
        }
        scroll = juce::jlimit (0, maxScroll(), scroll - juce::roundToInt (w.deltaY * 160.0f));
        setHover (indexAt (e.y));
        repaint();
    }

private:
    int contentHeight() const { return (int) rows.size() * (listRowH + listGap) - listGap; }
    int maxScroll() const     { return juce::jmax (0, contentHeight() - getHeight()); }

    juce::Rectangle<float> rowBounds (int i) const
    {
        const auto w = getWidth() - (maxScroll() > 0 ? 8 : 0);
        return juce::Rectangle<int> (0, i * (listRowH + listGap) - scroll, w, listRowH).toFloat();
    }

    int indexAt (int y) const
    {
        const auto i = (y + scroll) / (listRowH + listGap);
        const auto inRow = (y + scroll) % (listRowH + listGap) < listRowH;
        return inRow && juce::isPositiveAndBelow (i, (int) rows.size()) ? i : -1;
    }

    void setHover (int i)
    {
        if (i == hover) return;
        hover = i;
        setMouseCursor (i >= 0 && ! rows[(size_t) i].selected ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    std::vector<Row> rows;
    int scroll = 0, hover = -1;
};

//==============================================================================
SetupWizard::SetupWizard (UiSession& u, int initialStep)
    : DialogPanel (tr ("setup.title"), tr ("setup.micro")), SessionView (u),
      measure (tr ("setup.latency.measure")),
      manualUse (tr ("setup.latency.manual.use")),
      manualClear (tr ("setup.latency.manual.clear"))
{
    next = &addFooterKey (tr ("setup.next"), KeyRole::primary, [this]
    {
        if (step < 2) setStep (step + 1);
        else if (onCloseRequest) onCloseRequest();
    });
    back = &addFooterKey (tr ("setup.back"), KeyRole::normal, [this] { setStep (juce::jmax (0, step - 1)); });

    meter.setLevels (u->inputPeakDb, u->inputRmsDb, u->inputPeakHoldDb, u->inputClipped);
    meter.onClick = [this] { session.resetInputClip(); };
    meter.setTooltip (tr ("meter.clip.tooltip"));
    addChildComponent (meter);

    measure.withIcon (Icon::metronome);
    measure.onClick = [this] { session.measureLatency(); };
    addChildComponent (measure);

    // 手入力（測れない時）。0〜1000 ms、小数 1 桁まで
    manualField.setIndents (10, 6);
    manualField.setJustification (juce::Justification::centredLeft);
    manualField.setSelectAllWhenFocused (true);
    manualField.setScrollbarsShown (false);
    manualField.setFont (mono (14.0f, Weight::semibold));
    manualField.setInputRestrictions (6, "0123456789.,");
    manualField.setTextToShowWhenEmpty ("-", colours::textMute);
    manualField.onReturnKey = [this] { commitManual(); };
    manualField.setTooltip (tr ("setup.latency.manual.tooltip"));
    addChildComponent (manualField);
    manualUse.onClick = [this] { commitManual(); };
    manualClear.onClick = [this] { session.clearLatencyManual(); };
    addChildComponent (manualUse);
    addChildComponent (manualClear);
    refreshLatencyControls();

    if (live())
    {
        session.rescanDevices();     // 抜き差しを知らせないドライバ（ALSA など）のため
        rebuildDeviceControls();
    }

    setSize (780, live() ? 630 : 560);
    setStep (initialStep);
}

SetupWizard::~SetupWizard() = default;

void SetupWizard::later (std::function<void (SetupWizard&)> f)
{
    juce::Component::SafePointer<SetupWizard> safe (this);
    juce::MessageManager::callAsync ([safe, f] { if (safe != nullptr) f (*safe); });
}

void SetupWizard::selected (const juce::String& error)
{
    selectError = error;
    repaint();
}

void SetupWizard::commitManual()
{
    const auto text = manualField.getText().trim().replace (",", ".");
    if (text.isEmpty() || ! live())
        return;
    session.setLatencyManualMs (text.getDoubleValue());
    refreshLatencyControls();
}

void SetupWizard::refreshLatencyControls()
{
    const auto& s = state();
    const bool ready = ! live() || (s.input.open && s.output.open);
    measure.setEnabled (ready && ! s.latencyMeasuring && ! s.isRecording);
    measure.setButtonText (s.latencyMeasuring ? tr ("setup.latency.measuring") : tr ("setup.latency.measure"));

    const auto ld = latencyDisplay (s);
    manualField.setEnabled (live() && s.input.open && ! s.latencyMeasuring);
    manualUse.setEnabled (manualField.isEnabled());
    if (! manualField.hasKeyboardFocus (true))
        manualField.setText (ld.manual ? juce::String (ld.ms, 1) : juce::String(), false);
    manualClear.setVisible (step == 2 && ld.manual);
    resized();
}

void SetupWizard::onSessionChanged (juce::uint32 c)
{
    const auto& s = state();
    if (c & (change::latency | change::device | change::transport))
    {
        refreshLatencyControls();
        if (step == 2)
            repaint();
    }
    if (c & (change::meter | change::device))
    {
        meter.setLevels (s.inputPeakDb, s.inputRmsDb, s.inputPeakHoldDb, s.inputClipped);
        if (step == 1)
            repaint();
    }
    if ((c & (change::device | change::mode)) && live())
    {
        rebuildDeviceControls();
        updateNextKey();
        repaint();
    }
}

void SetupWizard::rebuildDeviceControls()
{
    devices = session.getDeviceList();
    const auto& s = state();
    const auto& in = s.input;

    // 中身が同じなら作り直さない（30 Hz の通知や 1 秒ごとの見直しで部品がちらつかないように）
    juce::String key;
    key << devices.types.joinIntoString ("|") << "#" << devices.currentType
        << "#" << devices.inputs.joinIntoString ("|") << "#" << devices.outputs.joinIntoString ("|")
        << "#" << devices.currentInput << "#" << devices.currentOutput
        << "#" << in.numChannels << "#" << in.channel << "#" << (int) in.open
        << "#" << devices.bufferSize << "#" << devices.sampleRate << "#" << (int) s.mode
        << "#" << s.recordRate << "#" << (int) s.recordFloat << "#" << s.songRate;
    for (auto b : devices.bufferSizes) key << "," << b;
    if (key == devicesKey)
        return;
    devicesKey = key;

    const auto khz = formatKhz (juce::roundToInt (devices.sampleRate)) + "kHz";

    // 入力 / 出力の一覧
    {
        std::vector<DeviceListView::Row> rows;
        for (auto& name : devices.inputs)
        {
            DeviceListView::Row r;
            r.name = name;
            r.selected = name == devices.currentInput;
            r.bluetooth = audio::looksLikeBluetooth (name);
            if (r.selected && in.open)
                r.detail = (in.numChannels > 0 ? juce::String (in.numChannels) + "ch  " : juce::String()) + khz;
            rows.push_back (r);
        }
        inputList = std::make_unique<DeviceListView> (std::move (rows));
        const auto names = devices.inputs;
        inputList->onSelect = [this, names] (int i)
        {
            const auto name = names[i];
            later ([name] (SetupWizard& w) { w.selected (w.session.selectInputDevice (name)); });
        };
        addChildComponent (*inputList);
    }
    {
        std::vector<DeviceListView::Row> rows;
        for (auto& name : devices.outputs)
        {
            DeviceListView::Row r;
            r.name = name;
            r.selected = name == devices.currentOutput;
            r.bluetooth = audio::looksLikeBluetooth (name);
            if (r.selected && s.output.open)
                r.detail = khz;
            rows.push_back (r);
        }
        outputList = std::make_unique<DeviceListView> (std::move (rows));
        const auto names = devices.outputs;
        outputList->onSelect = [this, names] (int i)
        {
            const auto name = names[i];
            later ([name] (SetupWizard& w) { w.selected (w.session.selectOutputDevice (name)); });
        };
        addChildComponent (*outputList);
    }

    // ドライバ（WASAPI / DirectSound / ASIO / Core Audio / ALSA …）
    driverPick.reset();
    if (devices.types.size() > 1)
    {
        driverPick = std::make_unique<Dropdown> (devices.types, juce::jmax (0, devices.types.indexOf (devices.currentType)));
        driverPick->setFont (sans (12.0f, Weight::medium));
        const auto types = devices.types;
        driverPick->onChange = [this, types] (int i)
        {
            const auto type = types[i];
            later ([type] (SetupWizard& w) { w.selected (w.session.selectDeviceType (type)); });
        };
        addChildComponent (*driverPick);
    }

    // 入力チャンネル（モノラルで録る。既定 L。DESIGN 13）
    channelKeys.reset();
    channelPick.reset();
    if (in.open && in.numChannels >= 2)
    {
        juce::StringArray labels;
        for (int i = 0; i < in.numChannels; ++i)
            labels.add (inputChannelLabel (i, in.numChannels));

        auto choose = [this] (int i) { later ([i] (SetupWizard& w) { w.selected (w.session.selectInputChannel (i)); }); };
        if (in.numChannels <= 8)
        {
            channelKeys = std::make_unique<SegmentedKeys> (labels, in.channel);
            channelKeys->setFont (sans (12.0f, Weight::medium));
            channelKeys->onChange = choose;
            addChildComponent (*channelKeys);
        }
        else
        {
            channelPick = std::make_unique<Dropdown> (labels, in.channel);
            channelPick->setFont (sans (12.0f, Weight::medium));
            channelPick->onChange = choose;
            addChildComponent (*channelPick);
        }
    }

    // バッファ（プロだけ選べる。DESIGN 2 / 5）
    bufferPick.reset();
    if (s.mode == project::Mode::pro && ! devices.bufferSizes.isEmpty())
    {
        juce::StringArray items;
        for (auto b : devices.bufferSizes)
            items.add (juce::String (b) + " smp");
        bufferPick = std::make_unique<Dropdown> (items, juce::jmax (0, devices.bufferSizes.indexOf (devices.bufferSize)));
        bufferPick->setFont (mono (12.0f));
        const auto sizes = devices.bufferSizes;
        bufferPick->onChange = [this, sizes] (int i)
        {
            const auto size = sizes[i];
            later ([size] (SetupWizard& w) { w.selected (w.session.selectBufferSize (size)); });
        };
        addChildComponent (*bufferPick);
    }

    // 録音形式（2026-10-01）：SR はこの機器で録れる値だけ（44.1〜384 kHz）、ビット数は 24bit / 32bit float。簡単は自動
    ratePick.reset();
    bitKeys.reset();
    if (s.mode != project::Mode::easy)
    {
        rateChoices.clear();
        juce::StringArray items;
        items.add (s.songRate > 0 ? tr ("setup.device.format.songRate", formatKhz (s.songRate))
                                  : tr ("setup.device.format.songRateUnknown"));
        rateChoices.add (0.0);
        for (auto r : audio::recordingRates())
            if (devices.sampleRates.contains (r) || std::abs (r - s.recordRate) < 0.5)
            {
                items.add (formatKhz (juce::roundToInt (r)) + " kHz");
                rateChoices.add (r);
            }
        int current = 0;
        for (int i = 0; i < rateChoices.size(); ++i)
            if (std::abs (rateChoices[i] - s.recordRate) < 0.5)
                current = i;
        ratePick = std::make_unique<Dropdown> (items, current);
        ratePick->setFont (mono (12.0f));
        const auto choices = rateChoices;
        ratePick->onChange = [this, choices] (int i)
        {
            const auto rate = choices[i];
            later ([rate] (SetupWizard& w) { w.session.setRecordFormat (rate, w.state().recordFloat); });
        };
        addChildComponent (*ratePick);

        bitKeys = std::make_unique<SegmentedKeys> (juce::StringArray { "24bit", "32bit float" }, s.recordFloat ? 1 : 0);
        bitKeys->setFont (mono (11.5f));
        bitKeys->onChange = [this] (int i)
        {
            later ([i] (SetupWizard& w) { w.session.setRecordFormat (w.state().recordRate, i == 1); });
        };
        addChildComponent (*bitKeys);
    }

    if (getWidth() > 0)
        setStep (step);   // 見せる / 隠す・配置
}

void SetupWizard::updateNextKey()
{
    // Mac でマイクの許可が無い間は先へ進めない（DESIGN 13「Mac 権限なし：セットアップで止める」）
    const auto p = state().input.permission;
    const bool blocked = live() && step == 0 && (p == audio::MicPermission::denied || p == audio::MicPermission::asking);
    next->setEnabled (! blocked);
}

void SetupWizard::setStep (int s)
{
    step = juce::jlimit (0, 2, s);
    back->setEnabled (step > 0);
    next->setButtonText (step == 2 ? tr ("setup.finish") : tr ("setup.next"));
    meter.setVisible (step == 1);
    measure.setVisible (step == 2);
    manualField.setVisible (step == 2);
    manualUse.setVisible (step == 2);
    manualClear.setVisible (false);
    refreshLatencyControls();

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             inputList.get(), outputList.get(), driverPick.get(), channelKeys.get(), channelPick.get(), bufferPick.get(),
             ratePick.get(), bitKeys.get() })
        if (c != nullptr)
            c->setVisible (step == 0);

    updateNextKey();
    resized();
    repaint();
}

void SetupWizard::layoutBody (juce::Rectangle<int> body)
{
    auto r = body;
    r.removeFromTop (stepperH + 18);

    if (step == 0 && live())
    {
        const LiveLayout l (body);
        if (inputList != nullptr)  inputList->setBounds (l.inList);
        if (outputList != nullptr) outputList->setBounds (l.outList);
        if (driverPick != nullptr) driverPick->setBounds (l.control (rowDriver, juce::jmin (300, driverPick->idealWidth())));
        if (channelKeys != nullptr) channelKeys->setBounds (l.control (rowChannel, channelKeys->idealWidth()));
        if (channelPick != nullptr) channelPick->setBounds (l.control (rowChannel, juce::jmin (220, channelPick->idealWidth())));
        if (bufferPick != nullptr) bufferPick->setBounds (l.control (rowBuffer, juce::jmin (180, bufferPick->idealWidth())));
        if (ratePick != nullptr)
        {
            const auto rw = juce::jmin (230, ratePick->idealWidth());
            ratePick->setBounds (l.control (rowFormat, rw));
            if (bitKeys != nullptr)
            {
                auto a = l.rows[rowFormat].withTrimmedLeft (labelW + rw + 10);
                const auto bw = juce::jmin (a.getWidth(), bitKeys->idealWidth());
                bitKeys->setBounds (a.removeFromLeft (bw).withSizeKeepingCentre (bw, 26));
            }
        }
    }
    if (step == 1)
    {
        r.removeFromTop (70);
        meterArea = r.removeFromTop (44);
        meter.setBounds (meterArea);
    }
    if (step == 2)
    {
        const LatencyLayout l (r);
        measure.setSize (10, 34);
        measure.setBounds (l.button.withWidth (juce::jmax (measure.idealWidth(), 132)));
        auto m = l.manual.withTrimmedLeft (latencyLabelW);
        manualField.setBounds (m.removeFromLeft (92));
        m.removeFromLeft (34);   // 「ms」
        manualUse.setBounds (m.removeFromLeft (manualUse.idealWidth()));
        m.removeFromLeft (8);
        manualClear.setBounds (m.removeFromLeft (manualClear.idealWidth()));
    }
}

void SetupWizard::paintBody (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto body = r;
    paintStepper (g, r.removeFromTop (stepperH));
    r.removeFromTop (18);

    switch (step)
    {
        case 0:  if (live()) paintDeviceLive (g, body); else paintDevice (g, r); break;
        case 1:  paintLevel (g, r);   break;
        default: paintLatency (g, r); break;
    }
}

void SetupWizard::paintStepper (juce::Graphics& g, juce::Rectangle<int> r)
{
    const char* names[] = { "setup.step.device", "setup.step.level", "setup.step.latency" };
    const auto w = r.getWidth() / 3;

    for (int i = 0; i < 3; ++i)
    {
        auto cell = r.removeFromLeft (w).toFloat().reduced (2.0f, 4.0f);
        const bool active = i == step, done = i < step;

        if (active) paint::keycap (g, cell, { false, false, true, true });
        else        paint::inset (g, cell);

        auto c = cell.reduced (12.0f, 0.0f);
        const auto num = c.removeFromLeft (22.0f).withSizeKeepingCentre (20.0f, 20.0f);
        g.setColour (done ? colours::signal : (active ? colours::text : colours::line));
        g.fillEllipse (num);
        g.setColour (colours::bgDeep);
        if (done) drawIcon (g, Icon::check, num.reduced (4.0f), colours::onFill (colours::signal));
        else
        {
            g.setFont (mono (11.0f, Weight::semibold));
            g.drawText (juce::String (i + 1), num, juce::Justification::centred, false);
        }
        c.removeFromLeft (10.0f);
        g.setColour (active ? colours::text : colours::textDim);
        g.setFont (sans (13.0f, active ? Weight::semibold : Weight::regular));
        g.drawText (tr (names[i]), c, juce::Justification::centredLeft, true);
    }
}

void SetupWizard::paintDevice (juce::Graphics& g, juce::Rectangle<int> r)
{
    // UI_MOCK：静的モック
    auto list = [&] (const juce::String& heading, const DeviceRow* rows, size_t n, juce::Rectangle<int> area)
    {
        paint::microLabel (g, area.removeFromTop (18).toFloat(), heading, colours::textMute);
        area.removeFromTop (4);
        for (size_t i = 0; i < n; ++i)
        {
            auto row = area.removeFromTop (40).toFloat();
            area.removeFromTop (4);
            paintDeviceRow (g, row, utf8 (rows[i].name), rows[i].detail, rows[i].selected, rows[i].warnLatency, false);
        }
    };

    auto cols = r;
    auto left = cols.removeFromLeft (cols.getWidth() / 2 - 10);
    cols.removeFromLeft (20);
    list (tr ("setup.device.input"), inputs, std::size (inputs), left.removeFromTop (160));
    list (tr ("setup.device.output"), outputs, std::size (outputs), cols.removeFromTop (160));

    r.removeFromTop (176);
    const auto& s = state();
    auto info = [&] (const juce::String& label, const juce::String& value, juce::Colour c)
    {
        auto row = r.removeFromTop (24).toFloat();
        paint::microLabel (g, row.removeFromLeft (150.0f), label, colours::textMute);
        g.setColour (c);
        g.setFont (sans (12.0f));
        g.drawText (value, row, juce::Justification::centredLeft, true);
    };
    info (tr ("setup.device.driver"), tr ("setup.device.driver.value", s.driver), colours::text);
    info (tr ("setup.device.buffer"), s.mode == project::Mode::pro ? tr ("setup.device.buffer.pro", s.bufferSize)
                                                                  : tr ("setup.device.buffer.auto", s.bufferSize), colours::text);
    info (tr ("setup.device.permission"), tr ("setup.device.permission.ok"), colours::signal);
}

void SetupWizard::paintDeviceLive (juce::Graphics& g, juce::Rectangle<int> body)
{
    const LiveLayout l (body);
    const auto& s = state();
    const auto& in = s.input;

    paint::microLabel (g, l.inHead.toFloat(), tr ("setup.device.input"), colours::textMute);
    paint::microLabel (g, l.outHead.toFloat(), tr ("setup.device.output"), colours::textMute);

    // 行：見出し＋値（部品がある行は、部品の右に補足）
    auto row = [&] (int index, const juce::String& label, const juce::String& value, juce::Colour c, juce::Component* control = nullptr)
    {
        auto a = l.rows[index].toFloat();
        paint::microLabel (g, a.removeFromLeft ((float) labelW).withHeight ((float) infoRowH), label, colours::textMute);
        if (control != nullptr)
            a.setLeft ((float) control->getRight() + 12.0f);
        if (value.isEmpty())
            return;
        g.setColour (c);
        g.setFont (sans (12.0f));
        if (index == rowMic)
            g.drawFittedText (value, a.toNearestInt().withTrimmedTop (7), juce::Justification::topLeft, 2, 1.0f);
        else
            g.drawText (value, a, juce::Justification::centredLeft, true);
    };

    // ドライバ（ASIO は SDK のあるビルドのみ。DESIGN 11.1）
    {
        juce::String note;
       #if JUCE_WINDOWS && ! JUCE_ASIO
        note = tr ("setup.device.driver.asioNote");   // ASIO を入れずにビルドした時だけ
       #endif
        if (driverPick != nullptr) row (rowDriver, tr ("setup.device.driver"), note, colours::textMute, driverPick.get());
        else                       row (rowDriver, tr ("setup.device.driver"), devices.currentType, colours::text);
    }

    // チャンネル（1 ch だけ・モノラルで録る）
    if (channelKeys != nullptr)      row (rowChannel, tr ("setup.device.channel"), tr ("setup.device.channel.note"), colours::textMute, channelKeys.get());
    else if (channelPick != nullptr) row (rowChannel, tr ("setup.device.channel"), tr ("setup.device.channel.note"), colours::textMute, channelPick.get());
    else row (rowChannel, tr ("setup.device.channel"), in.open ? tr ("setup.device.channel.mono") : juce::String ("-"), colours::text);

    // SR（いま / 対応）
    {
        juce::StringArray rates;
        for (auto sr : devices.sampleRates)
            rates.add (formatKhz (juce::roundToInt (sr)));
        const auto value = devices.sampleRate > 0.0
                         ? tr ("setup.device.rate.value", formatKhz (juce::roundToInt (devices.sampleRate)), rates.joinIntoString (" / "))
                         : juce::String ("-");
        row (rowRate, tr ("setup.device.rate"), value, colours::text);
    }

    // 録音形式（SR・ビット数）。選べる時は部品、簡単は自動の説明
    if (ratePick != nullptr)
    {
        const auto controlRight = bitKeys != nullptr ? bitKeys.get() : static_cast<juce::Component*> (ratePick.get());
        row (rowFormat, tr ("setup.device.format"), tr ("setup.device.format.note"), colours::textMute, controlRight);
    }
    else
    {
        row (rowFormat, tr ("setup.device.format"), tr ("setup.device.format.auto", formatBits (s.project.bitDepthExport)), colours::text);
    }

    // バッファ（プロは選べる。ほかは自動）
    if (bufferPick != nullptr) row (rowBuffer, tr ("setup.device.buffer"), {}, colours::text, bufferPick.get());
    else row (rowBuffer, tr ("setup.device.buffer"), tr ("setup.device.buffer.auto", devices.bufferSize), colours::text);

    // レイテンシ（録音位置の補正に使う値。手入力 → 実測 → 申告値。測るのは Step 3）
    {
        const auto ld = latencyDisplay (s);
        const auto value = ! ld.known    ? juce::String ("-")
                         : ld.manual     ? tr ("setup.device.latency.manual", juce::String (ld.ms, 1), ld.samples)
                         : ! ld.reported ? tr ("setup.device.latency.measured", juce::String (ld.ms, 1), ld.samples)
                         : ld.estimated  ? tr ("setup.device.latency.estimated", juce::String (ld.ms, 1), ld.samples)
                                         : tr ("setup.device.latency.value", juce::String (ld.ms, 1), ld.samples);
        row (rowLatency, tr ("setup.device.latency"), value, colours::text);
    }

    // マイクの許可・入力の状態
    {
        juce::String text;
        auto c = colours::signal;
        using audio::MicPermission;
        if (in.permission == MicPermission::denied)        { text = tr ("setup.device.permission.denied"); c = colours::bad; }
        else if (in.permission == MicPermission::asking)   { text = tr ("setup.device.permission.asking"); c = colours::warn; }
        else if (in.open && in.silent)                     { text = tr ("setup.device.permission.silent"); c = colours::warn; }
        else if (in.open)                                  text = in.permission == MicPermission::granted ? tr ("setup.device.permission.ok")
                                                                                                          : tr ("setup.device.permission.open");
        else if (in.problem == audio::InputProblem::stalled)    { text = tr ("setup.device.permission.stalled"); c = colours::bad; }
        else if (in.problem == audio::InputProblem::openFailed) { text = tr ("setup.device.permission.failed", firstLine (in.error)); c = colours::bad; }
        else if (in.problem == audio::InputProblem::noChannels) { text = tr ("setup.device.permission.noChannels"); c = colours::bad; }
        else                                               { text = tr ("setup.device.permission.noDevice"); c = colours::warn; }
        row (rowMic, tr ("setup.device.permission"), text, c);
    }

    if (selectError.isNotEmpty())
    {
        auto e = l.error.toFloat();
        drawIcon (g, Icon::warning, e.removeFromLeft (16.0f).withSizeKeepingCentre (12.0f, 12.0f), colours::bad);
        e.removeFromLeft (6.0f);
        g.setColour (colours::bad);
        g.setFont (sans (11.5f));
        g.drawText (tr ("setup.device.selectError", firstLine (selectError)), e, juce::Justification::centredLeft, true);
    }
}

void SetupWizard::paintLevel (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& s = state();
    g.setColour (colours::text);
    g.setFont (sans (15.0f, Weight::semibold));
    g.drawText (tr ("setup.level.instruction"), r.removeFromTop (26), juce::Justification::centredLeft, true);
    g.setColour (colours::textDim);
    g.setFont (sans (12.0f));
    g.drawText (tr ("setup.level.sub"), r.removeFromTop (22), juce::Justification::centredLeft, true);

    r.removeFromTop (22 + 44 + 20);

    // 判定（-3 超: 下げて / -20 未満: 上げて / それ以外: OK。DESIGN 5）
    // 実デバイスはホールド（1.5 秒の最大）で見る（歌の合間で判定が揺れないように）
    juce::String text;
    auto c = colours::signal;
    if (live() && ! s.input.open)
    {
        text = tr ("setup.level.noInput");
        c = colours::bad;
    }
    else if (live() && s.input.silent)
    {
        text = tr ("setup.device.permission.silent");
        c = colours::warn;
    }
    else
    {
        const auto db = live() ? s.inputPeakHoldDb : s.inputPeakDb;
        switch (audio::InputMeter::judge (db))
        {
            case audio::InputMeter::Verdict::hot: text = tr ("setup.level.hot", formatDb (db)); c = colours::bad;  break;
            case audio::InputMeter::Verdict::low: text = tr ("setup.level.low", formatDb (db)); c = colours::warn; break;
            case audio::InputMeter::Verdict::ok:  text = tr ("setup.level.ok", formatDb (db));                     break;
        }
    }

    auto verdict = r.removeFromTop (52).toFloat();
    paint::inset (g, verdict);
    auto v = verdict.reduced (14.0f, 0.0f);
    paint::led (g, { v.getX() + 4.0f, v.getCentreY() }, 3.4f, c, true);
    v.removeFromLeft (18.0f);
    g.setColour (c);
    g.setFont (sans (14.0f, Weight::semibold));
    g.drawText (text, v, juce::Justification::centredLeft, true);

    r.removeFromTop (16);
    auto rule = [&] (const juce::String& t, juce::Colour rc)
    {
        auto row = r.removeFromTop (22).toFloat();
        paint::led (g, { row.getX() + 4.0f, row.getCentreY() }, 2.4f, rc, true);
        g.setColour (colours::textDim);
        g.setFont (sans (11.5f));
        g.drawText (t, row.withTrimmedLeft (16.0f), juce::Justification::centredLeft, true);
    };
    rule (tr ("setup.level.rule.hot"), colours::bad);
    rule (tr ("setup.level.rule.low"), colours::warn);
    rule (tr ("setup.level.rule.gain"), colours::textMute);
    rule (tr ("setup.level.rule.loopback"), colours::textMute);
}

void SetupWizard::paintLatency (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& s = state();
    const LatencyLayout l (r);
    g.setColour (colours::text);
    g.setFont (sans (15.0f, Weight::semibold));
    g.drawText (tr ("setup.latency.instruction"), l.instruction, juce::Justification::centredLeft, true);
    g.setColour (colours::textDim);
    g.setFont (sans (12.0f));
    g.drawFittedText (tr ("setup.latency.sub"), l.sub, juce::Justification::topLeft, 2, 1.0f);

    // ボタンの右：測定中・最後の測定の結果（失敗の理由）
    {
        auto st = l.status.toFloat().withTrimmedLeft ((float) juce::jmax (measure.getWidth(), 132) + 16.0f);
        juce::String text;
        juce::Colour c = colours::textDim;
        if (s.latencyMeasuring)
        {
            text = tr ("setup.latency.status.measuring");
            c = colours::ref;   // 測っている間（青）
        }
        else if (s.latencyHasResult)
        {
            const auto& res = s.latencyResult;
            using Status = audio::latency::Result::Status;
            switch (res.status)
            {
                case Status::ok:       text = tr ("setup.latency.status.ok", res.agreeing, res.bursts, juce::String (res.spreadMs, 2)); c = colours::signal; break;
                case Status::silent:   text = tr ("setup.latency.status.silent"); c = colours::warn; break;
                case Status::weak:     text = tr ("setup.latency.status.weak"); c = colours::warn; break;
                case Status::unstable: text = tr ("setup.latency.status.unstable"); c = colours::warn; break;
                case Status::failed:   text = tr ("setup.latency.status.failed"); c = colours::warn; break;
            }
            if (res.clipped)
                text << "  " << tr ("setup.latency.status.clipped");
        }
        if (text.isNotEmpty())
        {
            paint::led (g, { st.getX() + 4.0f, st.getCentreY() }, 3.0f, c, true);
            g.setColour (s.latencyMeasuring ? (juce::Colour) colours::text : c);
            g.setFont (sans (12.0f));
            g.drawFittedText (text, st.withTrimmedLeft (16.0f).toNearestInt(), juce::Justification::centredLeft, 2, 1.0f);
        }
    }

    // 録音位置の補正に使う値（手入力 → 実測 → 申告値）
    const auto ld = latencyDisplay (s);
    auto box = l.box.toFloat();
    paint::inset (g, box);
    auto b = box.reduced (16.0f, 10.0f);
    const auto label = ld.manual ? tr ("setup.latency.manualLabel")
                     : ld.reported ? tr ("setup.latency.reported")
                                   : tr ("setup.latency.result");
    paint::microLabel (g, b.removeFromTop (12.0f), label, colours::textMute);
    g.setColour (colours::text);
    g.setFont (mono (26.0f, Weight::semibold));
    const auto val = ld.known ? juce::String (ld.ms, 1) + " ms" : juce::String ("-");
    g.drawText (val, b.removeFromLeft (textWidth (mono (26.0f, Weight::semibold), val) + 12.0f), juce::Justification::centredLeft, false);
    g.setColour (colours::textDim);
    g.setFont (mono (12.0f));
    juce::String detail;
    if (! ld.known)        detail = tr ("setup.level.noInput");
    else if (ld.manual)    detail = tr ("setup.latency.manualSamples", ld.samples);
    else if (! ld.reported) detail = tr ("setup.latency.samples", ld.samples);
    else if (ld.estimated) detail = tr ("setup.latency.estimatedSamples", ld.samples);
    else                   detail = tr ("setup.latency.reportedSamples", ld.samples, s.input.inputLatency, s.input.outputLatency);
    g.drawText (detail, b, juce::Justification::centredLeft, true);

    // 手入力の行
    {
        auto m = l.manual.toFloat();
        paint::microLabel (g, m.removeFromLeft ((float) latencyLabelW), tr ("setup.latency.manual"), colours::textMute);
        m.removeFromLeft (92.0f);
        g.setColour (colours::textDim);
        g.setFont (mono (12.0f));
        g.drawText ("ms", m.removeFromLeft (34.0f).withTrimmedLeft (8.0f), juce::Justification::centredLeft, false);
    }

    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawFittedText (ld.reported && live() ? tr ("setup.latency.reportedNote") : tr ("setup.latency.note"),
                      l.note, juce::Justification::topLeft, 2, 1.0f);
}
} // namespace vb
