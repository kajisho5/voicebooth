#include "AboutDialog.h"
#include "separation/SeparatorClient.h"

namespace vb
{
namespace
{
    /** 同梱・利用しているもの（名前とライセンスはデータ。翻訳しない。README の「ライセンス」と同じ並び） */
    struct Component { const char* name; const char* useKey; const char* license; };
    const Component components[] = {
        { "JUCE 8",                     "about.use.framework", "AGPLv3" },
        { "Rubber Band Library 4",      "about.use.practice",  "GPL v2+" },
        { "ONNX Runtime 1.22.0",        "about.use.models",    "MIT" },
        { "Monocypher 4.0.3",           "about.use.signature", "CC0 / BSD-2-Clause" },
        { "minimp3",                    "about.use.mp3",       "CC0" },
        { "IBM Plex Sans JP / Mono",    "about.use.fonts",     "SIL OFL 1.1" },
       #if JUCE_WINDOWS
        { "Steinberg ASIO SDK 2.3.4",   "about.use.asio",      "GPLv3" },
       #endif
    };
}

AboutDialog::AboutDialog()
    : DialogPanel (tr ("about.title"), tr ("about.micro"))
{
    addFooterKey (tr ("common.close"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });
    addFooterKey (tr ("about.license"), KeyRole::normal, []
    {
        // 配ったアプリにはライセンス全文が付いている（#15）。開発中のビルドなどで無ければ GitHub の LICENSE
        const auto dir = licensesFolder();
        if (! (dir.isDirectory() && dir.startAsProcess()))
            juce::URL (licenseUrl).launchInDefaultBrowser();
    });
    addFooterKey (tr ("about.source"), KeyRole::normal, [] { juce::URL (sourceUrl).launchInDefaultBrowser(); });
    setSize (860, 640);
}

juce::File AboutDialog::licensesFolder()
{
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
   #if JUCE_MAC
    return exeDir.getSiblingFile ("Resources").getChildFile ("licenses");
   #else
    return exeDir.getChildFile ("licenses");
   #endif
}

void AboutDialog::paintBody (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();

    // 名前とバージョン
    {
        auto top = r.removeFromTop (40.0f);
        const auto nf = sans (22.0f, Weight::semibold);
        g.setFont (nf);
        g.setColour (colours::text);
        const juce::String name ("VoiceBooth");
        g.drawText (name, top.removeFromLeft (textWidth (nf, name) + 14.0f), juce::Justification::centredLeft, false);
        g.setFont (mono (13.0f, Weight::medium));
        g.setColour (colours::signal);
        g.drawText (update::currentVersion(), top, juce::Justification::centredLeft, false);
    }
    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawFittedText (tr ("about.appLicense"), r.removeFromTop (44.0f).toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    r.removeFromTop (8.0f);

    // 表：名前 / 使い道 / ライセンス
    auto row = [&] (const juce::String& name, const juce::String& use, const juce::String& license, juce::Colour licenseColour)
    {
        auto line = r.removeFromTop (28.0f);
        paint::hline (g, std::round (line.getBottom()) - 0.5f, line.getX(), line.getRight(), colours::line.withAlpha (0.5f));
        g.setColour (colours::text);
        g.setFont (sans (12.5f, Weight::medium));
        g.drawText (name, line.removeFromLeft (230.0f), juce::Justification::centredLeft, true);
        g.setColour (licenseColour);
        g.setFont (mono (11.0f, Weight::medium));
        g.drawText (license, line.removeFromRight (190.0f), juce::Justification::centredRight, true);
        g.setColour (colours::textDim);
        g.setFont (sans (12.0f));
        g.drawText (use, line.reduced (8.0f, 0.0f), juce::Justification::centredLeft, true);
    };

    paint::microLabel (g, r.removeFromTop (22.0f), tr ("about.components"), colours::textMute);
    for (auto& c : components)
        row (c.name, tr (c.useKey), c.license, colours::textDim);
    r.removeFromTop (14.0f);

    // モデル（押した時だけダウンロード。入っているかを右に）
    paint::microLabel (g, r.removeFromTop (22.0f), tr ("about.models"), colours::textMute);
    using SC = separation::SeparatorClient;
    struct Model { const char* name; const char* useKey; const char* license; bool installed; };
    const Model models[] = {
        { "BS-RoFormer ft1 (anvuew)",     "about.use.separation", "GPL-3.0", SC::modelInstalled() },
        { "BS-RoFormer karaoke (anvuew)", "about.use.lead",       "GPL-3.0", SC::karaokeInstalled() },
        { "RMVPE (RVC)",                  "about.use.pitch",      "MIT",     SC::pitchModelFile().existsAsFile() },
    };
    for (auto& m : models)
        row (m.name, tr (m.useKey) + "  " + utf8 ("\xc2\xb7") + "  " + tr (m.installed ? "about.installed" : "about.notInstalled"),
             m.license, m.installed ? colours::signal : colours::textDim);

    r.removeFromTop (12.0f);
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawFittedText (tr ("about.modelNote"), r.removeFromTop (48.0f).toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
}
} // namespace vb
