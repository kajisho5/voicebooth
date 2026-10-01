#include "ExportDialog.h"
#include "../WaveLane.h"
#include "export/ExportService.h"

namespace vb
{
namespace
{
    constexpr int rowH = 46;
}

ExportDialog::ExportDialog (UiSession& u)
    : DialogPanel (tr ("export.title"), tr ("export.micro")), SessionView (u),
      packMode ({ tr ("export.pack.files"), tr ("export.pack.zip") }, u->mode == project::Mode::easy ? 0 : 1)
{
    const auto& s = state();
    using project::TrackType;

    auto add = [&] (TrackType t, bool clip, const char* peak)
    {
        if (! session.isTrackVisible (t)) return;
        const auto* tr_ = s.project.findTrack (t);
        const bool recorded = tr_ != nullptr && ! tr_->comp.empty();
        rows.push_back ({ t, exporter::ExportService::dryFileName (s.songName, t),
                          recorded, clip && recorded, recorded ? peak : "" });   // ピーク値はダミー（B15）
    };
    add (TrackType::main, true, "-0.1");
    add (TrackType::doubleTrack, false, "-4.8");
    add (TrackType::harm1, false, "");
    add (TrackType::harm2, false, "");
    rows.push_back ({ TrackType::backing, s.songName + "_refmix.wav", true, false, "-1.2" });

    for (auto& r : rows)
    {
        auto* k = checks.add (new KeyButton());
        k->withLed().withToggle (true);
        k->setToggleState (r.available, juce::dontSendNotification);
        k->setEnabled (r.available);
        addAndMakeVisible (k);
    }

    // 簡単は通常 WAV のみ（DESIGN 2）
    packMode.setVisible (s.mode != project::Mode::easy);
    packMode.onChange = [this] (int) { repaint(); };
    addChildComponent (packMode);

    addFooterKey (tr ("export.do"), KeyRole::primary, [this] { if (onExport) onExport(); });
    addFooterKey (tr ("common.cancel"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });

    setSize (860, 520 + rowH * (int) rows.size());   // 行数（モードで変わる）に合わせる
}

void ExportDialog::layoutBody (juce::Rectangle<int> r)
{
    listArea = r.removeFromTop (22 + rowH * (int) rows.size());
    auto list = listArea.withTrimmedTop (22);
    for (auto* k : checks)
    {
        auto row = list.removeFromTop (rowH);
        k->setBounds (row.removeFromLeft (34).withSizeKeepingCentre (30, 28));
    }

    r.removeFromTop (14);
    formatArea = r.removeFromTop (24);
    r.removeFromTop (12);

    packArea = r.removeFromTop (34);
    if (packMode.isVisible())
        packMode.setBounds (packArea.removeFromRight (juce::jmax (260, packMode.idealWidth())));

    r.removeFromTop (12);
    destArea = r.removeFromBottom (26);
    r.removeFromBottom (10);
    previewArea = r;
}

void ExportDialog::paintBody (juce::Graphics& g, juce::Rectangle<int> r)
{
    const auto& s = state();
    juce::ignoreUnused (r);

    // ファイル一覧（メンバーを削らないようコピーで扱う）
    {
        auto list = listArea;
        auto head = list.removeFromTop (22).toFloat().withTrimmedLeft (44.0f);
        paint::microLabel (g, head.removeFromLeft (330.0f), tr ("export.col.file"), colours::textMute);
        paint::microLabel (g, head.removeFromLeft (110.0f), tr ("export.col.length"), colours::textMute);
        paint::microLabel (g, head.removeFromLeft (80.0f), tr ("export.col.peak"), colours::textMute);
        paint::microLabel (g, head, tr ("export.col.status"), colours::textMute);

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& f = rows[i];
            auto row = list.removeFromTop (rowH).toFloat();
            paint::hline (g, row.getBottom() - 1.0f, row.getX(), row.getRight(), colours::grid);
            row.removeFromLeft (44.0f);

            auto name = row.removeFromLeft (330.0f);
            g.setColour (f.available ? colours::text : colours::textMute);
            g.setFont (mono (12.0f, Weight::medium));
            g.drawText (f.file, name.removeFromTop (name.getHeight() * 0.55f), juce::Justification::bottomLeft, true);
            g.setColour (colours::textMute);
            g.setFont (sans (10.5f));
            g.drawText (f.type == project::TrackType::backing ? tr ("export.refmix") : trackName (f.type),
                        name, juce::Justification::topLeft, true);

            g.setColour (colours::textDim);
            g.setFont (mono (11.5f));
            g.drawText (f.available ? formatTime (s.project.lengthSamples, s.sampleRate(), true) : juce::String ("-"),
                        row.removeFromLeft (110.0f), juce::Justification::centredLeft, false);
            g.drawText (f.peak.isEmpty() ? juce::String ("-") : f.peak, row.removeFromLeft (80.0f), juce::Justification::centredLeft, false);

            auto chip = [&] (const juce::String& text, juce::Colour c, Icon icon)
            {
                const auto cf = sans (11.0f, Weight::medium);
                const auto w = textWidth (cf, text) + 32.0f;
                auto b = row.removeFromLeft (w).withSizeKeepingCentre (w, 22.0f);
                row.removeFromLeft (6.0f);
                g.setColour (c.withAlpha (0.14f));
                g.fillRoundedRectangle (b, 3.0f);
                drawIcon (g, icon, b.removeFromLeft (24.0f).withSizeKeepingCentre (12.0f, 12.0f), c);
                g.setColour (c);
                g.setFont (cf);
                g.drawText (text, b, juce::Justification::centredLeft, false);
            };

            if (! f.available)  chip (tr ("export.status.unrecorded"), colours::textMute, Icon::warning);
            else if (f.clip)    chip (tr ("export.status.clip"), colours::bad, Icon::warning);
            else                chip (tr ("export.status.ok"), colours::signal, Icon::check);
        }
    }

