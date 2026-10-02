#include "UiSession.h"
#include "audio/Retro.h"
#include "Animator.h"
#include "audio/DeviceRules.h"
#include "audio/InputMeter.h"
#include "project/Comp.h"
#include "export/ExportService.h"
#include "Timeline.h"
#include "audio/Resample.h"

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
    finishRecording();   // 録音中に別の曲を開いたら、そこまでのテイクは残す
    s = dummy::makeSongSession (s, file.getFileNameWithoutExtension(), file.getFullPathName(),
                                sampleRate, lengthSamples, std::move (wave));
    s.projectFolder = projectFolderFor (s.songName);
    s.songRate = sampleRate;
    // 前に落ちた時の裏録りの残り（B7。REC にならなかった分）を消す
    for (auto& f : s.projectFolder.getChildFile ("Audio/Takes").findChildFiles (juce::File::findFiles, false, ".retro-*.wav"))
        f.deleteFile();
    s.songOriginal = audio;

    if (engine != nullptr)
    {
        engine->setSong (std::move (audio));   // 止まって頭へ。出力の SR を曲に合わせる
        engine->setBackingLevel (s.offVocalGain, s.backingMuted);
        syncLoopToEngine();
        refreshOutputStatus();
    }
    notify (change::all);

    // 録音の SR を曲と違う値にしていれば、伴奏をその SR にそろえる（裏で）
    if (s.targetRate() != s.songRate)
        conformSong();
}

//==============================================================================
bool UiSession::hasTakes() const
{
    for (auto& t : s.project.tracks)
        if (! t.takes.empty())
            return true;
    return false;
}

void UiSession::setRecordFormat (double rate, bool floatSamples)
{
    const bool rateChanged = std::abs (rate - s.recordRate) > 0.5;
    s.recordFloat = floatSamples;
    s.recordRate = rate;
    s.project.bitDepthExport = floatSamples ? 32 : 24;
    notify (change::recordFormat | change::practice);

    if (! rateChanged || s.songOriginal == nullptr || s.isRecording)
        return;

    // テイクがある曲は時間軸を変えない（録った声の SR と合わなくなる）。次に開く曲から
    if (hasTakes())
    {
        if (s.targetRate() != s.sampleRate())
            postNotice (tr ("format.rateLater", formatKhz (s.sampleRate()), formatKhz (s.targetRate())));
        return;
    }
    conformSong();
}

