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
    constexpr double sampleModelMB = 220.0;   // 見本（モック）

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

ModelDownloadDialog::ModelDownloadDialog (Stage s, bool anim, float from)
    : DialogPanel (tr ("model.title"), tr ("model.micro")), stage (s), animate (anim)
{
    modelMB = sampleModelMB;
    build (from);
}

ModelDownloadDialog::ModelDownloadDialog (Stage s, UiSession& session)
    : DialogPanel (tr (session->modelDl.kind == 1 ? "model.lyrics.title" : "model.title"), tr ("model.micro")),
      stage (s), animate (false), live (&session), lyricsModel (session->modelDl.kind == 1)
{
    const auto& m = session->modelDl;
    modelMB = juce::jmax (1.0, (double) m.size / (1024.0 * 1024.0));
    if (m.title.isNotEmpty()) modelName = m.title;
    if (m.license.isNotEmpty()) modelLicense = m.license;
    build (-1.0f);
    readLive();
}

void ModelDownloadDialog::readLive()
{
    // 本物：UiSession が持つダウンロードの状態をそのまま見せる
    const auto& m = (*live)->modelDl;
    using DS = models::DownloadStatus::Stage;
    gotMB = (double) m.received / (1024.0 * 1024.0);
    speed = juce::jmax (0.0, m.bytesPerSecond / (1024.0 * 1024.0));
    verifying = m.stage == (int) DS::verifying;
    waitLeft = (float) m.retryIn;
    retry = juce::jmax (1, m.attempt);
    if (stage == Stage::done) gotMB = modelMB;
}

void ModelDownloadDialog::build (float from)
{
    switch (stage)
    {
        case Stage::confirm:
            addFooterKey (tr ("model.download", juce::roundToInt (modelMB)), KeyRole::primary,
                          [this] { if (live != nullptr) live->startModelDownload(); else handOff (Stage::downloading); });
            addFooterKey (tr (lyricsModel ? "model.lyrics.later" : "model.later"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });
            break;

        case Stage::downloading:
            if (live == nullptr)
                addFooterKey (tr ("model.pause"), KeyRole::normal, [] {});   // 本物は再生・録音の間だけ自動で止まる
            addFooterKey (tr ("common.cancel"), KeyRole::normal, [this]
            {
                if (live != nullptr) live->cancelModelDownload();   // 届いた分は残す（次は続きから）
                if (onCloseRequest) onCloseRequest();
            });
            gotMB = modelMB * (from >= 0.0f ? from : (animate ? 0.0f : 0.62f));
            break;

        case Stage::interrupted:
            // 届いた分は保存済み。少し待って続きから自動で再開（DESIGN 4.10「失敗と再開」）
            addFooterKey (tr ("model.resumeNow"), KeyRole::primary, [this] { if (live != nullptr) live->startModelDownload(); else handOff (Stage::downloading); });
            addFooterKey (tr ("common.cancel"), KeyRole::normal, [this]
            {
                if (live != nullptr) live->cancelModelDownload();
                if (onCloseRequest) onCloseRequest();
            });
            gotMB = std::floor (modelMB * (from >= 0.0f ? from : 0.38f));
            break;

        case Stage::done:
            addFooterKey (tr (lyricsModel ? "model.lyrics.continue" : "model.continue"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });
            gotMB = modelMB;
            flash = 1.0f;     // 開いた時に全体が一度光る
            break;

        case Stage::failed:
            addFooterKey (tr ("model.retry"), KeyRole::primary, [this] { if (live != nullptr) live->startModelDownload(); else handOff (Stage::downloading); });
            addFooterKey (tr (lyricsModel ? "model.lyrics.later" : "model.later"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });
            gotMB = modelMB;
            shakeT = 0.0;     // 開いた時に小さく揺れる
            break;
    }
    shownMB = gotMB;
    setSize (640, 460);

    if (stage != Stage::confirm)
        startAnimating();
}

