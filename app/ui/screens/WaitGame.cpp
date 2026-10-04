#include "WaitGame.h"

namespace vb
{
namespace
{
    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    /** 音名から数字（オクターブ）を外す：C#4 → C#（どのオクターブでもよい） */
    juce::String pitchLetter (float midi)
    {
        return dummy::noteName (midi).trimCharactersAtEnd ("-0123456789");
    }
}

WaitGame::WaitGame (UiSession& u)
    : session (u),
      pitchKey (tr ("game.pitch")), rhythmKey (tr ("game.rhythm")), randomKey (tr ("game.random")),
      closeKey (tr ("game.close"), KeyButton::Kind::ghost), listenKey (tr ("game.pitch.listen")), againKey (tr ("game.rhythm.again"))
{
    setWantsKeyboardFocus (true);
    pitchKey.onClick = [this] { open (Kind::pitch); };
    rhythmKey.onClick = [this] { open (Kind::rhythm); };
    // おまかせ：マイクが使えなければリズムタップ
    randomKey.onClick = [this] { open (state().inputLive() && rng.nextBool() ? Kind::pitch : Kind::rhythm); };
    closeKey.onClick = [this] { close(); };
    listenKey.withShortcut ("L");
    listenKey.onClick = [this] { playTarget(); };
    againKey.onClick = [this] { startRhythm(); resized(); repaint(); };
    for (auto* k : { &pitchKey, &rhythmKey, &randomKey })
        addAndMakeVisible (k);
    for (auto* k : { &closeKey, &listenKey, &againKey })
        addChildComponent (k);
}

WaitGame::~WaitGame()
{
    onOpenChanged = nullptr;   // 呼ぶ側（起動画面）はもう壊れかけている
    close();
}

//==============================================================================
void WaitGame::open (Kind k)
{
    if (kind != Kind::none)
        close();
    kind = k;
    lastTickMs = nowMs();
    if (kind == Kind::pitch)
    {
        session.startGameVoice();
        hits = 0;
        hold = 0.0;
        target = 0.0f;
        nextTarget();
    }
    else
    {
        startRhythm();
    }
    startTimerHz (30);
    resized();
    repaint();
    if (isShowing())
        grabKeyboardFocus();   // Space でタップ・L で音を聴く
    if (onOpenChanged) onOpenChanged();
}

void WaitGame::close()
{
    if (kind == Kind::none)
        return;
    kind = Kind::none;
    stopTimer();
    session.stopGameVoice();
    session.setGameBeat (0.0);
    session.playGameTone (0.0f, 0.0);
    resized();
    repaint();
    if (onOpenChanged) onOpenChanged();
}

void WaitGame::timerCallback()
{
    const auto now = nowMs();
    const auto dt = juce::jlimit (0.0, 0.1, (now - lastTickMs) / 1000.0);
    lastTickMs = now;
    if (kind == Kind::pitch)
        tickPitch (dt);
    repaint();
}

//==============================================================================
void WaitGame::resized()
{
    for (auto* k : { &pitchKey, &rhythmKey, &randomKey })
        k->setVisible (! isOpen());
    closeKey.setVisible (isOpen());
    listenKey.setVisible (kind == Kind::pitch && state().output.open);
    againKey.setVisible (kind == Kind::rhythm && (int) offsets.size() >= tapsPerRound);

    auto r = getLocalBounds();
    if (! isOpen())
    {
        // 1 行：「待っている間に遊ぶ」と 3 つのキー
        r.removeFromLeft ((int) textWidth (sans (12.5f, Weight::medium), tr ("game.label")) + 14);
        for (auto* k : { &pitchKey, &rhythmKey, &randomKey })
        {
            k->setSize (10, 32);
            const auto w = k->idealWidth();
            k->setBounds (r.removeFromLeft (w).withSizeKeepingCentre (w, 32));
            r.removeFromLeft (8);
        }
        body = {};
        return;
    }

    auto head = r.removeFromTop (36);
    for (auto* k : { &closeKey, &listenKey, &againKey })
    {
        if (! k->isVisible()) continue;
        k->setSize (10, 32);
        const auto w = juce::jmax (90, k->idealWidth());
        k->setBounds (head.removeFromRight (w).withSizeKeepingCentre (w, 32));
        head.removeFromRight (8);
    }
    r.removeFromTop (30);   // 遊び方の 1 行
    body = r;
}

void WaitGame::paint (juce::Graphics& g)
{
    if (! isOpen())
    {
        g.setColour (colours::textDim);
        g.setFont (sans (12.5f, Weight::medium));
        g.drawText (tr ("game.label"), getLocalBounds(), juce::Justification::centredLeft, false);
        return;
    }

    auto r = getLocalBounds().toFloat();
    auto head = r.removeFromTop (36.0f);
    g.setColour (colours::text);
    g.setFont (sans (15.0f, Weight::semibold));
    g.drawText (tr (kind == Kind::pitch ? "game.pitch" : "game.rhythm"), head, juce::Justification::centredLeft, false);

    auto help = r.removeFromTop (30.0f);
    g.setColour (colours::textDim);
    const auto helpText = tr (kind == Kind::pitch ? "game.pitch.help" : "game.rhythm.help");
    g.setFont (sansFor (helpText, 12.0f));
    g.drawFittedText (helpText, help.toNearestInt(), juce::Justification::topLeft, 2, 0.9f);

    paint::inset (g, body.toFloat(), 6.0f);
    const auto inner = body.toFloat().reduced (20.0f, 14.0f);
    if (kind == Kind::pitch) paintPitch (g, inner);
    else                     paintRhythm (g, inner);
}

void WaitGame::mouseDown (const juce::MouseEvent& e)
{
    if (kind == Kind::rhythm && body.contains (e.getPosition()))
        tap();
}

bool WaitGame::keyPressed (const juce::KeyPress& k)
{
    if (kind == Kind::rhythm && k == juce::KeyPress::spaceKey)
    {
        tap();
        return true;
    }
    if (kind == Kind::pitch && (k.getTextCharacter() == 'l' || k.getTextCharacter() == 'L'))
    {
        listenKey.flash();
        playTarget();
        return true;
    }
    if (isOpen() && k == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

//==============================================================================
double WaitGame::pitchClassCents (float sungMidi, float targetMidi)
{
    auto d = (double) sungMidi - (double) targetMidi;
    d -= 12.0 * std::round (d / 12.0);
    return d * 100.0;
}

void WaitGame::nextTarget()
{
    // 声域を測ってあればその中（端は 2 半音あける）、無ければ A3〜A4
    const auto& s = state();
    int lo = 57, hi = 69;
    if (s.voiceLow > 0 && s.voiceHigh > s.voiceLow + 6)
    {
        lo = s.voiceLow + 2;
        hi = s.voiceHigh - 2;
    }
    if (target <= 0.0f)
        target = (float) (lo + rng.nextInt (hi - lo + 1));
    else
    {
        static constexpr int steps[] = { -5, -4, -3, -2, 2, 3, 4, 5, 7 };
        auto next = target;
        for (int tries = 0; tries < 12 && juce::approximatelyEqual (next, target); ++tries)
        {
            const auto c = target + (float) steps[rng.nextInt ((int) std::size (steps))];
            if (c >= (float) lo && c <= (float) hi) next = c;
        }
        target = juce::approximatelyEqual (next, target) ? (float) (lo + rng.nextInt (hi - lo + 1)) : next;
    }
    hold = 0.0;
    playTarget();
}

void WaitGame::playTarget()
{
    if (kind != Kind::pitch || ! state().output.open)
        return;
    session.playGameTone (target, 0.9);
    ignoreVoiceUntilMs = nowMs() + 1050.0;   // 鳴らした音をマイクが拾っても自分の声として数えない
}

void WaitGame::tickPitch (double dt)
{
    auto voice = session.gameVoiceMidi();
    if (nowMs() < ignoreVoiceUntilMs)
        voice = 0.0f;
    hasVoice = voice > 0.0f;
    if (! hasVoice)
    {
        hold = juce::jmax (0.0, hold - dt * 0.25);
        return;
    }
    const auto c = (float) pitchClassCents (voice, target);
    lastCents = std::abs (c - lastCents) > 150.0f ? c : lastCents * 0.5f + c * 0.5f;   // 少しならして、跳んだらそのまま
    if (std::abs (lastCents) <= hitCents) hold += dt;
    else                                  hold = juce::jmax (0.0, hold - dt * 0.5);
    if (hold >= holdToHit)
    {
        ++hits;
        hitFlashMs = nowMs();
        nextTarget();
    }
}

void WaitGame::paintPitch (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (! state().inputLive())
    {
        g.setColour (colours::warn);
        g.setFont (sansFor (tr ("game.pitch.noInput"), 14.0f));
        g.drawFittedText (tr ("game.pitch.noInput"), r.toNearestInt(), juce::Justification::centred, 2, 1.0f);
        return;
    }

    // 左：目標の音（大きく）。当たった瞬間は光る
    auto left = r.removeFromLeft (juce::jmin (220.0f, r.getWidth() * 0.32f));
    const auto flash = (float) juce::jmax (0.0, 1.0 - (nowMs() - hitFlashMs) / 450.0);
    if (flash > 0.0f)
        paint::glow (g, left.withSizeKeepingCentre (left.getWidth() * 0.8f, left.getHeight() * 0.9f), colours::signal.withAlpha (0.35f * flash));
    g.setColour (flash > 0.0f ? colours::signal.interpolatedWith (colours::ref, 1.0f - flash) : juce::Colour (colours::ref));
    g.setFont (sans (juce::jmin (64.0f, left.getHeight() * 0.55f), Weight::semibold));
    auto letter = left.removeFromTop (left.getHeight() * 0.72f);
    g.drawText (pitchLetter (target), letter, juce::Justification::centred, false);
    g.setColour (colours::textMute);
    g.setFont (sans (11.5f));
    g.drawText (tr ("game.pitch.anyOctave"), left, juce::Justification::centredTop, false);

    r.removeFromLeft (24.0f);
    // 右：目標からのずれ（±100 セント）。真ん中の帯に入れて保つ
    auto score = r.removeFromBottom (22.0f);
    g.setColour (colours::textDim);
    g.setFont (mono (12.0f));
    g.drawText (tr ("game.pitch.score", hits), score, juce::Justification::centredRight, false);

    auto meter = r.withSizeKeepingCentre (r.getWidth(), 30.0f).translated (0.0f, -12.0f);
    paint::inset (g, meter, 4.0f);
    const auto xFor = [&] (float cents) { return meter.getCentreX() + juce::jlimit (-100.0f, 100.0f, cents) / 100.0f * meter.getWidth() * 0.5f; };
    const auto band = juce::Rectangle<float>::leftTopRightBottom (xFor ((float) -hitCents), meter.getY() + 3.0f, xFor ((float) hitCents), meter.getBottom() - 3.0f);
    g.setColour (colours::signal.withAlpha (0.16f));
    g.fillRoundedRectangle (band, 3.0f);
    paint::vline (g, meter.getCentreX(), meter.getY() + 3.0f, meter.getBottom() - 3.0f, colours::signal.withAlpha (0.5f));
    if (hasVoice)
    {
        const bool in = std::abs (lastCents) <= hitCents;
        const auto x = xFor (lastCents);
        g.setColour (in ? juce::Colour (colours::signal) : juce::Colour (colours::warn));
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 3.0f, meter.getY() - 4.0f, 6.0f, meter.getHeight() + 8.0f), 3.0f);
    }
    g.setColour (colours::textMute);
    g.setFont (sans (11.0f));
    auto under = juce::Rectangle<float> (meter.getX(), meter.getBottom() + 4.0f, meter.getWidth(), 16.0f);
    g.drawText (tr ("game.pitch.low"), under, juce::Justification::centredLeft, false);
    g.drawText (tr ("game.pitch.high"), under, juce::Justification::centredRight, false);

    // 保った時間（10 個の LED）
    auto leds = juce::Rectangle<float> (meter.getCentreX() - 110.0f, under.getBottom() + 10.0f, 220.0f, 8.0f);
    const auto lit = juce::jlimit (0.0, 1.0, hold / holdToHit) * 10.0;
    for (int i = 0; i < 10; ++i)
        paint::ledBar (g, leds.withWidth (18.0f).translated ((float) i * 22.5f, 0.0f), colours::signal, juce::jlimit (0.0f, 1.0f, (float) (lit - i)));
}

//==============================================================================
WaitGame::TapStats WaitGame::tapStats (const std::vector<double>& x)
{
    TapStats st;
    if (x.empty())
        return st;
    for (auto v : x) st.mean += v;
    st.mean /= (double) x.size();
    for (auto v : x) st.spread += (v - st.mean) * (v - st.mean);
    st.spread = std::sqrt (st.spread / (double) x.size());
    return st;
}

void WaitGame::startRhythm()
{
    static constexpr double tempos[] = { 80.0, 90.0, 100.0, 110.0, 120.0 };
    bpm = tempos[rng.nextInt ((int) std::size (tempos))];
    offsets.clear();
    lastTapMs = 0.0;
    audioClock = state().engineAttached && state().output.open;
    visualStartMs = nowMs();
    session.setGameBeat (audioClock ? bpm : 0.0);
}

double WaitGame::rhythmClock() const
{
    return audioClock ? session.gameBeatClock() : (nowMs() - visualStartMs) / 1000.0;
}

void WaitGame::tap()
{
    if ((int) offsets.size() >= tapsPerRound)
        return;
    const auto t = rhythmClock();
    if (t < 0.0)
        return;
    const auto beatSec = 60.0 / bpm;
    const auto idx = std::round (t / beatSec);
    if (idx < (double) countInBeats)
        return;   // 最初の 4 拍は聴くだけ
    lastOffset = (t - idx * beatSec) * 1000.0;
    offsets.push_back (lastOffset);
    lastTapMs = nowMs();
    if ((int) offsets.size() >= tapsPerRound)
    {
        session.setGameBeat (0.0);
        resized();
    }
    repaint();
}

void WaitGame::paintRhythm (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto beatSec = 60.0 / bpm;
    const bool finished = (int) offsets.size() >= tapsPerRound;
    const auto t = finished ? -1.0 : rhythmClock();
    const auto beat = t >= 0.0 ? (juce::int64) std::floor (t / beatSec) : (juce::int64) -1;

    // テンポ（左上）
    g.setColour (colours::textMute);
    g.setFont (mono (11.0f));
    g.drawText (juce::String (juce::roundToInt (bpm)) + " BPM", r.removeFromTop (16.0f), juce::Justification::centredLeft, false);

    // 4 つの LED：拍の頭で点いて消えていく。聴くだけの 4 拍はアンバー、1 拍目は少し大きく
    auto ledRow = r.removeFromTop (44.0f);
    for (int i = 0; i < 4; ++i)
    {
        const auto c = juce::Point<float> (ledRow.getCentreX() + ((float) i - 1.5f) * 56.0f, ledRow.getCentreY());
        float level = 0.0f;
        if (beat >= 0 && (int) (beat % 4) == i)
            level = (float) juce::jmax (0.0, 1.0 - (t - (double) beat * beatSec) / 0.18);
        const auto col = beat < countInBeats ? juce::Colour (colours::warn) : juce::Colour (colours::signal);
        paint::led (g, c, i == 0 ? 11.0f : 9.0f, col, level);
    }

    // いまの様子：聴いて… / 何回目 / 最後のずれ / 結果
    auto status = r.removeFromTop (40.0f);
    if (finished)
    {
        const auto st = tapStats (offsets);
        g.setColour (colours::text);
        g.setFont (sans (16.0f, Weight::semibold));
        g.drawText (tr ("game.rhythm.result", (st.mean >= 0.0 ? "+" : "") + juce::String (st.mean, 0), juce::String (st.spread, 0)),
                    status, juce::Justification::centred, false);
    }
    else if (beat < countInBeats)
    {
        g.setColour (colours::warn);
        g.setFont (sans (15.0f, Weight::medium));
        g.drawText (tr ("game.rhythm.listen"), status, juce::Justification::centred, false);
    }
    else
    {
        const auto ms = juce::roundToInt (std::abs (lastOffset));
        const bool recent = nowMs() - lastTapMs < 900.0 && ! offsets.empty();
        const auto txt = ! recent ? juce::String()
                       : ms <= 10 ? tr ("game.rhythm.onBeat")
                       : lastOffset < 0.0 ? tr ("game.rhythm.early", ms) : tr ("game.rhythm.late", ms);
        g.setColour (ms <= 20 ? juce::Colour (colours::signal) : (ms <= 50 ? juce::Colour (colours::text) : juce::Colour (colours::warn)));
        g.setFont (sans (16.0f, Weight::semibold));
        g.drawText (txt, status, juce::Justification::centred, false);
    }

    // ずれの点（± 100 ms。左が早い・右が遅い）と回数
    auto axis = r.removeFromTop (34.0f).withSizeKeepingCentre (juce::jmin (420.0f, r.getWidth()), 34.0f);
    const auto y = axis.getCentreY();
    paint::hline (g, std::round (y), axis.getX(), axis.getRight(), colours::lineHi);
    paint::vline (g, axis.getCentreX(), y - 9.0f, y + 9.0f, colours::signal.withAlpha (0.6f));
    for (auto v : offsets)
    {
        const auto x = axis.getCentreX() + (float) juce::jlimit (-100.0, 100.0, v) / 100.0f * axis.getWidth() * 0.5f;
        g.setColour ((std::abs (v) <= 20.0 ? juce::Colour (colours::signal) : juce::Colour (colours::warn)).withAlpha (0.85f));
        g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
    }
    g.setColour (colours::textMute);
    g.setFont (sans (10.5f));
    g.drawText (tr ("game.rhythm.earlySide"), axis.withTrimmedTop (22.0f), juce::Justification::bottomLeft, false);
    g.drawText (tr ("game.rhythm.lateSide"), axis.withTrimmedTop (22.0f), juce::Justification::bottomRight, false);
    g.setFont (mono (11.0f));
    g.drawText (juce::String ((int) offsets.size()) + " / " + juce::String (tapsPerRound), r.removeFromBottom (18.0f), juce::Justification::centredRight, false);

    if (! audioClock)
    {
        g.setColour (colours::warn);
        g.setFont (sans (11.5f));
        g.drawText (tr ("game.rhythm.silent"), r.removeFromBottom (18.0f), juce::Justification::centredLeft, false);
    }
}
} // namespace vb