void UiSession::conformSong()
{
    const auto target = s.targetRate();
    if (s.songOriginal == nullptr || target <= 0 || target == s.sampleRate() || s.conforming)
        return;

    if (s.isPlaying) stop();
    s.conforming = true;
    postNotice (tr ("format.conforming", formatKhz (target)));
    notify (change::recordFormat);

    // 元の伴奏から作り直す（何度変えても劣化しない）。概形もそろえた SR で作り直す
    const auto original = s.songOriginal;
    const auto serial = s.songSerial;
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, original, target, serial]
    {
        auto audio = audio::resampleSong (*original, (double) target);
        std::shared_ptr<audio::WaveformOverview> wave;
        if (audio != nullptr)
        {
            wave = std::make_shared<audio::WaveformOverview> (audio->length());
            std::vector<const float*> ch;
            for (int c = 0; c < audio->buffer.getNumChannels(); ++c)
                ch.push_back (audio->buffer.getReadPointer (c));
            wave->append (ch.data(), (int) ch.size(), audio->buffer.getNumSamples());
        }
        juce::MessageManager::callAsync ([this, weak, audio, wave, target, serial]
        {
            if (weak.expired() || serial != s.songSerial)
                return;   // 消えた・別の曲を開いた
            s.conforming = false;
            if (audio == nullptr)
            {
                postNotice (tr ("format.conformFailed", formatKhz (target)));
                notify (change::recordFormat);
                return;
            }

            // 位置をすべて新しい SR のサンプルに直す（時刻は変えない）
            const auto ratio = (double) target / (double) s.sampleRate();
            auto scale = [ratio] (int64 v) { return (int64) std::llround ((double) v * ratio); };
            s.playhead = scale (s.playhead);
            s.rangeIn = scale (s.rangeIn);
            s.rangeOut = scale (s.rangeOut);
            s.viewStart = scale (s.viewStart);
            s.viewEnd = scale (s.viewEnd);
            auto scaleTimed = [&scale] (int64 v) { return v >= 0 ? scale (v) : v; };   // -1（時刻なし）はそのまま
            s.project.tempo.downbeatSample = scale (s.project.tempo.downbeatSample);
            for (auto& b : s.project.tempo.beats) b = scale (b);
            for (auto& sec : s.project.sections) sec.startSample = scale (sec.startSample);
            for (auto& l : s.project.lyrics.lines) { l.startSample = scaleTimed (l.startSample); l.endSample = scaleTimed (l.endSample); }
            s.project.sampleRate = target;
            s.project.lengthSamples = audio->length();
            s.backingWave = wave;

            if (engine != nullptr)
            {
                engine->setSong (audio);   // デバイスもこの SR に切り替える（対応していれば）
                engine->setBackingLevel (s.offVocalGain, s.backingMuted);
                engine->seek (s.playhead);
                syncLoopToEngine();
                refreshOutputStatus();
            }
            postNotice (tr ("format.conformed", formatKhz (target), formatBits (s.project.bitDepthExport)));
            notify (change::all);
        });
    });
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
    pushMonitorToEngine();
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
    if (engine != nullptr)
        checkSpeakerOutput();
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
    // DESIGN 13「デバイス抜け：停止して再選択」。エンジン側はもう止まっている。
    // 録音中なら、そこまでをテイクとして閉じる（録った声は捨てない）
    finishRecording();
    if (engine != nullptr)
        engine->stop();
    s.isPlaying = s.isRecording = false;
    if (s.latencyMeasuring)
    {
        // 測っている途中で機器が変わった：やめて、失敗として出す
        if (engine != nullptr)
            engine->cancelLatencyProbe();
        s.latencyMeasuring = false;
        s.latencyResult = {};
        s.latencyHasResult = true;
        notify (change::latency);
    }
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

void UiSession::setSelfMonitorLevel (float fader)
{
    s.monitorGain = juce::jlimit (0.0f, 1.0f, fader);
    pushMonitorToEngine();
    notify (change::monitor);
}

void UiSession::setSelfMonitorMuted (bool m)
{
    s.selfMuted = m;
    pushMonitorToEngine();
    notify (change::monitor);
}

void UiSession::setMonitorReverb (float fader)
{
    s.monitorReverb = juce::jlimit (0.0f, 1.0f, fader);
    pushMonitorToEngine();
    notify (change::monitor);
}

void UiSession::pushMonitorToEngine()
{
    if (engine == nullptr)
        return;
    engine->setSelfMonitor (s.monitorGain, s.selfMuted);
    engine->setMonitorReverb (s.monitorReverb);
}

void UiSession::checkSpeakerOutput()
{
    // 出力の機器が替わった時だけ判定する（ミュートを自分で外したら、同じ機器のうちは尊重する）。
    // 閉じている間は判定しない（抜けて同じ機器に戻っただけで、外したミュートをかけ直さない）
    const auto name = s.output.open ? s.output.deviceName : juce::String();
    if (name.isEmpty() || name == s.speakerCheckedFor)
        return;

    s.speakerCheckedFor = name;
    s.speakerOutput = name.isNotEmpty() && audio::looksLikeSpeakers (name);
    if (s.speakerOutput && ! s.selfMuted)
    {
        s.selfMuted = true;    // スピーカーから自分の声を返すとハウリングする（DESIGN 7.3 / B4）
        pushMonitorToEngine();
    }
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
    if (! p && s.isRecording)
    {
        finishRecording();   // 止めたら録音も止まる（そこまでをテイクにする）
        s.isRecording = false;
    }
    if (isEngineDriven()) { if (p) engine->play(); else engine->stop(); }
    notify (change::transport);
}

