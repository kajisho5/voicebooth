#include "UiSession.h"

namespace vb
{
UiSession::UiSession() : s (dummy::makeSession()) {}

void UiSession::notify (juce::uint32 changes)
{
    listeners.call ([changes] (Listener& l) { l.sessionChanged (changes); });
}

//==============================================================================
void UiSession::loadSong (const juce::File& file, int sampleRate, int64 lengthSamples,
                          std::shared_ptr<const audio::WaveformOverview> wave,
                          std::shared_ptr<const audio::SongAudio> audio)
{
    s = dummy::makeSongSession (s, file.getFileNameWithoutExtension(), file.getFullPathName(),
                                sampleRate, lengthSamples, std::move (wave));

    if (engine != nullptr)
    {
        engine->setSong (std::move (audio));   // 止まって頭へ。出力の SR を曲に合わせる
        engine->setBackingLevel (s.offVocalGain, s.backingMuted);
        syncLoopToEngine();
        refreshOutputStatus();
    }
    notify (change::all);
}

void UiSession::attachEngine (audio::AudioEngine* e)
{
    engine = e;
    s.engineAttached = e != nullptr;
    refreshOutputStatus();
    notify (change::device);
}

bool UiSession::isEngineDriven() const
{
    return engine != nullptr && s.output.open && engine->hasSong();
}

void UiSession::refreshOutputStatus()
{
    s.output = engine != nullptr ? engine->getOutputStatus() : audio::OutputStatus {};
}

void UiSession::syncLoopToEngine()
{
    if (engine != nullptr)
        engine->setLoop (s.rangeIn, s.rangeOut, s.loopOn && s.hasRange());
}

void UiSession::setBackingLevel (float fader)
{
    s.offVocalGain = juce::jlimit (0.0f, 1.0f, fader);
    if (engine != nullptr) engine->setBackingLevel (s.offVocalGain, s.backingMuted);
    notify (change::monitor);
}

void UiSession::setBackingMuted (bool m)
{
    s.backingMuted = m;
    if (engine != nullptr) engine->setBackingLevel (s.offVocalGain, s.backingMuted);
    notify (change::monitor);
}

//==============================================================================
void UiSession::setPlaying (bool p)
{
    if (s.isPlaying == p) return;

    // 終わりにいるときの再生は頭から
    if (p && s.playhead >= s.project.lengthSamples)
        seek (s.hasRange() && s.loopOn ? s.rangeIn : 0);

    s.isPlaying = p;
    if (! p && s.isRecording) s.isRecording = false;   // 止めたら録音も止まる
    if (isEngineDriven()) { if (p) engine->play(); else engine->stop(); }
    notify (change::transport);
}

void UiSession::setRecording (bool r)
{
    if (s.isRecording == r) return;
    s.isRecording = r;
    if (r)
    {
        s.isPlaying = true;          // 録音は再生と同時（DESIGN 6.3）。録音そのものは B5
        s.recordStart = s.playhead;
        if (isEngineDriven()) engine->play();
    }
    notify (change::transport | change::practice);
}

void UiSession::stop()
{
    s.isPlaying = false;
    s.isRecording = false;
    if (engine != nullptr) engine->stop();
    notify (change::transport);
}

void UiSession::goToStart()
{
    seek (s.hasRange() && s.loopOn ? s.rangeIn : 0);
}

void UiSession::seek (int64 sample)
{
    s.playhead = juce::jlimit ((int64) 0, s.project.lengthSamples, sample);
    if (s.isRecording) s.recordStart = s.playhead;
    if (isEngineDriven()) engine->seek (s.playhead);
    keepPlayheadInView();
    notify (change::playhead);
}

void UiSession::tick (double seconds)
{
    // 出力デバイスの状態はときどき見直す（抜けた・SR が変わった）
    sinceStatus += seconds;
    if (engine != nullptr && sinceStatus >= 1.0)
    {
        sinceStatus = 0.0;
        const auto before = s.output;
        refreshOutputStatus();
        if (before.open != s.output.open || std::abs (before.sampleRate - s.output.sampleRate) > 0.5
            || before.bufferSize != s.output.bufferSize || before.deviceName != s.output.deviceName)
            notify (change::device);
    }

    if (! s.isPlaying) return;

    // 曲を開いていれば、位置は鳴っている音（エンジン）から取る
    if (isEngineDriven())
    {
        const auto pos = juce::jlimit ((int64) 0, s.project.lengthSamples, engine->getPlayheadSample());
        if (s.isRecording && s.loopOn && s.hasRange() && pos < s.playhead && s.playhead >= s.rangeIn)
            s.recordStart = s.rangeIn;   // ループで戻った

        const bool ended = engine->consumeReachedEnd() || ! engine->isPlaying();
        s.playhead = pos;
        keepPlayheadInView();
        if (ended)
        {
            s.isPlaying = s.isRecording = false;
            notify (change::playhead | change::transport);
            return;
        }
        notify (change::playhead);
        return;
    }

    auto next = s.playhead + (int64) std::llround (seconds * s.sampleRate() * s.tempoPercent / 100.0);

    if (s.loopOn && s.hasRange() && s.playhead < s.rangeOut && next >= s.rangeOut)
    {
        next = s.rangeIn + (next - s.rangeOut);
        if (s.isRecording) s.recordStart = s.rangeIn;
    }

    if (next >= s.project.lengthSamples)
    {
        s.playhead = s.project.lengthSamples;
        s.isPlaying = s.isRecording = false;
        notify (change::playhead | change::transport);
        return;
    }

    s.playhead = next;
    keepPlayheadInView();
    notify (change::playhead);
}

void UiSession::keepPlayheadInView()
{
    const auto len = s.viewEnd - s.viewStart;
    if (s.playhead >= s.viewStart && s.playhead <= s.viewStart + len * 85 / 100)
        return;

    // 再生ヘッドが 1/4 の位置に来るようにページ送り
    auto start = juce::jlimit ((int64) 0, juce::jmax ((int64) 0, s.project.lengthSamples - len), s.playhead - len / 4);
    s.viewStart = start;
    s.viewEnd = start + len;
    notify (change::view);
}

void UiSession::setLoop (bool l)          { if (s.loopOn != l) { s.loopOn = l; syncLoopToEngine(); notify (change::transport | change::range); } }
void UiSession::setCountIn (int bars)     { s.countInBars = juce::jlimit (0, 2, bars); notify (change::transport); }
void UiSession::setClick (bool c)         { s.clickOn = c; notify (change::transport); }

void UiSession::setRange (int64 in, int64 out)
{
    if (out < in) std::swap (in, out);
    s.rangeIn = juce::jlimit ((int64) 0, s.project.lengthSamples, in);
    s.rangeOut = juce::jlimit ((int64) 0, s.project.lengthSamples, out);
    syncLoopToEngine();
    notify (change::range);
}

void UiSession::clearRange()
{
    s.rangeIn = s.rangeOut = 0;
    syncLoopToEngine();
    notify (change::range);
}

void UiSession::setRangeInAtPlayhead()
{
    const auto out = s.rangeOut > s.playhead ? s.rangeOut : s.project.lengthSamples;
    setRange (s.playhead, out);
}

void UiSession::setRangeOutAtPlayhead()
{
    const auto in = (s.hasRange() && s.rangeIn < s.playhead) ? s.rangeIn : 0;
    setRange (in, s.playhead);
}

//==============================================================================
void UiSession::setView (int64 start, int64 end)
{
    s.viewStart = start;
    s.viewEnd = end;
    notify (change::view);
}

void UiSession::setOctaveAlign (bool b) { s.octaveAlign = b; notify (change::view); }
void UiSession::setOctaveUp (bool b)    { s.octaveUp = b; notify (change::view); }

void UiSession::setFullRange (bool b)
{
    s.fullRange = b;
    s.lowMidi  = b ? 36 : 48;   // 全体: C2–C7 / 既定: C3–C6
    s.highMidi = b ? 96 : 84;
    notify (change::view);
}

//==============================================================================
void UiSession::selectTrack (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) s.trackUi.size())) return;
    if (! isTrackVisible (s.trackUi[(size_t) index].type)) return;
    s.selectedTrack = index;
    notify (change::tracks);
}