ModelDownloadDialog::~ModelDownloadDialog() = default;

void ModelDownloadDialog::visibilityChanged()      { if (stage != Stage::confirm) startAnimating(); }
void ModelDownloadDialog::parentHierarchyChanged() { if (stage != Stage::confirm) startAnimating(); }

void ModelDownloadDialog::handOff (Stage next)
{
    if (handedOff) return;
    handedOff = true;
    if (onStage) onStage (next, next == Stage::downloading && stage == Stage::failed ? 0.0f : (float) (gotMB / modelMB));
}

void ModelDownloadDialog::simulate (float dt)
{
    // 通信のまね：不規則な間隔で、ばらついた量が届く（部品ラボ DL と同じ考え方。本物は B16）
    if (stage == Stage::downloading && ! verifying)
    {
        nextChunk -= dt;
        if (nextChunk <= 0.0)
        {
            chunkMB = 3.0 + rng.nextDouble() * 11.0;
            chunkDt = 0.1 + rng.nextDouble() * 0.3;
            nextChunk = chunkDt;
            gotMB = juce::jmin (modelMB, gotMB + chunkMB);
        }
        const auto raw = chunkMB / chunkDt;
        speed = speed + (raw - speed) * juce::jmin (1.0, (double) dt * 1.2);   // 速さは平均で
        if (gotMB >= modelMB)
        {
            verifying = true;
            verify = 0.0f;
        }
    }
    else if (stage == Stage::downloading && verifying)
    {
        verify += dt * 0.9f;   // 頭から照合
        if (verify >= 1.0f)
            handOff (Stage::done);
    }
    else if (stage == Stage::interrupted)
    {
        waitLeft -= dt;
        if (waitLeft <= 0.0f)
            handOff (Stage::downloading);   // 続きから
    }
}

bool ModelDownloadDialog::advanceAnimation (float dt)
{
    if (! isShowing())
        return false;   // 閉じた・隠れた（出たら visibilityChanged で戻る）

    const bool reduced = motion::prefersReducedMotion();
    clock += dt;
    if (live != nullptr)
        readLive();
    else if (animate && ! handedOff)
        simulate (dt);

    // 数値はなめらかに追う（動きを減らす設定なら即）
    shownMB = motion::approach ((float) shownMB, (float) gotMB, 8.0f, dt, reduced);
    if (std::abs (shownMB - gotMB) < 0.3) shownMB = gotMB;

    flash = reduced ? 0.0f : juce::jmax (0.0f, flash - dt * 1.6f);

    // 失敗：小さく揺れる（ダイアログ全体を横にずらすだけ）
    float dx = 0.0f;
    if (shakeT >= 0.0)
    {
        shakeT += dt;
        dx = reduced ? 0.0f : motion::shake::offset (shakeT);
        if (shakeT >= motion::shake::seconds) shakeT = -1.0;
    }
    setTransform (juce::AffineTransform::translation (dx, 0.0f));

    repaint();

    // 取得中・照合中・再開待ちは明滅し続ける。完了・失敗は光・揺れが終われば止まる
    const bool running = stage == Stage::downloading || stage == Stage::interrupted;
    return running || flash > 0.0f || shakeT >= 0.0;
}

//==============================================================================
void ModelDownloadDialog::paintTally (juce::Graphics& g, juce::Rectangle<float>& row, const juce::String& text)
{
    // タリー：琥珀＝取得中、青＝照合中、緑＝完了、赤＝失敗（DESIGN 4.10）
    const bool reduced = motion::prefersReducedMotion();
    juce::Colour c = colours::warn;
    float level = 1.0f;
    if (stage == Stage::downloading && verifying) c = colours::ref;
    else if (stage == Stage::downloading)        level = reduced ? 1.0f : 0.55f + 0.45f * (float) std::abs (std::sin (clock * 3.0));
    else if (stage == Stage::done)               c = colours::signal;
    else if (stage == Stage::failed)             c = colours::bad;

    const auto centre = juce::Point<float> (row.getX() + 5.0f, row.getCentreY());
    paint::glow (g, juce::Rectangle<float> (22.0f, 22.0f).withCentre (centre), c.withAlpha (0.45f * level));
    paint::led (g, centre, 3.2f, c, level);
    row.removeFromLeft (16.0f);
    paint::microLabel (g, row, text, c);
}

