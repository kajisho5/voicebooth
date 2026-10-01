#include "ControlPanel.h"

namespace vb
{
ControlPanel::ControlPanel (const dummy::Session& s)
    : session (s),
      tempo (jp ("テンポ"), 50.0, 150.0, s.tempoPercent, 1.0, [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; }),
      key (jp ("キー"), -6.0, 6.0, s.keyShift, 1.0,
           [] (double v) { const auto k = juce::roundToInt (v); return k == 0 ? juce::String ("0") : (k > 0 ? "+" : "") + juce::String (k); },
           true, colours::refPitch),
      reverb (jp ("モニターリバーブ"), 0.0, 1.0, s.monitorReverb, 0.01,
              [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }),
      offVocal (jp ("オフボーカル"), s.offVocalGain),
      mainVol (jp ("お手本 Main"), s.mainGain, true, colours::refPitch),
      harmVol (jp ("お手本 Harm"), s.harmonyGain, true, colours::refPitch),
      monitorVol (jp ("自分のモニター"), s.monitorGain),
      recMode ({ jp ("納品"), jp ("練習") }, (int) s.recMode),
      meter (false)
{
    tempo.setCaption (jp ("50–150%\n原速 120 BPM"));
    key.setCaption (jp ("-6 〜 +6\n練習用"));
    reverb.setCaption (jp ("耳だけ\n録音には入りません"));

    meter.setLevels (s.inputPeakDb, s.inputRmsDb, s.inputPeakHoldDb, false);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &tempo, &key, &reverb, &offVocal, &mainVol, &harmVol, &monitorVol, &recMode, &meter })
        addAndMakeVisible (c);
}

void ControlPanel::resized()
{
    auto r = getLocalBounds().reduced (metrics::pad, 10);

    // 狭いウィンドウでは固定幅カードを縮め、音量カードの最低幅を確保する
    constexpr int gap = 10, minMixWidth = 330;
    constexpr int tempoW = 170, keyW = 160, reverbW = 200, recW = 236, inputW = 270;
    constexpr int fixedSum = tempoW + keyW + reverbW + recW + inputW;
    const auto available = r.getWidth() - gap * 5;
    const auto scale = juce::jlimit (0.6f, 1.0f, (float) (available - minMixWidth) / (float) fixedSum);
    auto w = [scale] (int base) { return juce::roundToInt ((float) base * scale); };

    tempoCard  = r.removeFromLeft (w (tempoW));    r.removeFromLeft (gap);
    keyCard    = r.removeFromLeft (w (keyW));      r.removeFromLeft (gap);
    inputCard  = r.removeFromRight (w (inputW));   r.removeFromRight (gap);
    recCard    = r.removeFromRight (w (recW));     r.removeFromRight (gap);
    reverbCard = r.removeFromRight (w (reverbW));  r.removeFromRight (gap);
    mixCard    = r;

    tempo.setBounds (tempoCard.reduced (12, 8));
    key.setBounds (keyCard.reduced (12, 8));
    reverb.setBounds (reverbCard.reduced (12, 8));

    {
        auto m = mixCard.reduced (12, 8);
        m.removeFromTop (20);
        const auto rowH = m.getHeight() / 4;
        for (auto* f : { &offVocal, &mainVol, &harmVol, &monitorVol })
            f->setBounds (m.removeFromTop (rowH));
    }

    {
        auto c = recCard.reduced (12, 8);
        c.removeFromTop (22);
        recMode.setBounds (c.removeFromTop (28));
        c.removeFromTop (6);
        recStatusArea = c;
    }

    {
        auto c = inputCard.reduced (12, 8);
        c.removeFromTop (24);
        meter.setBounds (c.removeFromTop (36));
        c.removeFromTop (4);
        inputReadoutArea = c;
    }
}

void ControlPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
    g.setColour (colours::border);
    g.fillRect (getLocalBounds().withHeight (1));

    for (auto c : { tempoCard, keyCard, mixCard, reverbCard, recCard, inputCard })
        drawCard (g, c.toFloat());

    drawCardTitle (g, mixCard.reduced (12, 8).withHeight (20), jp ("音量"));
    drawCardTitle (g, recCard.reduced (12, 8).withHeight (20), jp ("録音"));
    drawCardTitle (g, inputCard.reduced (12, 8).withHeight (20), jp ("入力レベル"));

    // 録音カード：現在の録音先と、納品 REC 中のロック
    {
        auto r = recStatusArea;
        const bool rec = session.isRecording;
        const bool delivery = session.recMode == project::RecMode::delivery;

        auto line = r.removeFromTop (20);
        const auto dot = line.removeFromLeft (14).toFloat().withSizeKeepingCentre (8.0f, 8.0f);
        g.setColour (rec ? colours::rec : colours::textDim);
        g.fillEllipse (dot);
        g.setFont (font (12.0f, FontWeight::bold));
        g.setColour (rec ? colours::rec : colours::text);
        g.drawText ((rec ? jp ("録音中: ") : jp ("待機: ")) + (delivery ? jp ("Dry Vocal / 納品") : jp ("練習テイク")),
                    line, juce::Justification::centredLeft, true);

        auto lock = r.removeFromTop (18);
        g.setColour (colours::textDim);
        g.fillPath (makeIcon (Icon::lock, lock.removeFromLeft (14).toFloat().withSizeKeepingCentre (11.0f, 11.0f)));
        lock.removeFromLeft (4);
        g.setFont (font (10.5f));
        g.drawText (jp ("納品REC中はテンポ100%・キー0に固定"), lock, juce::Justification::centredLeft, true);
    }

    // 入力カード：数値
    {
        auto r = inputReadoutArea;
        g.setFont (font (11.0f));

        auto put = [&] (const juce::String& label, const juce::String& value, juce::Colour vc, int w)
        {
            auto cell = r.removeFromLeft (w);
            g.setColour (colours::textDim);
            g.drawText (label, cell.removeFromLeft ((int) textWidth (font (11.0f), label) + 4), juce::Justification::centredLeft, false);
            g.setColour (vc);
            g.setFont (font (12.0f, FontWeight::bold));
            g.drawText (value, cell, juce::Justification::centredLeft, false);
            g.setFont (font (11.0f));
        };

        put (jp ("ピーク"), juce::String (session.inputPeakDb, 1), colours::text, 82);
        put ("RMS", juce::String (session.inputRmsDb, 1), colours::text, 74);

        const bool inTarget = session.inputPeakDb >= -12.0f && session.inputPeakDb <= -6.0f;
        const auto tag = r.removeFromRight (40).toFloat().withSizeKeepingCentre (36.0f, 18.0f);
        g.setColour ((inTarget ? colours::ok : colours::warn).withAlpha (0.16f));
        g.fillRoundedRectangle (tag, 4.0f);
        g.setColour (inTarget ? colours::ok : colours::warn);
        g.setFont (font (11.0f, FontWeight::bold));
        g.drawText (inTarget ? "OK" : jp ("調整"), tag, juce::Justification::centred, false);

        g.setColour (colours::textDim);
        g.setFont (font (10.5f));
        g.drawText ("dBFS", r, juce::Justification::centredLeft, false);
    }
}
} // namespace vb