void UiSession::setRecording (bool r)
{
    if (s.isRecording == r) return;

    if (! r)
    {
        finishRecording();
        s.isRecording = false;
        notify (change::transport | change::practice);
        return;
    }

    // UI_MOCK・デモ（曲を開いていない）：見た目だけ（Phase A）
    if (! isEngineDriven())
    {
        s.isRecording = true;
        s.isPlaying = true;          // 録音は再生と同時（DESIGN 6.3）
        s.recordStart = s.playhead;
        notify (change::transport | change::practice);
        return;
    }

    // 通し録音（B5）。録れない時は理由を知らせて何もしない
    if (const auto problem = recordProblem(); problem.isNotEmpty())
    {
        postNotice (problem == "record.problem.sampleRate" ? tr (problem.toRawUTF8(), formatKhz (s.sampleRate()))
                                                          : tr (problem.toRawUTF8()));
        return;
    }

    auto* armed = [this]() -> const dummy::TrackUi*
    {
        for (auto& t : s.trackUi) if (t.armed) return &t;
        return nullptr;
    }();
    auto* track = const_cast<project::Track*> (s.project.findTrack (armed->type));

    // 名前は take1, take2 …。同じ名前のファイルがフォルダにあれば番号を進める（前に録った声を上書きしない）
    const bool practice = s.recMode == project::RecMode::practice;
    const auto key = juce::String (project::trackKey (armed->type));
    auto id = project::nextTakeId (*track);
    auto rel = [&] { return (practice ? "Practice/" : "Audio/Takes/") + key + "_" + id + ".wav"; };
    for (int n = id.substring (4).getIntValue(); s.projectFolder.getChildFile (rel()).exists(); )
        id = "take" + juce::String (++n);

    // 再生中で裏で録っていれば（B7）、それをこのテイクにする。押す前に歌い始めていれば、フレーズの頭から採る
    if (shadowActive && s.isPlaying && engine->isRecording() && engine->recordingStartSample() >= 0)
    {
        shadowActive = false;
        loopBeforeRecording = s.loopOn && s.hasRange();
        engine->setLoop (s.rangeIn, s.rangeOut, false);
        s.recordingTake = id;
        s.recordingTrack = armed->type;
        s.recordingPath = rel();
        s.isRecording = true;
        s.recordStart = retroStart (juce::jlimit ((int64) 0, s.project.lengthSamples, engine->getPlayheadSample()));
        notify (change::transport | change::practice);
        return;
    }

    if (s.playhead >= s.project.lengthSamples)
        seek (0);

    // 録音中はループしない（通し録音。区間の録り直しは B10）
    loopBeforeRecording = s.loopOn && s.hasRange();
    engine->setLoop (s.rangeIn, s.rangeOut, false);

    // 往復の遅れ（B6）：テイクの頭をこの分だけ前にずらす。曲の終わりの後もこの分だけ録り足す
    const auto ld = latencyDisplay (s);
    s.recordingLatency = ld.known ? juce::jmax ((int64) 0, ld.samples) : 0;
    const auto error = engine->startRecording (s.projectFolder.getChildFile (rel()), s.recordFloat, s.recordingLatency);
    if (error.isNotEmpty())
    {
        syncLoopToEngine();
        postNotice (tr ("record.problem.failed", error));
        return;
    }

    s.recordingTake = id;
    s.recordingTrack = armed->type;
    s.recordingPath = rel();
    s.isRecording = true;
    s.isPlaying = true;
    s.recordStart = s.playhead;
    engine->play();
    notify (change::transport | change::practice);
}

juce::String UiSession::recordProblem() const
{
    if (engine == nullptr || ! engine->hasSong())     return "record.problem.noSong";
    if (! s.input.open)                               return "record.problem.noInput";
    bool armed = false;
    for (auto& t : s.trackUi) armed = armed || t.armed;
    if (! armed)                                      return "record.problem.noArm";
    if (s.conforming)                                 return "record.problem.conforming";
    if (s.latencyMeasuring)                           return "record.problem.measuring";
    if (! s.output.open || s.output.converting)       return "record.problem.sampleRate";
    return {};
}