void ModelDownloadDialog::paintLadder (juce::Graphics& g, juce::Rectangle<float> bar)
{
    static constexpr int segs = 44;
    const bool reduced = motion::prefersReducedMotion();
    paint::inset (g, bar.expanded (2.0f), 3.0f);

    const auto gap = 2.0f;
    const auto w = (bar.getWidth() - gap * (float) (segs - 1)) / (float) segs;
    const auto lit = (float) (shownMB / modelMB) * (float) segs;
    const auto head = (int) std::floor (lit);
    const bool kept = stage == Stage::interrupted;

    // 1 回目は光、2 回目は粒（隣の光が粒に重ならないように）
    for (int pass = 0; pass < 2; ++pass)
    for (int i = 0; i < segs; ++i)
    {
        const auto seg = juce::Rectangle<float> (bar.getX() + (float) i * (w + gap), bar.getY(), w, bar.getHeight());
        juce::Colour c = colours::signal;
        float level = 0.0f;
        bool glowing = false;

        if (stage == Stage::downloading && verifying)
        {
            // 照合中：頭から青が走り、照合済みはライム
            const auto v = verify * (float) segs;
            level = (float) i < v ? 1.0f : 0.35f;
            if (std::abs ((float) i - v) < 1.5f) { c = colours::ref; level = 1.0f; glowing = true; }
        }
        else if (stage == Stage::failed)
        {
            c = colours::bad;
            level = 0.8f;
        }
        else if (i < head)
        {
            level = kept ? 0.55f : 1.0f;   // 切れた時：届いた分は点いたまま（少し暗く）
        }
        else if (i == head && kept)
        {
            // 切れた所の粒：琥珀で明滅
            c = colours::warn;
            level = reduced ? 1.0f : 0.35f + 0.65f * (float) std::abs (std::sin (clock * 5.0));
            glowing = true;
        }
        else if (i == head && lit > (float) head && stage == Stage::downloading)
        {
            // 先頭の 1 つ：届いた割合で明るさ、明滅
            const auto blink = reduced ? 1.0f : 0.55f + 0.45f * (float) std::sin (clock * 18.0);
            level = juce::jlimit (0.15f, 1.0f, (lit - (float) head) * blink + 0.15f);
            glowing = true;
        }

        if (pass == 0)
        {
            // 完了：全体が一度光る（粒ごとに光を重ねる）
            if (flash > 0.0f && level > 0.0f)
                paint::glow (g, seg.expanded (w * 1.4f, bar.getHeight() * 1.2f), c.withAlpha (0.55f * flash));
            else if (glowing)
                paint::glow (g, seg.expanded (w * 1.2f, bar.getHeight() * 0.9f), c.withAlpha (0.45f * level));
            continue;
        }
        paint::ledBar (g, seg, flash > 0.0f ? c.brighter (0.5f * flash) : c, level);
    }
}