    // 形式（DESIGN 6.5）
    {
        auto f = formatArea.toFloat();
        paint::microLabel (g, f.removeFromLeft (80.0f), tr ("export.format"), colours::textMute);
        g.setColour (colours::text);
        g.setFont (sans (12.0f));
        g.drawText (tr ("export.format.value", formatKhz (s.sampleRate()), s.project.bitDepthExport), f, juce::Justification::centredLeft, true);
    }

    // 書き出し方
    {
        auto p = packArea.toFloat();
        g.setColour (colours::text);
        g.setFont (sans (13.0f, Weight::semibold));
        g.drawText (s.mode == project::Mode::easy ? tr ("export.pack.easy") : tr ("export.pack.title"), p, juce::Justification::centredLeft, true);
    }

    // パックの中身（プレビュー）
    {
        auto p = previewArea.toFloat();
        paint::inset (g, p);
        p.reduce (14.0f, 10.0f);

        const bool zip = packMode.isVisible() && packMode.getSelected() == 1;
        juce::StringArray lines;
        if (zip)
        {
            lines.add ("export_20261001/");
            for (auto& f : rows) if (f.available) lines.add ("  " + f.file);
            lines.add ("  notes.txt");
            if (s.mode == project::Mode::pro) lines.add ("  take_map.txt");
        }
        else
        {
            for (auto& f : rows) if (f.available) lines.add (f.file);
        }

        auto col = p.removeFromLeft (p.getWidth() * 0.45f);
        paint::microLabel (g, col.removeFromTop (14.0f), tr ("export.preview.files"), colours::textMute);
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        for (auto& l : lines)
            g.drawText (l, col.removeFromTop (17.0f), juce::Justification::centredLeft, true);

        if (zip)
        {
            p.removeFromLeft (16.0f);
            paint::microLabel (g, p.removeFromTop (14.0f), "notes.txt", colours::textMute);
            const juce::StringArray notes {
                "title: " + s.songName,
                "sr: " + juce::String (s.sampleRate()),
                "bit: 24",
                "key: " + juce::String (s.keyShift),
                "tempo: 100",
                "peak_vocal_dbfs: -0.1",
                "normalized: no",
                "latency_compensation_ms: " + juce::String ((double) s.latencySamples * 1000.0 / s.sampleRate(), 1),
            };
            g.setColour (colours::textDim);
            g.setFont (mono (11.0f));
            for (auto& l : notes)
                g.drawText (l, p.removeFromTop (17.0f), juce::Justification::centredLeft, true);
        }
    }

    // 保存先
    {
        auto d = destArea.toFloat();
        drawIcon (g, Icon::folder, d.removeFromLeft (18.0f).withSizeKeepingCentre (14.0f, 14.0f), colours::textDim);
        d.removeFromLeft (8.0f);
        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        g.drawText ("Projects/" + s.songName + "/export_20261001/", d, juce::Justification::centredLeft, true);
    }
}
} // namespace vb