void UiSession::finishRecording()
{
    if (engine == nullptr || ! engine->isRecording())
    {
        shadowActive = false;
        return;
    }

    // REC にならなかった裏録り（B7）は消す
    if (shadowActive)
    {
        shadowActive = false;
        engine->stopRecording().file.deleteFile();
        return;
    }

    const auto res = engine->stopRecording();
    syncLoopToEngine();   // ループを元に戻す
    juce::ignoreUnused (loopBeforeRecording);

    const auto type = s.recordingTrack;
    const auto id = s.recordingTake;
    auto* track = const_cast<project::Track*> (s.project.findTrack (type));
    if (track == nullptr || res.length <= 0)
    {
        res.file.deleteFile();   // 何も録れていない（0 サンプルの空ファイル）
        return;
    }
    if (res.dropped)
    {
        postNotice (tr ("record.dropped"));
        return;                  // ファイルは残す（消さない）が、採用しない
    }

    // 裏録りから昇格したテイク（B7）は、テイクの名前に付け直す
    const auto target = s.projectFolder.getChildFile (s.recordingPath);
    auto path = s.recordingPath;
    if (res.file != target)
    {
        // 同じ名前があれば空いている名前へ（moveFileTo は先のファイルを消してしまう。録った声は上書きしない）。
        // 移せなければ元の名前のまま使う（その時は .retro- の掃除から外れないので、知らせる）
        const auto dest = target.exists() ? target.getNonexistentSibling (false) : target;
        if (dest.getParentDirectory().createDirectory().wasOk() && res.file.moveFileTo (dest))
            path = dest.getRelativePathFrom (s.projectFolder).replaceCharacter ('\\', '/');
        else
        {
            path = res.file.getRelativePathFrom (s.projectFolder).replaceCharacter ('\\', '/');
            postNotice (tr ("record.renameFailed", path));
        }
    }

    project::Take take;
    take.id = id;
    take.path = path;
    take.startSample = res.startSample - s.recordingLatency;   // 歌い手が聞いた伴奏の位置にそろえる（ファイルは切らない）
    take.endSample = take.startSample + res.length;
    take.created = juce::Time::getCurrentTime();
    take.clip = res.clipped;
    take.peak = res.peak;
    take.recMode = s.recMode;
    take.latencySamples = s.recordingLatency;

    // 練習録音は納品の採用区間に入れない（DESIGN 6.1 / 13）
    // 採用は REC を押した所から（遡及録音ならフレーズの頭。B7）。それより前の分もファイルには残す
    if (take.recMode == project::RecMode::delivery)
        project::applyTake (*track, take, s.recordStart);
    else
        track->takes.push_back (take);

    loadTakeWave (type, take);

    const auto name = [type]
    {
        switch (type)
        {
            case project::TrackType::doubleTrack: return tr ("track.double");
            case project::TrackType::harm1:       return tr ("track.harm1");
            case project::TrackType::harm2:       return tr ("track.harm2");
            case project::TrackType::main:
            case project::TrackType::backing:
            case project::TrackType::guide:       break;
        }
        return tr ("track.main");
    }();
    const auto range = formatTime (std::max ({ (int64) 0, take.startSample, s.recordStart }), s.sampleRate(), true) + " - "
                     + formatTime (juce::jmin (s.project.lengthSamples, take.endSample), s.sampleRate(), true);
    if (take.recMode == project::RecMode::practice) postNotice (tr ("record.donePractice", name, id, range));
    else if (take.clip)                             postNotice (tr ("record.doneClip", name, id, range));
    else                                            postNotice (tr ("record.done", name, id, range));
    notify (change::takes | change::tracks);
}

void UiSession::loadTakeWave (project::TrackType type, const project::Take& take)
{
    // 録ったファイルを裏で読んで概形を作る（波形レーンに出す）。読み終わる前に UiSession が消えても安全に
    const auto file = s.projectFolder.getChildFile (take.path);
    const auto key = dummy::takeWaveKey (type, take.id);
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, file, key]
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
            return;
        auto wave = std::make_shared<audio::WaveformOverview> ((int64) reader->lengthInSamples);
        juce::AudioBuffer<float> buf (1, 65536);
        for (int64 pos = 0; pos < (int64) reader->lengthInSamples; pos += buf.getNumSamples())
        {
            const auto n = (int) juce::jmin<int64> (buf.getNumSamples(), (int64) reader->lengthInSamples - pos);
            reader->read (&buf, 0, n, pos, true, false);
            const float* ch[] = { buf.getReadPointer (0) };
            wave->append (ch, 1, n);
        }
        juce::MessageManager::callAsync ([this, weak, key, wave]
        {
            if (weak.expired())
                return;
            s.takeWaves[key] = wave;
            notify (change::takes);
        });
    });
}

