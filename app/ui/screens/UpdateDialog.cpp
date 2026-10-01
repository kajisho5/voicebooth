#include "UpdateDialog.h"

/*  見た目だけのモック。版番号・サイズ・変更点・速度などはダミー（通信しない） */

namespace vb
{
namespace
{
    /** アイコン（または LED）＋ 1 行の説明 */
    void infoLine (juce::Graphics& g, juce::Rectangle<float>& r, Icon icon, juce::Colour c, const juce::String& text,
                   juce::Colour textColour = colours::textDim)
    {
        auto row = r.removeFromTop (26.0f);
        drawIcon (g, icon, row.removeFromLeft (22.0f).withSizeKeepingCentre (15.0f, 15.0f), c);
        row.removeFromLeft (8.0f);
        g.setColour (textColour);
        g.setFont (sans (12.5f));
        g.drawText (text, row, juce::Justification::centredLeft, true);
    }

    /** 罫線つきの小見出し */
    void heading (juce::Graphics& g, juce::Rectangle<float>& r, const juce::String& text)
    {
        auto h = r.removeFromTop (24.0f);
        paint::microLabel (g, h, text, colours::textMute);
        r.removeFromTop (4.0f);
    }
}

//==============================================================================
UpdateDialog::UpdateDialog() : DialogPanel (tr ("update.title"), tr ("update.micro"))
{
    addFooterKey (tr ("update.install"), KeyRole::primary, [this] { if (onInstall) onInstall(); });
    addFooterKey (tr ("update.later"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });
    addFooterKey (tr ("update.skip"), KeyRole::normal, [this] { if (onSkip) onSkip(); });
    setSize (640, 480);
}

void UpdateDialog::paintBody (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();

    // いまの版 → 新しい版
    {
        auto v = r.removeFromTop (64.0f);
        paint::inset (g, v, 6.0f);
        v.reduce (18.0f, 0.0f);

        const auto cur = juce::String (JUCE_APPLICATION_VERSION_STRING);
        const auto vf = mono (24.0f, Weight::semibold);
        g.setFont (vf);
        g.setColour (colours::textMute);
        g.drawText (cur, v.removeFromLeft (textWidth (vf, cur) + 4.0f), juce::Justification::centredLeft, false);
        drawIcon (g, Icon::chevronRight, v.removeFromLeft (34.0f).withSizeKeepingCentre (16.0f, 16.0f), colours::textMute);
        g.setColour (colours::signal);
        g.drawText ("0.2.0", v.removeFromLeft (textWidth (vf, "0.2.0") + 4.0f), juce::Justification::centredLeft, false);

        g.setColour (colours::textDim);
        g.setFont (mono (11.0f));
        g.drawText ("2026-10-15   8.1 MB", v, juce::Justification::centredRight, false);
    }
    r.removeFromTop (18.0f);

    // 主な変更（appcast のリリースノートから。ここはダミー）
    heading (g, r, tr ("update.notes"));
    for (auto key : { "update.sample.note1", "update.sample.note2", "update.sample.note3" })
    {
        auto row = r.removeFromTop (24.0f);
        paint::led (g, { row.getX() + 5.0f, row.getCentreY() }, 2.2f, colours::signal, true);
        g.setColour (colours::text);
        g.setFont (sans (12.5f));
        g.drawText (tr (key), row.withTrimmedLeft (18.0f), juce::Justification::centredLeft, true);
    }
    r.removeFromTop (16.0f);

    // 安心材料（DESIGN 11.7 の必須事項をユーザーの言葉で）
    heading (g, r, tr ("update.safety"));
    infoLine (g, r, Icon::shield, colours::signal, tr ("update.signed"), colours::text);
    infoLine (g, r, Icon::rec, colours::textMute, tr ("update.notDuringRec"));
    infoLine (g, r, Icon::warning, colours::warn, tr ("update.restart"));
}

//==============================================================================
namespace
{
    constexpr double modelMB = 220.0;   // ダミー

    juce::String modelsFolderText()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("VoiceBooth").getChildFile ("Models");
        auto path = dir.getFullPathName();
        const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
        if (path.startsWith (home))
            path = "~" + path.substring (home.length());
        return path;
    }

    juce::String freeSpaceText()
    {
        const auto bytes = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getBytesFreeOnVolume();
        return juce::String (juce::roundToInt ((double) bytes / (1024.0 * 1024.0 * 1024.0))) + " GB";
    }
}

ModelDownloadDialog::ModelDownloadDialog (Stage s, bool animate)
    : DialogPanel (tr ("model.title"), tr ("model.micro")), stage (s)
{
    switch (stage)
    {
        case Stage::confirm:
            addFooterKey (tr ("model.download", juce::roundToInt (modelMB)), KeyRole::primary,
                          [this] { if (onStage) onStage (Stage::downloading); });
            addFooterKey (tr ("model.later"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });
            break;

        case Stage::downloading:
            addFooterKey (tr ("model.pause"), KeyRole::normal, [] {});
            addFooterKey (tr ("common.cancel"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });
            if (animate)
            {
                progress = 0.0f;
                startTimerHz (30);
            }
            break;

        case Stage::done:
            addFooterKey (tr ("model.continue"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });
            break;

        case Stage::failed:
            addFooterKey (tr ("model.retry"), KeyRole::primary, [this] { if (onStage) onStage (Stage::downloading); });
            addFooterKey (tr ("model.later"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });
            break;
    }
    setSize (640, 440);
}

