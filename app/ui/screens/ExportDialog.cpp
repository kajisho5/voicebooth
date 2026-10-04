#include "ExportDialog.h"
#include "../WaveLane.h"
#include "export/ExportService.h"
#include "export/DeliveryPack.h"

namespace vb
{
namespace
{
    constexpr int rowH = 46;
}

ExportDialog::ExportDialog (UiSession& u)
    : DialogPanel (tr ("export.title"), tr ("export.micro")), SessionView (u),
      packMode ({ tr ("export.pack.files"), tr ("export.pack.zip") }, u->mode == project::Mode::easy ? 0 : 1),
      bitKeys ({ "16bit", "24bit", "32bit float" }, u->project.bitDepthExport >= 32 ? 2 : (u->project.bitDepthExport <= 16 ? 0 : 1))
{
    const auto& s = state();
    using project::TrackType;

    // 曲を開いていれば録ったテイクから（B5）、無ければ見本のダミー
    const bool real = s.backingWave != nullptr;
    auto add = [&] (TrackType t, bool clip, const char* peak)
    {
        const auto* tr_ = s.project.findTrack (t);
        const bool recorded = tr_ != nullptr && ! tr_->comp.empty();
        // いまのモードで隠れているトラックでも、録ってあれば出す（モードを下げると、録ったダブル・ハモリが知らせなしに納品から抜けていた。監査 2026-10-04）
        const bool visible = session.isTrackVisible (t);
        if (! visible && ! (real && recorded)) return;
        FileRow row { t, exporter::ExportService::dryFileName (s.songName, t), recorded, clip && recorded, recorded ? peak : "", {}, false, {} };
        row.hiddenByMode = ! visible;
        row.packFile = exporter::DeliveryPack::packFileName (t);
        if (real && recorded)
        {
            // 採用区間に使っているテイクの最大値とクリップ（ノーマライズしないので、そのまま書き出される値）
            float pk = 0.0f;
            juce::StringArray clipped;
            for (auto& c : tr_->comp)
                for (auto& k : tr_->takes)
                    if (k.id == c.takeId)
                    {
                        pk = juce::jmax (pk, k.peak);
                        if (k.clip) clipped.addIfNotAlreadyThere (k.id);
                    }
            row.peak = formatDb (juce::Decibels::gainToDecibels (pk, -100.0f));
            row.clip = ! clipped.isEmpty();
            row.clipTakes = clipped.joinIntoString (", ");
        }
        rows.push_back (row);
    };
    add (TrackType::main, true, "-0.1");
    add (TrackType::doubleTrack, false, "-4.8");
    add (TrackType::harm1, false, "");
    add (TrackType::harm2, false, "");
    FileRow refmix { TrackType::backing, s.songName + "_refmix.wav", true, false, real ? "" : "-1.2", {}, true, {} };
    refmix.packFile = "refmix.wav";   // 確認用ミックスは納品パックに入る（B15。いま聞いている音量で）
    rows.push_back (refmix);

    for (auto& r : rows)
    {
        auto* k = checks.add (new KeyButton());
        k->withLed().withToggle (true);
        k->setToggleState (r.available, juce::dontSendNotification);
        k->setEnabled (r.available && ! r.packOnly);
        k->onClick = [this] { repaint(); };
        addAndMakeVisible (k);
    }

    // 簡単は通常 WAV のみ（DESIGN 2）。標準・プロは個別 WAV か納品パック（B15）
    packMode.setVisible (s.mode != project::Mode::easy);
    packMode.onChange = [this] (int)
    {
        // 確認用ミックスはパックの時だけ（入れるかどうかは選べる）
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i].packOnly)
            {
                checks[(int) i]->setEnabled (packSelected());
                checks[(int) i]->setToggleState (packSelected(), juce::dontSendNotification);
            }
        repaint();
    };
    addChildComponent (packMode);
    packMode.onChange (packMode.getSelected());

    // ビット数（既定は録音形式。依頼先が 16bit を指定する時など）。16bit はディザー付き
    bitKeys.setFont (mono (11.0f));
    bitKeys.onChange = [this] (int) { repaint(); };
    addAndMakeVisible (bitKeys);

    addFooterKey (tr ("export.do"), KeyRole::primary, [this] { if (onExport) onExport(); });
    addFooterKey (tr ("common.cancel"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });

    setSize (1040, 520 + rowH * (int) rows.size());   // 行数（モードで変わる）に合わせる
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
    formatArea = r.removeFromTop (28);
    bitKeys.setBounds (formatArea.removeFromRight (juce::jmin (formatArea.getWidth() / 2, bitKeys.idealWidth())).withSizeKeepingCentre (bitKeys.idealWidth(), 26));
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
        paint::microLabel (g, head.removeFromLeft (420.0f), tr ("export.col.file"), colours::textMute);
        paint::microLabel (g, head.removeFromLeft (140.0f), tr ("export.col.length"), colours::textMute);
        paint::microLabel (g, head.removeFromLeft (130.0f), tr ("export.col.peak"), colours::textMute);
        paint::microLabel (g, head, tr ("export.col.status"), colours::textMute);

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& f = rows[i];
            auto row = list.removeFromTop (rowH).toFloat();
            paint::hline (g, row.getBottom() - 1.0f, row.getX(), row.getRight(), colours::grid);
            row.removeFromLeft (44.0f);

            auto name = row.removeFromLeft (420.0f);
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
                        row.removeFromLeft (140.0f), juce::Justification::centredLeft, false);
            g.drawText (f.peak.isEmpty() ? juce::String ("-") : f.peak, row.removeFromLeft (130.0f), juce::Justification::centredLeft, false);

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

            if (f.packOnly && ! packSelected()) chip (tr ("export.status.packOnly"), colours::textMute, Icon::warning);
            else if (! f.available)  chip (tr ("export.status.unrecorded"), colours::textMute, Icon::warning);
            else if (f.clip)    chip (f.clipTakes.isNotEmpty() ? tr ("export.status.clipTakes", f.clipTakes) : tr ("export.status.clip"),
                                      colours::bad, Icon::warning);
            else if (f.hiddenByMode) chip (tr ("export.status.hiddenMode"), colours::warn, Icon::warning);
            else                chip (tr ("export.status.ok"), colours::signal, Icon::check);
        }
    }

    // 形式（DESIGN 6.5）
    {
        auto f = formatArea.toFloat();
        paint::microLabel (g, f.removeFromLeft (80.0f), tr ("export.format"), colours::textMute);
        g.setColour (colours::text);
        g.setFont (sans (12.0f));
        const auto bits = selectedBitDepth();
        auto text = tr ("export.format.value", formatKhz (s.sampleRate()), formatBits (bits));
        if (bits == 16)
            text << "  " << tr ("export.format.dither");
        g.drawText (text, f, juce::Justification::centredLeft, true);
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

        const bool zip = packSelected();
        const bool real = s.backingWave != nullptr && s.projectFolder != juce::File();
        const auto packName = real ? exporter::DeliveryPack::nextFolder (s.projectFolder, juce::Time::getCurrentTime()).getFileName()
                                   : juce::String ("export_20261001");
        juce::StringArray lines;
        if (zip)
        {
            lines.add (packName + "/   (+ " + packName + ".zip)");
            for (size_t i = 0; i < rows.size(); ++i)
                if (rows[i].available && checks[(int) i]->getToggleState()) lines.add ("  " + rows[i].packFile);
            lines.add ("  notes.txt");
            if (s.mode == project::Mode::pro) lines.add ("  take_map.txt");
        }
        else
        {
            for (size_t i = 0; i < rows.size(); ++i)
                if (rows[i].available && ! rows[i].packOnly && checks[(int) i]->getToggleState()) lines.add (rows[i].file);
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
            const auto bits = selectedBitDepth();
            juce::String peak = "-";
            for (auto& f : rows) if (f.type == project::TrackType::main && f.available) peak = f.peak;
            const juce::StringArray notes {
                "title: " + s.songName,
                "sr: " + juce::String (s.sampleRate()),
                "bit: " + (bits >= 32 ? juce::String ("32 float") : juce::String (bits)),
                "key: 0",          // 納品は原キー・原速（練習のテンポ・キーは入らない）
                "tempo: 100",
                "peak_vocal_dbfs: " + peak,
                "normalized: no",
                "latency_compensation_ms: " + juce::String (latencyDisplay (s).ms, 1),   // いま補正に使っている値（B6）
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
        const auto dest = s.backingWave != nullptr && s.projectFolder != juce::File()
                        ? exporter::DeliveryPack::nextFolder (s.projectFolder, juce::Time::getCurrentTime()).getFullPathName()
                                + juce::File::getSeparatorString()
                        : "Projects/" + s.songName + "/export_20261001/";
        g.drawText (dest, d, juce::Justification::centredLeft, true);
    }
}

int ExportDialog::selectedBitDepth() const
{
    switch (bitKeys.getSelected())
    {
        case 0:  return 16;
        case 2:  return 32;
        default: return 24;
    }
}

bool ExportDialog::packSelected() const
{
    return packMode.isVisible() && packMode.getSelected() == 1;
}

bool ExportDialog::refmixSelected() const
{
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].packOnly)
            return packSelected() && checks[(int) i]->getToggleState();
    return false;
}

std::vector<project::TrackType> ExportDialog::selectedTracks() const
{
    std::vector<project::TrackType> out;
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].available && ! rows[i].packOnly && rows[i].type != project::TrackType::backing
            && checks[(int) i]->getToggleState())
            out.push_back (rows[i].type);
    return out;
}
} // namespace vb