//==============================================================================
void UiSession::updateShadow()
{
    if (! isEngineDriven() || s.isRecording)
        return;

    // 裏で録るのは、再生中で、REC を押せば録れる時だけ（アーム・入力・SR がそろっている）
    const bool want = s.isPlaying && recordProblem().isEmpty();
    if (shadowActive && (! want || ! engine->isRecording() || engine->recordingEnded()))
        finishRecording();   // ループで戻った・止まった：消す（戻った時は次で録り直す）

    if (shadowActive || ! want || engine->isRecording())
        return;

    const auto ld = latencyDisplay (s);
    const auto latency = ld.known ? juce::jmax ((int64) 0, ld.samples) : 0;
    shadowFile = s.projectFolder.getChildFile ("Audio/Takes/.retro-" + juce::String (++shadowSerial) + "-"
                                               + juce::String (juce::Time::currentTimeMillis()) + ".wav");
    if (engine->startRecording (shadowFile, s.recordFloat, latency).isEmpty())
    {
        shadowActive = true;
        s.recordingLatency = latency;
    }
}

int64 UiSession::retroStart (int64 press) const
{
    std::vector<float> env;
    const auto hop = engine->recordingEnvelope (env);
    const auto first = engine->recordingStartSample();
    if (hop <= 0 || first < 0)
        return press;

    // ファイルの i サンプル目 = 曲の (first - 遅れ + i)。押した所の声はファイルの press - (first - 遅れ)
    const auto fileStart = first - s.recordingLatency;
    const auto pressFrame = (int) juce::jmax ((int64) 0, (press - fileStart) / hop);
    const auto frame = audio::retro::phraseStartFrame (env.data(), (int) env.size(), pressFrame);
    if (frame >= pressFrame)
        return press;   // 遡らない（まだ歌っていない・フレーズの頭が分からない）
    return juce::jlimit (juce::jmax ((int64) 0, fileStart), press, fileStart + (int64) frame * hop);
}

//==============================================================================
void UiSession::measureLatency()
{
    if (engine == nullptr || s.latencyMeasuring || s.isRecording)
        return;
    if (! s.input.open || ! s.output.open)
    {
        postNotice (tr ("latency.problem.noDevice"));
        return;
    }

    // 測定音は出力を占有する：再生・録音は止める
    setPlaying (false);
    if (const auto error = engine->startLatencyProbe(); error.isNotEmpty())
    {
        postNotice (tr ("latency.problem.failed", error));
        return;
    }
    s.latencyMeasuring = true;
    s.latencyHasResult = false;
    latencyStartMs = juce::Time::getMillisecondCounter();
    notify (change::latency | change::transport);
}

void UiSession::cancelLatencyMeasure()
{
    if (! s.latencyMeasuring || analysingLatency)
        return;
    if (engine != nullptr)
        engine->cancelLatencyProbe();
    s.latencyMeasuring = false;
    notify (change::latency);
}

void UiSession::pollLatencyProbe()
{
    if (! s.latencyMeasuring || engine == nullptr || analysingLatency)
        return;

    if (! engine->latencyProbeFinished())
    {
        // 機器が止まって測定音が進まない：8 秒でやめて失敗として出す
        if (juce::Time::getMillisecondCounter() - latencyStartMs > 8000)
        {
            engine->cancelLatencyProbe();
            latencyMeasured ({}, {});
        }
        return;
    }

    // 録り終えた：裏で解析して、戻ってきたらこの機器の組み合わせに覚える
    audio::latency::Plan plan;
    auto captured = std::make_shared<std::vector<float>> (engine->latencyProbeCapture (plan));
    const auto key = latencyProfileKey (s);
    std::weak_ptr<bool> weak = alive;
    analysingLatency = true;
    juce::Thread::launch ([this, weak, captured, plan, key]
    {
        const auto r = audio::latency::analyse (captured->data(), (int64) captured->size(), plan);
        juce::MessageManager::callAsync ([this, weak, r, key]
        {
            if (! weak.expired())
                latencyMeasured (r, key);
        });
    });
}