void UiSession::armTrack (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) s.trackUi.size())) return;
    const bool wasArmed = s.trackUi[(size_t) index].armed;
    for (auto& t : s.trackUi) t.armed = false;
    s.trackUi[(size_t) index].armed = ! wasArmed;
    if (! wasArmed) s.selectedTrack = index;   // アームしたトラックを前面に
    notify (change::tracks);
}

void UiSession::setMute (int i, bool m) { if (juce::isPositiveAndBelow (i, (int) s.trackUi.size())) { s.trackUi[(size_t) i].mute = m; notify (change::tracks); } }
void UiSession::setSolo (int i, bool v) { if (juce::isPositiveAndBelow (i, (int) s.trackUi.size())) { s.trackUi[(size_t) i].solo = v; notify (change::tracks); } }

//==============================================================================
void UiSession::setTempo (int p)              { s.tempoPercent = juce::jlimit (50, 150, p); notify (change::practice); }
void UiSession::setKey (int k)                { s.keyShift = juce::jlimit (-6, 6, k); notify (change::practice); }
void UiSession::setRecMode (project::RecMode m) { s.recMode = m; notify (change::practice); }
void UiSession::setPitchTolerance (float c)   { s.pitchToleranceCents = c; notify (change::view); }

void UiSession::setMode (project::Mode m)
{
    s.mode = m;
    // 見えなくなったトラックが選択中なら Main に戻す（データは消さない）
    if (! isTrackVisible (s.currentTrack().type))
        s.selectedTrack = 0;
    notify (change::mode | change::tracks);
}

bool UiSession::isTrackVisible (project::TrackType t) const
{
    using project::Mode;
    using project::TrackType;
    switch (t)
    {
        case TrackType::doubleTrack:
        case TrackType::harm1:   return s.mode != Mode::easy;    // 簡単: Main のみ
        case TrackType::harm2:   return s.mode == Mode::pro;     // Harm 2 はプロのみ
        case TrackType::backing:
        case TrackType::guide:
        case TrackType::main:    return true;
    }
    return true;
}
} // namespace vb