void ModelDownloadDialog::paintBody (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();

    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawFittedText (tr (lyricsModel ? "model.lyrics.sub" : "model.sub"), r.removeFromTop (40.0f).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
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
        const auto name = modelName;
        g.drawText (name, top.removeFromLeft (textWidth (mono (14.0f, Weight::semibold), name) + 14.0f), juce::Justification::centredLeft, false);
        g.setColour (colours::textDim);
        g.setFont (sans (12.0f));
        g.drawText (tr (lyricsModel ? "model.lyrics.role" : "model.role"), top, juce::Justification::centredLeft, true);

        g.setColour (colours::textMute);
        g.setFont (sans (11.0f));
        g.drawText (tr ("model.meta", modelLicense), card, juce::Justification::centredLeft, true);
    }
    r.removeFromTop (18.0f);

    const auto mbText = juce::String (juce::roundToInt (shownMB)) + " / " + juce::String (juce::roundToInt (modelMB)) + " MB";

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
        case Stage::interrupted:
        {
            auto head = r.removeFromTop (24.0f);
            const auto pct = juce::String (juce::jmin (100, (int) std::floor (juce::roundToInt (shownMB) / modelMB * 100.0))) + "%";   // MB の表示とそろえる
            g.setColour (colours::text);
            g.setFont (mono (12.0f, Weight::semibold));
            g.drawText (pct, head.removeFromRight (60.0f), juce::Justification::centredRight, false);
            paintTally (g, head, stage == Stage::interrupted ? tr ("model.interrupted")
                               : verifying ? tr ("model.verifying") : tr ("model.downloading"));
            r.removeFromTop (4.0f);

            paintLadder (g, r.removeFromTop (12.0f).reduced (2.0f, 0.0f));
            r.removeFromTop (10.0f);

            auto line = r.removeFromTop (22.0f);
            g.setColour (colours::text);
            g.setFont (mono (12.0f, Weight::medium));
            if (stage == Stage::interrupted)
            {
                g.drawText (mbText, line, juce::Justification::centredLeft, false);
                g.setColour (colours::warn);
                g.setFont (sans (12.0f));
                const auto secs = juce::jmax (1, (int) std::ceil (waitLeft));
                // 本物で自動の再開を使い切った時は秒を出さない（「今すぐ再開」で続きから）
                if (live == nullptr || (*live)->modelDl.retryIn > 0)
                    g.drawText (tr ("model.resumeIn", secs, retry), line, juce::Justification::centredRight, false);
                r.removeFromTop (16.0f);
                infoLine (g, r, Icon::check, colours::signal, tr ("model.kept", juce::roundToInt (gotMB)));
                infoLine (g, r, Icon::shield, colours::signal, tr ("model.verifyAfter"));
                break;
            }

            if (verifying)
            {
                g.drawText (mbText, line, juce::Justification::centredLeft, false);
            }
            else
            {
                g.drawText (mbText + "   " + juce::String (speed, 1) + " MB/s", line, juce::Justification::centredLeft, false);

                // 残り時間：大きく変わった時と、1 秒ごとに減る時だけ更新（数字が暴れない）
                const auto rem = juce::jmax (0, (int) std::ceil ((modelMB - shownMB) / juce::jmax (1.0, speed)));
                if (remainShown < 0 || std::abs (rem - remainShown) >= 2 || (rem < remainShown && clock - remainChangedAt >= 1.0))
                {
                    remainShown = rem;
                    remainChangedAt = clock;
                }
                g.setColour (colours::textDim);
                g.setFont (sans (12.0f));
                g.drawText (tr ("model.remaining", remainShown), line, juce::Justification::centredRight, false);
            }
            r.removeFromTop (16.0f);

            infoLine (g, r, Icon::rec, colours::textMute, tr ("model.pauseOnPlay"));
            infoLine (g, r, Icon::shield, colours::signal, tr ("model.verifyAfter"));
            break;
        }

        case Stage::done:
        {
            paintLadder (g, r.removeFromTop (10.0f).reduced (2.0f, 0.0f));
            r.removeFromTop (12.0f);
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
            paintLadder (g, r.removeFromTop (10.0f).reduced (2.0f, 0.0f));
            r.removeFromTop (12.0f);
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
            infoLine (g, r, Icon::check, colours::textMute, tr (lyricsModel ? "model.lyrics.failed.safe" : "model.failed.safe"));
            break;
        }
    }
}
} // namespace vb