void UiSession::latencyMeasured (const audio::latency::Result& r, const juce::String& key)
{
    analysingLatency = false;
    s.latencyMeasuring = false;
    s.latencyResult = r;
    s.latencyHasResult = true;
    if (r.ok() && key.isNotEmpty())
    {
        auto& p = s.latencyProfiles[key];
        p.measured = r.samples;
        p.manualMs = -1.0;   // 測れたら手入力より実測を使う
    }
    notify (change::latency);
}

void UiSession::setLatencyManualMs (double ms)
{
    if (! s.input.open)
        return;
    s.latencyProfiles[latencyProfileKey (s)].manualMs = juce::jlimit (0.0, 1000.0, ms);
    notify (change::latency);
}

void UiSession::clearLatencyManual()
{
    const auto it = s.latencyProfiles.find (latencyProfileKey (s));
    if (it == s.latencyProfiles.end() || it->second.manualMs < 0.0)
        return;
    it->second.manualMs = -1.0;
    notify (change::latency);
}

void UiSession::restoreLatencyProfiles (const juce::String& json)
{
    s.latencyProfiles.clear();
    const auto v = juce::JSON::parse (json);
    if (auto* obj = v.getDynamicObject())
        for (auto& prop : obj->getProperties())
        {
            dummy::Session::LatencyProfile p;
            p.measured = prop.value.hasProperty ("measured") ? (int64) prop.value["measured"] : -1;
            p.manualMs = prop.value.hasProperty ("manualMs") ? (double) prop.value["manualMs"] : -1.0;
            if (p.measured >= 0 || p.manualMs >= 0.0)
                s.latencyProfiles[prop.name.toString()] = p;
        }
    notify (change::latency);
}

juce::String UiSession::latencyProfilesJson() const
{
    auto* root = new juce::DynamicObject();
    for (auto& [key, p] : s.latencyProfiles)
    {
        auto* o = new juce::DynamicObject();
        if (p.measured >= 0)   o->setProperty ("measured", p.measured);
        if (p.manualMs >= 0.0) o->setProperty ("manualMs", p.manualMs);
        root->setProperty (key, juce::var (o));
    }
    return juce::JSON::toString (juce::var (root), true);
}

void UiSession::postNotice (const juce::String& text)
{
    s.noticeText = text;
    ++s.noticeSerial;
    notify (change::notice);
}

juce::File UiSession::projectFolderFor (const juce::String& songName)
{
    const auto name = juce::File::createLegalFileName (songName.isNotEmpty() ? songName : juce::String ("Untitled"));
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("VoiceBooth").getChildFile ("Projects").getChildFile (name);
}

void UiSession::exportTracks (const std::vector<project::TrackType>& types, int bitDepth)
{
    if (s.exporting || s.project.lengthSamples <= 0 || s.projectFolder == juce::File() || types.empty())
        return;

    // 裏のスレッドで書く（曲の長さぶん読む・書くので、画面を止めない）。プロジェクトは値で渡す
    auto project = s.project;
    if (bitDepth == 16 || bitDepth == 24 || bitDepth == 32)
        project.bitDepthExport = bitDepth;   // この書き出しだけ（録音形式は変えない）
    const auto folder = s.projectFolder;
    const auto dest = folder.getChildFile ("export_" + juce::Time::getCurrentTime().formatted ("%Y%m%d"));
    const auto song = s.songName;
    s.exporting = true;
    notify (change::takes);

    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, project, folder, dest, song, types]
    {
        juce::StringArray failed;
        int written = 0;
        for (auto t : types)
        {
            const auto res = exporter::ExportService::exportTrackDry (project, t, folder,
                                                                      dest.getChildFile (exporter::ExportService::dryFileName (song, t)));
            if (res.ok) ++written;
            else        failed.add (exporter::ExportService::dryFileName (song, t) + " (" + res.message + ")");
        }
        juce::MessageManager::callAsync ([this, weak, dest, failed, written]
        {
            if (weak.expired())
                return;
            s.exporting = false;
            if (failed.isEmpty()) postNotice (tr ("export.done", written, dest.getFullPathName()));
            else                  postNotice (tr ("export.failed", failed.joinIntoString (", ")));
            notify (change::takes);
        });
    });
}