ModelDownloadDialog::~ModelDownloadDialog()
{
    stopTimer();
}

void ModelDownloadDialog::timerCallback()
{
    // 見た目だけ：約 4 秒で終わり、完了へ
    progress = juce::jmin (1.0f, progress + 1.0f / 120.0f);
    repaint();
    if (progress >= 1.0f)
    {
        stopTimer();
        if (onStage) onStage (Stage::done);
    }
}

void ModelDownloadDialog::paintBody (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();

    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawFittedText (tr ("model.sub"), r.removeFromTop (40.0f).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
    r.removeFromTop (8.0f);

    // モデル（名前・用途・サイズ・ライセンス・配布元）
    {
        auto card = r.removeFromTop (70.0f);
        paint::inset (g, card, 6.0f);
        card.reduce (16.0f, 10.0f);

        auto top = card.removeFromTop (card.getHeight() * 0.5f);
        g.setColour (colours::textDim);
        g.setFont (mono (12.0f, Weight::medium));
        g.drawText (juce::String (juce::roundToInt (modelMB)) + " MB", top.removeFromRight (90.0f), juce::Justification::centredRight, false);
        g.setColour (colours::text);
        g.setFont (mono (14.0f, Weight::semibold));
        const auto name = juce::String ("Mel-Band RoFormer");
        g.drawText (name, top.removeFromLeft (textWidth (mono (14.0f, Weight::semibold), name) + 14.0f), juce::Justification::centredLeft, false);
        g.setColour (colours::textDim);
        g.setFont (sans (12.0f));
        g.drawText (tr ("model.role"), top, juce::Justification::centredLeft, true);

        g.setColour (colours::textMute);
        g.setFont (sans (11.0f));
        g.drawText (tr ("model.meta", "MIT"), card, juce::Justification::centredLeft, true);
    }
    r.removeFromTop (18.0f);

    switch (stage)
    {
        case Stage::confirm:
            heading (g, r, tr ("model.before"));
            infoLine (g, r, Icon::folder, colours::textMute, tr ("model.folder", modelsFolderText()));
            infoLine (g, r, Icon::check, colours::signal, tr ("model.space", freeSpaceText()));
            infoLine (g, r, Icon::rec, colours::textMute, tr ("model.pauseOnPlay"));
            infoLine (g, r, Icon::shield, colours::signal, tr ("model.verify"));
            break;

        case Stage::downloading:
        {
            heading (g, r, tr ("model.downloading"));
            auto bar = r.removeFromTop (10.0f);
            paint::inset (g, bar, 4.0f);
            g.setColour (colours::signal);
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (8.0f, bar.getWidth() * progress)), 4.0f);
            r.removeFromTop (8.0f);

            const auto done = modelMB * progress;
            const auto left = juce::jmax (0, juce::roundToInt ((modelMB - done) / 8.4));
            auto line = r.removeFromTop (22.0f);
            g.setColour (colours::text);
            g.setFont (mono (12.0f, Weight::medium));
            g.drawText (juce::String (juce::roundToInt (done)) + " / " + juce::String (juce::roundToInt (modelMB)) + " MB   8.4 MB/s",
                        line, juce::Justification::centredLeft, false);
            g.setColour (colours::textDim);
            g.setFont (sans (12.0f));
            g.drawText (tr ("model.remaining", left), line, juce::Justification::centredRight, false);
            r.removeFromTop (16.0f);

            infoLine (g, r, Icon::rec, colours::textMute, tr ("model.pauseOnPlay"));
            infoLine (g, r, Icon::shield, colours::signal, tr ("model.verifyAfter"));
            break;
        }

        case Stage::done:
        {
            auto big = r.removeFromTop (44.0f);
            drawIcon (g, Icon::check, big.removeFromLeft (36.0f).withSizeKeepingCentre (26.0f, 26.0f), colours::signal);
            big.removeFromLeft (8.0f);
            g.setColour (colours::text);
            g.setFont (sans (16.0f, Weight::semibold));
            g.drawText (tr ("model.ready"), big, juce::Justification::centredLeft, true);
            r.removeFromTop (8.0f);
            infoLine (g, r, Icon::shield, colours::signal, tr ("model.verified"), colours::text);
            infoLine (g, r, Icon::folder, colours::textMute, tr ("model.folder", modelsFolderText()));
            infoLine (g, r, Icon::check, colours::textMute, tr ("model.noMore"));
            break;
        }

        case Stage::failed:
        {
            auto big = r.removeFromTop (44.0f);
            drawIcon (g, Icon::warning, big.removeFromLeft (36.0f).withSizeKeepingCentre (26.0f, 26.0f), colours::bad);
            big.removeFromLeft (8.0f);
            g.setColour (colours::bad);
            g.setFont (sans (16.0f, Weight::semibold));
            g.drawText (tr ("model.failed"), big, juce::Justification::centredLeft, true);
            r.removeFromTop (8.0f);
            g.setColour (colours::textDim);
            g.setFont (sans (12.5f));
            g.drawFittedText (tr ("model.failed.sub"), r.removeFromTop (40.0f).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
            r.removeFromTop (6.0f);
            infoLine (g, r, Icon::check, colours::textMute, tr ("model.failed.safe"));
            break;
        }
    }
}
} // namespace vb
