#include "SetupWizard.h"

namespace vb
{
namespace
{
    constexpr int stepperH = 44;

    struct DeviceRow { const char* name; const char* detail; bool selected; bool warnLatency; };

    // 機器名はデータ（翻訳しない）
    const DeviceRow inputs[] = {
        { "USB Audio Interface \xe2\x80\x94 In 1", "2ch  48kHz", true,  false },
        { "Built-in Microphone",                    "1ch  48kHz", false, false },
        { "Bluetooth Headset",                      "1ch  16kHz", false, true  },
    };
    const DeviceRow outputs[] = {
        { "USB Audio Interface \xe2\x80\x94 Out 1/2", "2ch  48kHz", true,  false },
        { "Built-in Output",                          "2ch  48kHz", false, false },
    };
}

SetupWizard::SetupWizard (UiSession& u, int initialStep)
    : DialogPanel (tr ("setup.title"), tr ("setup.micro")), SessionView (u),
      measure (tr ("setup.latency.measure"))
{
    next = &addFooterKey (tr ("setup.next"), KeyRole::primary, [this]
    {
        if (step < 2) setStep (step + 1);
        else if (onCloseRequest) onCloseRequest();
    });
    back = &addFooterKey (tr ("setup.back"), KeyRole::normal, [this] { setStep (juce::jmax (0, step - 1)); });

    meter.setLevels (u->inputPeakDb, u->inputRmsDb, u->inputPeakHoldDb, false);
    addChildComponent (meter);

    measure.withIcon (Icon::metronome);
    addChildComponent (measure);

    setSize (780, 560);
    setStep (initialStep);
}

void SetupWizard::setStep (int s)
{
    step = juce::jlimit (0, 2, s);
    back->setEnabled (step > 0);
    next->setButtonText (step == 2 ? tr ("setup.finish") : tr ("setup.next"));
    meter.setVisible (step == 1);
    measure.setVisible (step == 2);
    resized();
    repaint();
}

void SetupWizard::layoutBody (juce::Rectangle<int> r)
{
    r.removeFromTop (stepperH + 18);

    if (step == 1)
    {
        r.removeFromTop (70);
        meterArea = r.removeFromTop (44);
        meter.setBounds (meterArea);
    }
    if (step == 2)
    {
        r.removeFromTop (70);
        measure.setSize (10, 34);
        measure.setBounds (r.removeFromTop (34).removeFromLeft (measure.idealWidth()));
    }
}

void SetupWizard::paintBody (juce::Graphics& g, juce::Rectangle<int> r)
{
    paintStepper (g, r.removeFromTop (stepperH));
    r.removeFromTop (18);

    switch (step)
    {
        case 0:  paintDevice (g, r);  break;
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
        if (done) drawIcon (g, Icon::check, num.reduced (4.0f), colours::bgDeep);
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
    auto list = [&] (const juce::String& heading, const DeviceRow* rows, size_t n, juce::Rectangle<int> area)
    {
        paint::microLabel (g, area.removeFromTop (18).toFloat(), heading, colours::textMute);
        area.removeFromTop (4);
        for (size_t i = 0; i < n; ++i)
        {
            auto row = area.removeFromTop (40).toFloat();
            area.removeFromTop (4);
            if (rows[i].selected) paint::keycap (g, row, { false, false, true, true });
            else                  paint::inset (g, row);

            auto c = row.reduced (12.0f, 0.0f);
            paint::led (g, { c.getX() + 4.0f, c.getCentreY() }, 3.0f, colours::signal, rows[i].selected);
            c.removeFromLeft (16.0f);

            if (rows[i].warnLatency)
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

            g.setColour (colours::textMute);
            g.setFont (mono (10.5f));
            g.drawText (rows[i].detail, c.removeFromRight (90.0f), juce::Justification::centredRight, false);
            g.setColour (rows[i].selected ? colours::text : colours::textDim);
            g.setFont (sans (12.5f, rows[i].selected ? Weight::medium : Weight::regular));
            g.drawText (utf8 (rows[i].name), c, juce::Justification::centredLeft, true);
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

void SetupWizard::paintLevel (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (colours::text);
    g.setFont (sans (15.0f, Weight::semibold));
    g.drawText (tr ("setup.level.instruction"), r.removeFromTop (26), juce::Justification::centredLeft, true);
    g.setColour (colours::textDim);
    g.setFont (sans (12.0f));
    g.drawText (tr ("setup.level.sub"), r.removeFromTop (22), juce::Justification::centredLeft, true);

    r.removeFromTop (22 + 44 + 20);

    // 判定（-3 超: 下げて / -20 未満: 上げて / それ以外: OK）
    auto verdict = r.removeFromTop (52).toFloat();
    paint::inset (g, verdict);
    auto v = verdict.reduced (14.0f, 0.0f);
    paint::led (g, { v.getX() + 4.0f, v.getCentreY() }, 3.4f, colours::signal, true);
    v.removeFromLeft (18.0f);
    g.setColour (colours::signal);
    g.setFont (sans (14.0f, Weight::semibold));
    g.drawText (tr ("setup.level.ok", juce::String (state().inputPeakDb, 1)), v, juce::Justification::centredLeft, true);

    r.removeFromTop (16);
    auto rule = [&] (const juce::String& text, juce::Colour c)
    {
        auto row = r.removeFromTop (22).toFloat();
        paint::led (g, { row.getX() + 4.0f, row.getCentreY() }, 2.4f, c, true);
        g.setColour (colours::textDim);
        g.setFont (sans (11.5f));
        g.drawText (text, row.withTrimmedLeft (16.0f), juce::Justification::centredLeft, true);
    };
    rule (tr ("setup.level.rule.hot"), colours::bad);
    rule (tr ("setup.level.rule.low"), colours::warn);
    rule (tr ("setup.level.rule.gain"), colours::textMute);
    rule (tr ("setup.level.rule.loopback"), colours::textMute);
}

void SetupWizard::paintLatency (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& s = state();
    g.setColour (colours::text);
    g.setFont (sans (15.0f, Weight::semibold));
    g.drawText (tr ("setup.latency.instruction"), r.removeFromTop (26), juce::Justification::centredLeft, true);
    g.setColour (colours::textDim);
    g.setFont (sans (12.0f));
    g.drawText (tr ("setup.latency.sub"), r.removeFromTop (22), juce::Justification::centredLeft, true);

    r.removeFromTop (22 + 34 + 20);

    auto box = r.removeFromTop (78).toFloat();
    paint::inset (g, box);
    auto b = box.reduced (16.0f, 10.0f);
    paint::microLabel (g, b.removeFromTop (12.0f), tr ("setup.latency.result"), colours::textMute);
    const auto ms = (double) s.latencySamples * 1000.0 / s.sampleRate();
    g.setColour (colours::text);
    g.setFont (mono (26.0f, Weight::semibold));
    const auto val = juce::String (ms, 1) + " ms";
    g.drawText (val, b.removeFromLeft (textWidth (mono (26.0f, Weight::semibold), val) + 12.0f), juce::Justification::centredLeft, false);
    g.setColour (colours::textDim);
    g.setFont (mono (12.0f));
    g.drawText (tr ("setup.latency.samples", s.latencySamples), b, juce::Justification::centredLeft, false);

    r.removeFromTop (16);
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawFittedText (tr ("setup.latency.note"), r.removeFromTop (40), juce::Justification::topLeft, 2, 1.0f);
}
} // namespace vb