void UiSession::stop()
{
    finishRecording();
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
    if (s.lyricSyncing) s.lyricCursor = song::firstLineToSync (s.project.lyrics, s.playhead);   // 戻って合わせ直す（B4b）
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

    pollLatencyProbe();
    updateShadow();

    if (! s.isPlaying) return;

    // 曲を開いていれば、位置は鳴っている音（エンジン）から取る
    if (isEngineDriven())
    {
        const auto pos = juce::jlimit ((int64) 0, s.project.lengthSamples, engine->getPlayheadSample());
        if (s.isRecording && s.loopOn && s.hasRange() && pos < s.playhead && s.playhead >= s.rangeIn)
            s.recordStart = s.rangeIn;   // ループで戻った

        const bool ended = engine->consumeReachedEnd() || ! engine->isPlaying();
        s.playhead = pos;
        followPlayhead (seconds);

        // 曲の終わりの後も、遅れて届く歌の終わり（補正量の分）を録り足してから閉じる。機器が止まっても 1.5 秒で閉じる
        if (ended && s.isRecording && engine->isRecording() && ! engine->recordingEnded())
        {
            const auto now = juce::Time::getMillisecondCounter();
            if (tailWaitStart == 0)
                tailWaitStart = now;
            if (now - tailWaitStart < 1500)
            {
                notify (change::playhead);
                return;
            }
        }
        tailWaitStart = 0;

        if (ended || (s.isRecording && engine->recordingEnded()))
        {
            finishRecording();   // 曲の終わり・デバイスが止まった：そこまでをテイクにする
            if (ended) s.isPlaying = false;
            s.isRecording = false;
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
    followPlayhead (seconds);
    notify (change::playhead);
}

void UiSession::followPlayhead (double seconds)
{
    // 再生中：右から 7 割を越えたら画面がなめらかに先回りする（ページ送りしない。DESIGN 4.10.1 PH）
    const auto len = s.viewEnd - s.viewStart;
    const auto start = motion::playhead::follow (s.viewStart, len, s.playhead, s.project.lengthSamples,
                                                 seconds, motion::prefersReducedMotion());
    if (start == s.viewStart)
        return;
    s.viewStart = start;
    s.viewEnd = start + len;
    notify (change::view);
}

void UiSession::keepPlayheadInView()
{
    const auto len = s.viewEnd - s.viewStart;
    if (s.playhead >= s.viewStart && s.playhead <= s.viewStart + len * 85 / 100)
        return;

    // シークで画面の外へ飛んだ：再生ヘッドが 1/4 の位置に来るように合わせる（再生中の追従は followPlayhead）
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

juce::String latencyProfileKey (const dummy::Session& s)
{
    const auto& in = s.input;
    return in.typeName + "|" + in.deviceName + "|" + s.output.deviceName + "|"
         + juce::String (juce::roundToInt (in.sampleRate)) + "|" + juce::String (in.bufferSize);
}

LatencyDisplay latencyDisplay (const dummy::Session& s)
{
    LatencyDisplay d;
    if (! s.engineAttached)
    {
        // UI_MOCK：ダミーの実測値
        d.known = true;
        d.measured = true;
        d.samples = s.latencySamples;
        d.ms = (double) s.latencySamples * 1000.0 / s.sampleRate();
        return d;
    }
    if (! s.input.open || s.input.sampleRate <= 0.0)
        return d;

    const auto rate = s.input.sampleRate;
    d.known = true;
    if (const auto it = s.latencyProfiles.find (latencyProfileKey (s)); it != s.latencyProfiles.end())
    {
        if (it->second.manualMs >= 0.0)
        {
            d.manual = true;
            d.samples = (int64) std::llround (it->second.manualMs * 0.001 * rate);
            d.ms = it->second.manualMs;
            return d;
        }
        if (it->second.measured >= 0)
        {
            d.measured = true;
            d.samples = it->second.measured;
            d.ms = (double) d.samples * 1000.0 / rate;
            return d;
        }
    }

    const auto r = audio::reportedLatency (s.input.inputLatency, s.input.outputLatency, s.input.bufferSize);
    d.reported = true;
    d.estimated = r.estimated;
    d.samples = r.samples;
    d.ms = (double) r.samples * 1000.0 / rate;
    return d;
}
} // namespace vb
