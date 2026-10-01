#include "UiSession.h"
#include "audio/DeviceRules.h"
#include "audio/InputMeter.h"

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
    if (engine != nullptr)
        engine->setDeviceChangeCallback (nullptr);

    engine = e;
    s.engineAttached = e != nullptr;

    if (engine != nullptr)
    {
        // デバイスが外から変わった（抜けた等）。エンジンはメッセージスレッドから呼ぶ
        engine->setDeviceChangeCallback ([this] (bool lost) { deviceChanged (lost); });

        // ここからはダミーの値を出さない（入力が開くまでメーターは消灯）
        s.inputPeakDb = s.inputRmsDb = s.inputPeakHoldDb = audio::InputMeter::floorDb;
        s.inputClipped = false;
    }
    refreshOutputStatus();
    refreshInputStatus();
    notify (change::device | change::meter);
}

bool UiSession::isEngineDriven() const
{
    return engine != nullptr && s.output.open && engine->hasSong();
}

void UiSession::refreshOutputStatus()
{
    s.output = engine != nullptr ? engine->getOutputStatus() : audio::OutputStatus {};
}

void UiSession::refreshInputStatus()
{
    if (engine == nullptr)
    {
        s.input = {};   // UI_MOCK：入力の表示はダミーのまま
        return;
    }

    s.input = engine->getInputStatus();
    s.inputDevice = s.input.deviceName;
    s.driver      = s.input.typeName.isNotEmpty() ? s.input.typeName : s.output.typeName;
    s.bufferSize  = s.input.bufferSize > 0 ? s.input.bufferSize : s.output.bufferSize;

    if (! s.input.open)
    {
        s.inputPeakDb = s.inputRmsDb = s.inputPeakHoldDb = audio::InputMeter::floorDb;
        s.inputClipped = false;
    }
}

void UiSession::pollInput()
{
    if (engine == nullptr || ! s.input.open)
        return;

    const auto l = engine->getInputLevel();
    auto same = [] (float a, float b) { return std::abs (a - b) < 0.05f; };
    if (same (l.peakDb, s.inputPeakDb) && same (l.rmsDb, s.inputRmsDb) && same (l.holdDb, s.inputPeakHoldDb)
        && l.clipped == s.inputClipped)
        return;

    s.inputPeakDb = l.peakDb;
    s.inputRmsDb = l.rmsDb;
    s.inputPeakHoldDb = l.holdDb;
    s.inputClipped = l.clipped;
    notify (change::meter);
}

void UiSession::resetInputClip()
{
    if (engine != nullptr)
        engine->resetInputClip();
    s.inputClipped = false;
    notify (change::meter);
}

void UiSession::deviceChanged (bool lost)
{
    // DESIGN 13「デバイス抜け：停止して再選択」。エンジン側はもう止まっている
    if (engine != nullptr)
        engine->stop();
    s.isPlaying = s.isRecording = false;
    if (lost)
        ++s.deviceLostCount;

    refreshOutputStatus();
    refreshInputStatus();
    notify (change::device | change::meter | change::transport);
}

audio::DeviceList UiSession::getDeviceList() const
{
    return engine != nullptr ? engine->getDeviceList() : audio::DeviceList {};
}

void UiSession::rescanDevices()
{
    if (engine == nullptr) return;
    engine->rescanDevices();
    notify (change::device);
}

juce::String UiSession::afterDeviceSelect (juce::String error)
{
    refreshOutputStatus();
    refreshInputStatus();
    notify (change::device | change::meter);
    return error;
}

// 切り替えるとデバイスが開き直すので、先に再生を止める
juce::String UiSession::selectDeviceType (const juce::String& t)    { if (engine == nullptr) return {}; stop(); return afterDeviceSelect (engine->setDeviceType (t)); }
juce::String UiSession::selectInputDevice (const juce::String& n)   { if (engine == nullptr) return {}; stop(); return afterDeviceSelect (engine->setInputDevice (n)); }
juce::String UiSession::selectOutputDevice (const juce::String& n)  { if (engine == nullptr) return {}; stop(); return afterDeviceSelect (engine->setOutputDevice (n)); }
juce::String UiSession::selectInputChannel (int ch)                 { if (engine == nullptr) return {}; stop(); return afterDeviceSelect (engine->setInputChannel (ch)); }
juce::String UiSession::selectBufferSize (int n)                    { if (engine == nullptr) return {}; stop(); return afterDeviceSelect (engine->setBufferSize (n)); }

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
    // デバイスの状態はときどき見直す（抜けた・SR が変わった・入力が無音のまま）
    sinceStatus += seconds;
    if (engine != nullptr && sinceStatus >= 1.0)
    {
        sinceStatus = 0.0;
        const auto before = s.output;
        const auto in = s.input;
        refreshOutputStatus();
        refreshInputStatus();
        const auto& i = s.input;
        if (before.open != s.output.open || std::abs (before.sampleRate - s.output.sampleRate) > 0.5
            || before.bufferSize != s.output.bufferSize || before.deviceName != s.output.deviceName
            || in.open != i.open || in.problem != i.problem || in.deviceName != i.deviceName || in.channel != i.channel
            || in.numChannels != i.numChannels || in.silent != i.silent || in.permission != i.permission
            || in.inputLatency != i.inputLatency || in.outputLatency != i.outputLatency || in.bufferSize != i.bufferSize)
            notify (change::device | change::meter);
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
//==============================================================================
juce::String inputChannelLabel (int channel, int numChannels)
{
    if (numChannels <= 1) return {};
    if (numChannels == 2) return channel == 0 ? tr ("input.channel.left") : tr ("input.channel.right");
    return tr ("input.channel.n", channel + 1);
}

juce::String inputDisplayName (const dummy::Session& s)
{
    if (! s.engineAttached) return s.inputDevice;          // UI_MOCK：ダミー
    if (! s.input.open)     return tr ("input.none");

    const auto ch = inputChannelLabel (s.input.channel, s.input.numChannels);
    return ch.isEmpty() ? s.input.deviceName
                        : s.input.deviceName + juce::String::fromUTF8 (" \xe2\x80\x94 ") + ch;   // 機器名はデータ（翻訳しない）
}

juce::String inputProblemShort (const dummy::Session& s)
{
    if (! s.engineAttached) return {};
    if (s.input.open)       return s.input.silent ? tr ("status.input.silent") : juce::String();

    switch (s.input.problem)
    {
        case audio::InputProblem::permissionDenied: return tr ("status.input.denied");
        case audio::InputProblem::permissionAsking: return tr ("status.input.asking");
        case audio::InputProblem::openFailed:
        case audio::InputProblem::noChannels:       return tr ("status.input.failed");
        case audio::InputProblem::stalled:          return tr ("status.input.stalled");
        case audio::InputProblem::noDevice:
        case audio::InputProblem::none:             break;
    }
    return tr ("status.input.none");
}

LatencyDisplay latencyDisplay (const dummy::Session& s)
{
    LatencyDisplay d;
    if (! s.engineAttached)
    {
        // UI_MOCK：ダミーの補正量
        d.known = true;
        d.samples = s.latencySamples;
        d.ms = (double) s.latencySamples * 1000.0 / s.sampleRate();
        return d;
    }
    if (! s.input.open || s.input.sampleRate <= 0.0)
        return d;

    const auto r = audio::reportedLatency (s.input.inputLatency, s.input.outputLatency, s.input.bufferSize);
    d.known = true;
    d.reported = true;
    d.estimated = r.estimated;
    d.samples = r.samples;
    d.ms = (double) r.samples * 1000.0 / s.input.sampleRate;
    return d;
}
} // namespace vb
