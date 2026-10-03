#include "UiSession.h"
#include "audio/Retro.h"
#include "audio/SongLoader.h"
#include "audio/Resample.h"
#include "analysis/RefPitch.h"
#include "analysis/MusicInfo.h"
#include "analysis/Separation.h"
#include "analysis/OffVocal.h"
#include "analysis/TakeStats.h"
#include "analysis/Rmvpe.h"
#include "Animator.h"
#include "audio/DeviceRules.h"
#include "audio/InputMeter.h"
#include "project/Comp.h"
#include "audio/PitchShift.h"
#include "export/ExportService.h"
#include "export/DeliveryPack.h"
#include "SongMarks.h"
#include "WaveLane.h"
#include "audio/PlaybackCore.h"
#include "Timeline.h"
#include "audio/Resample.h"

namespace vb
{
UiSession::UiSession() : s (dummy::makeSession()) {}

void UiSession::notify (juce::uint32 changes)
{
    // プロジェクトに入るものが変わったら、少し待ってから自動保存（B14）
    if ((changes & (change::takes | change::songInfo | change::recordFormat | change::tracks | change::practice)) != 0 && changes != change::all)
        markDirty();
    // 録ったトラックの再生（B12）：採用区間が変わったら作り直し、音量・M / S・録音中は今すぐ
    if (isEngineDriven())
    {
        if ((changes & change::takes) != 0)
            stemsDirty = true;
        if ((changes & (change::tracks | change::transport | change::practice)) != 0)
            syncStemGains();
    }
    // クリックの拍は曲のテンポ・拍子・1 小節目から（直したらすぐ鳴る位置も変わる。2026-10-02）
    if ((changes & (change::songInfo | change::song | change::transport | change::monitor)) != 0)
        syncClickToEngine();
    listeners.call ([changes] (Listener& l) { l.sessionChanged (changes); });
}

//==============================================================================
void UiSession::loadSong (const juce::File& file, int sampleRate, int64 lengthSamples,
                          std::shared_ptr<const audio::WaveformOverview> wave,
                          std::shared_ptr<const audio::SongAudio> audio)
{
    finishRecording();   // 録音中に別の曲を開いたら、そこまでのテイクは残す
    stopSeparation();    // 前の曲の分離は止める（B16）
    endTakeCompare (false);   // テイク比較の試聴中なら元の採用区間に戻す（B18c。範囲・ループも元へ）
    flushSave();         // 前の曲のプロジェクトを保存してから
    s = dummy::makeSongSession (s, file.getFileNameWithoutExtension(), file.getFullPathName(),
                                sampleRate, lengthSamples, std::move (wave));
    s.songRate = sampleRate;
    lastBackupMs = 0;

    // プロジェクトフォルダ（B14）：.vbooth から開いたらそのフォルダ。曲ファイルから開いたら Projects/{曲名}/。
    // 同じ名前のプロジェクトがあり、同じ曲（ファイル名と長さ）なら続きから開く。違う曲なら「{曲名} (2)」…の別のフォルダ
    if (pendingProject != nullptr)
    {
        s.projectFile = pendingProjectFile;
        s.projectFolder = pendingProjectFile.getParentDirectory();
    }
    else
    {
        auto folder = projectFolderFor (s.songName);
        auto sameSong = [&] (const juce::File& vbooth)
        {
            auto l = project::fromJson (vbooth.loadFileAsString());
            if (! l.ok || l.project.sampleRate <= 0)
                return false;
            const auto len = (double) l.project.lengthSamples * sampleRate / l.project.sampleRate;
            if (juce::File (l.project.songPath).getFileName() != file.getFileName() || std::abs (len - (double) lengthSamples) > 2.0)
                return false;
            pendingProject = std::make_unique<project::LoadedProject> (std::move (l));
            return true;
        };
        for (int n = 2; ; ++n)
        {
            const auto f = folder.getChildFile (folder.getFileName() + project::fileExtension);
            if (! f.exists() || sameSong (f))
                break;
            folder = projectFolderFor (s.songName + " (" + juce::String (n) + ")");
        }
        s.projectFolder = folder;
        s.projectFile = folder.getChildFile (folder.getFileName() + project::fileExtension);
    }

    // 曲はプロジェクトの中にコピーして持つ（持ち運べるように。元のファイルはそのまま）
    if (file.isAChildOf (s.projectFolder))
        s.project.songPath = file.getRelativePathFrom (s.projectFolder).replaceCharacter ('\\', '/');
    else
    {
        s.project.songPath = "Audio/" + file.getFileName();
        copyIntoProject (file, s.project.songPath);
    }
    // 前に落ちた時の裏録りの残り（B7。REC にならなかった分）を消す
    for (auto& f : s.projectFolder.getChildFile ("Audio/Takes").findChildFiles (juce::File::findFiles, false, ".retro-*.wav"))
        f.deleteFile();
    s.songOriginal = audio;
    s.songCurrent = audio;

    if (engine != nullptr)
    {
        engine->setSong (std::move (audio));   // 止まって頭へ。出力の SR を曲に合わせる（録ったトラックも外れる）
        for (auto& sig : stemSignature) sig.clear();
        stemsDirty = true;
        syncBackingLevel();
        syncGuideToEngine();
        syncLoopToEngine();
        syncPracticeToEngine();
        refreshOutputStatus();
    }
    notify (change::all);

    // 続きから開くプロジェクトの録音形式（時間軸の SR を前と同じにする）
    if (pendingProject != nullptr)
    {
        const auto& ex = pendingProject->extras;
        s.recordRate = ex.recordRate;
        s.recordFloat = ex.recordFloat;
        s.deviceFallbackRate = ex.deviceFallbackRate;
        s.project.bitDepthExport = ex.recordFloat ? 32 : 24;
        notify (change::recordFormat);
    }

    // 録音の SR を曲と違う値にしていれば、伴奏をその SR にそろえる（裏で）。そろえ終わってからプロジェクトの中身を戻す
    if (s.targetRate() != s.songRate)
        conformSong();
    else
        checkDeviceRate();
    if (! s.conforming)
        restoreProject();

    estimateSongInfo();
    markDirty();   // 新しい曲はすぐに .vbooth を作る
}

//==============================================================================
void UiSession::markDirty()
{
    if (! isEngineDriven() || restoring)
        return;
    dirty = true;
    dirtySince = juce::Time::getMillisecondCounter();
}

void UiSession::setPendingProject (const juce::File& vboothFile, const project::LoadedProject& p)
{
    pendingProject = std::make_unique<project::LoadedProject> (p);
    pendingProjectFile = vboothFile;
}

void UiSession::restoreProject()
{
    if (pendingProject == nullptr)
        return;
    const auto loaded = std::move (pendingProject);
    pendingProjectFile = juce::File();
    const auto& lp = loaded->project;
    restoring = true;

    // テイク・採用区間（ファイルはプロジェクトフォルダ相対）
    int takes = 0;
    for (auto& t : lp.tracks)
    {
        auto* mine = const_cast<project::Track*> (s.project.findTrack (t.type));
        if (mine == nullptr)
        {
            s.project.tracks.push_back (t);
            mine = &s.project.tracks.back();
        }
        mine->takes = t.takes;
        mine->comp = t.comp;
        for (auto& k : t.takes)
        {
            ++takes;
            if (s.projectFolder.getChildFile (k.path).existsAsFile())
                loadTakeWave (t.type, k);
        }
    }

    // 曲の情報（推定のままの値も戻す。あとから届く自動推定は確定した値を上書きしない）
    if (lp.tempo.known()) s.project.tempo = lp.tempo;
    if (lp.key.known())   s.project.key = lp.key;
    s.project.sections = lp.sections;
    s.project.lyrics = lp.lyrics;
    song::updateLineEnds (s.project.lyrics, s.project.lengthSamples, s.sampleRate());

    // 録ったトラックの音量・M・S と練習のテンポ・キー（古いファイルには無い：既定のまま）
    for (auto& m : loaded->extras.trackMix)
        for (auto& tu : s.trackUi)
            if (tu.type == m.type)
            {
                tu.monitorGain = m.gain;
                tu.mute = m.mute;
                tu.solo = m.solo;
            }
    setPractice (loaded->extras.practiceTempo, loaded->extras.practiceKey);

    if (lp.sampleRate != s.sampleRate())
        postNotice (tr ("project.rateChanged", formatKhz (lp.sampleRate), formatKhz (s.sampleRate())));
    else if (takes > 0)
        postNotice (tr ("project.resumed", takes));

    restoring = false;
    notify (change::takes | change::tracks | change::songInfo | change::view);

    // お手本（声入りの原曲）はもう一度合わせ直す（数秒〜。結果は知らせで）
    s.guideNudgeMs = loaded->extras.guideNudgeMs;   // お手本の位置の手直し（合わせ直した後に当てる）
    if (loaded->extras.guidePath.isNotEmpty())
    {
        const auto guide = s.projectFolder.getChildFile (loaded->extras.guidePath);
        if (guide.existsAsFile())
            loadGuide (guide);
    }
}

void UiSession::copyIntoProject (const juce::File& source, const juce::String& relativePath)
{
    // 裏でコピー（数十 MB）。同じ名前・同じ大きさのものが既にあれば何もしない
    const auto dest = s.projectFolder.getChildFile (relativePath);
    if (dest.existsAsFile() && dest.getSize() == source.getSize())
        return;
    juce::Thread::launch ([source, dest]
    {
        dest.getParentDirectory().createDirectory();
        const auto temp = dest.getSiblingFile (dest.getFileName() + ".part");
        if (source.copyFileTo (temp))
            temp.moveFileTo (dest);
        else
            temp.deleteFile();
    });
}

void UiSession::saveProject()
{
    if (! isEngineDriven() || s.projectFile == juce::File() || s.songOriginal == nullptr || restoring)
        return;

    project::ProjectExtras ex;
    ex.guidePath = s.guidePath;
    ex.guideNudgeMs = s.guideNudgeMs;
    ex.recordRate = s.recordRate;
    ex.recordFloat = s.recordFloat;
    ex.deviceFallbackRate = s.deviceFallbackRate;
    for (auto& tu : s.trackUi)   // 録ったトラックの音量・M・S（B12）と練習のテンポ・キー（B11）も覚える
        ex.trackMix.push_back ({ tu.type, tu.monitorGain, tu.mute, tu.solo });
    ex.practiceTempo = s.tempoPercent;
    ex.practiceKey = s.keyShift;

    // 世代バックアップ：開いてから最初の保存と、その後 10 分ごとに、前の .vbooth を Backups/ へ（新しい 10 個を残す）
    const auto now = juce::Time::getMillisecondCounter();
    if (s.projectFile.existsAsFile() && (lastBackupMs == 0 || now - lastBackupMs > 10 * 60 * 1000))
    {
        const auto dir = s.projectFolder.getChildFile ("Backups");
        dir.createDirectory();
        s.projectFile.copyFileTo (dir.getChildFile (s.projectFile.getFileNameWithoutExtension() + "-"
                                                   + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + project::fileExtension));
        auto old = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + project::fileExtension);
        std::sort (old.begin(), old.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName() > b.getFileName(); });
        for (int i = 10; i < old.size(); ++i)
            old.getReference (i).deleteFile();
        lastBackupMs = now;
    }

    // テイク比較の試聴中（B18c）は、選んでいない差し替えを書かない（確定している採用区間で保存する）
    auto committed = s.project;
    audition.restoreCommitted (committed);
    if (! project::writeAtomically (s.projectFile, project::toJson (committed, ex)))
    {
        postNotice (tr ("project.saveFailed", s.projectFile.getFullPathName()));
        dirty = false;   // 何度も出さない（次の変更でまた試す）
        return;
    }
    dirty = false;

    // 最近のプロジェクト（新しい順、8 件まで）
    const auto path = s.projectFile.getFullPathName();
    if (s.recentProjects[0] != path)
    {
        s.recentProjects.removeString (path);
        s.recentProjects.insert (0, path);
        while (s.recentProjects.size() > 8)
            s.recentProjects.remove (s.recentProjects.size() - 1);
        notify (change::project);
    }
}

void UiSession::flushSave()
{
    if (dirty)
        saveProject();
}

void UiSession::restoreRecentProjects (const juce::StringArray& list)
{
    s.recentProjects.clear();
    for (auto& p : list)
        if (p.isNotEmpty() && juce::File (p).existsAsFile() && ! s.recentProjects.contains (p))
            s.recentProjects.add (p);
    notify (change::project);
}

void UiSession::estimateSongInfo()
{
    // テンポ・1 小節目・キーを裏で推定する（B9b）。結果は「推定」。手で入れた（確定の）値は上書きしない
    if (s.songOriginal == nullptr)
        return;
    const auto audio = s.songOriginal;
    const auto serial = s.songSerial;
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, audio, serial]
    {
        const auto n = audio->buffer.getNumSamples(), ch = juce::jmax (1, audio->buffer.getNumChannels());
        std::vector<float> m ((size_t) n, 0.0f);
        for (int c = 0; c < audio->buffer.getNumChannels(); ++c)
        {
            const auto* x = audio->buffer.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                m[(size_t) i] += x[i] / (float) ch;
        }
        const auto tempo = analysis::estimateTempo (m.data(), (int64) n, audio->sampleRate);
        const auto key = analysis::estimateKey (m.data(), (int64) n, audio->sampleRate);
        juce::MessageManager::callAsync ([this, weak, serial, tempo, key, songRate = audio->sampleRate]
        {
            if (weak.expired() || serial != s.songSerial)
                return;
            bool changed = false;
            auto& t = s.project.tempo;
            if (! t.confirmed() && tempo.bpm > 0.0)
            {
                // 曲の元の SR → いまの時間軸の SR（録音形式で SR をそろえていれば）
                const auto ratio = (double) s.sampleRate() / songRate;
                t.bpm = tempo.bpm;
                t.signature = { 4, 4 };
                t.downbeatSample = (int64) std::llround ((double) tempo.downbeatSample * ratio);
                t.source = song::Source::estimated;
                t.confidence = tempo.confidence;
                t.beats.clear();
                changed = true;
            }
            auto& k = s.project.key;
            if (k.source != song::Source::confirmed && key.tonic >= 0)
            {
                k.tonic = key.tonic;
                k.minor = key.minor;
                k.source = song::Source::estimated;
                k.confidence = key.confidence;
                changed = true;
            }
            if (changed)
                songInfoChanged();
        });
    });
}

void UiSession::checkDeviceRate()
{
    // 「曲に合わせる」なのに機器が曲の SR で開けない（48 kHz 固定の機器・Windows の共有モードなど）：
    // REC で止めずに、機器の SR で録る（伴奏をその SR にそろえ、知らせる。元のファイルはそのまま。2026-10-02 決定）
    if (! isEngineDriven() || s.songOriginal == nullptr || s.conforming || s.isRecording || s.recordRate > 0.0)
        return;
    if (! s.output.open || ! s.output.converting || hasTakes())
        return;   // 開いていない・そのまま鳴らせる・テイクがある（時間軸を変えない）
    const auto deviceRate = juce::roundToInt (s.output.sampleRate);
    if (deviceRate <= 0 || deviceRate == s.sampleRate())
        return;
    s.deviceFallbackRate = deviceRate;
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
            for (auto& p : s.myPitch) p.sample = scale (p.sample);
            for (auto& p : s.refPitch) p.sample = scale (p.sample);
            for (auto& l : s.project.lyrics.lines) { l.startSample = scaleTimed (l.startSample); l.endSample = scaleTimed (l.endSample); }
            s.project.sampleRate = target;
            s.project.lengthSamples = audio->length();
            s.backingWave = wave;
            s.songCurrent = audio;

            if (engine != nullptr)
            {
                engine->setSong (audio);   // デバイスもこの SR に切り替える（対応していれば）
                for (auto& sig : stemSignature) sig.clear();
                stemsDirty = true;
                syncBackingLevel();
                syncGuideToEngine();   // お手本の声もこの SR にそろえ直す
                engine->seek (s.playhead);
                syncLoopToEngine();
                syncPracticeToEngine();
                refreshOutputStatus();
            }
            if (s.recordRate <= 0.0 && s.deviceFallbackRate == target)
                postNotice (tr ("format.deviceFallback", formatKhz (s.songRate), formatKhz (target)));
            else
                postNotice (tr ("format.conformed", formatKhz (target), formatBits (s.project.bitDepthExport)));
            restoreProject();   // 続きから開くプロジェクト（B14）は、時間軸の SR がそろってから戻す
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
        s.backingMeterDb = s.guideMeterDb = s.clickMeterDb = s.harmGuideMeterDb = audio::LevelFollower::floorDb;
    }
    sentClickSpb = -1.0;
    syncClickToEngine();
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
    pollMonitorLevels();
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

void UiSession::pollMonitorLevels()
{
    // モニターの帯のメーター（オフボ・お手本・クリックのフェーダー後。2026-10-02）。帯の目盛りは -48 dB までなので、
    // それより下は -60 にまとめる（消えていく間に毎回描き直さない）
    if (engine == nullptr)
        return;
    const auto l = engine->getMonitorLevels();
    bool changed = false;
    auto take = [&changed] (float& v, float db)
    {
        db = juce::jmax (-60.0f, db);
        if (std::abs (db - v) >= 0.05f)
        {
            v = db;
            changed = true;
        }
    };
    take (s.backingMeterDb, l.backingDb);
    take (s.guideMeterDb, l.guideDb);
    take (s.harmGuideMeterDb, l.harmGuideDb);
    take (s.clickMeterDb, l.clickDb);
    if (changed)
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
    checkDeviceRate();
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
    checkDeviceRate();
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

void UiSession::syncPracticeToEngine()
{
    if (engine != nullptr)
        engine->setPractice (s.tempoPercent / 100.0, s.keyShift);
}

bool UiSession::practiceShifted() const
{
    return s.tempoPercent != 100 || s.keyShift != 0;
}

void UiSession::setBackingLevel (float fader)
{
    s.offVocalGain = juce::jlimit (0.0f, 1.0f, fader);
    syncBackingLevel();
    notify (change::monitor);
}

void UiSession::setBackingMuted (bool m)
{
    s.backingMuted = m;
    syncBackingLevel();
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

void UiSession::discardRecording()
{
    if (! s.isRecording) return;
    const juce::ScopedValueSetter<bool> discard (discarding, true);
    setRecording (false);
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

    // 納品録音は原速・原キー（DESIGN 6.1。画面では確認してから来る。念のためここでも戻す）
    if (s.recMode == project::RecMode::delivery && practiceShifted())
        setPractice (100, 0);

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

    // 区間の録り直し（パンチイン。B10）：範囲があれば、採用は範囲の中だけ。範囲の前を鳴らしている時はそのまま、
    // そうでなければ範囲の少し前（プリロール）から鳴らして録る
    // 簡単モードは通し録りだけ（DESIGN 2「区間録り直し＝通しのみ」）：IN/OUT は練習のループにだけ使い、録音は今の位置から通しで
    const bool punch = s.hasRange() && s.mode != project::Mode::easy;
    const auto now = juce::jlimit ((int64) 0, s.project.lengthSamples, engine->getPlayheadSample());
    if (punch && ! (s.isPlaying && shadowActive && now < s.rangeIn))
    {
        if (shadowActive)
            finishRecording();    // 裏録りは消す（範囲の前から録り直す）
        if (s.isPlaying)
            setPlaying (false);
        seek (juce::jmax ((int64) 0, s.rangeIn - prerollSamples()));
    }

    // 再生中で裏で録っていれば（B7）、それをこのテイクにする。押す前に歌い始めていれば、フレーズの頭から採る
    if (shadowActive && s.isPlaying && engine->isRecording() && engine->recordingStartSample() >= 0)
    {
        shadowActive = false;
        loopBeforeRecording = s.loopOn && s.hasRange();
        engine->setLoop (s.rangeIn, s.rangeOut, false);
        s.recordingTake = id;
        s.recordingTrack = armed->type;
        s.recordingPath = rel();
        s.recordingTempo = s.tempoPercent;   // 裏録りは原速・原キーの時だけ（updateShadow）
        s.recordingKey = s.keyShift;
        s.isRecording = true;
        s.recordStart = punch ? s.rangeIn : retroStart (now);
        s.recordEnd = punch ? s.rangeOut : -1;
        notify (change::transport | change::practice);
        return;
    }

    if (s.playhead >= s.project.lengthSamples)
        seek (0);

    // 録音中はループしない（通し録音。区間の録り直しは B10）
    loopBeforeRecording = s.loopOn && s.hasRange();
    engine->setLoop (s.rangeIn, s.rangeOut, false);
    const bool fromStop = ! s.isPlaying;   // 鳴っている途中から録る時は数えない（もう拍が聞こえている）

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
    s.recordingTempo = s.tempoPercent;
    s.recordingKey = s.keyShift;
    s.isRecording = true;
    s.isPlaying = true;
    s.recordStart = punch ? s.rangeIn : s.playhead;
    s.recordEnd = punch ? s.rangeOut : -1;
    if (fromStop)
        startWithCountIn (punch);
    else
        engine->play();
    notify (change::transport | change::practice);
}

void UiSession::startWithCountIn (bool punch)
{
    // カウントイン（2026-10-02）。歌い始める所（通しは今の位置、範囲の録り直しは範囲の頭）の小節の 1 拍目から COUNT 小節前で数え始め、
    // 歌い始める所で数え終わる（song::countInStart。拍は曲の目盛りと同じ所に鳴るので、そのまま曲の拍につながる）
    //   - 通し：曲は止めたまま、クリックだけで数える。数え終わった所から曲と録音が始まる（数えている間の声は録らない）
    //   - 範囲の録り直し：助走（prerollSamples）の曲を鳴らしながら、範囲の頭まで拍を鳴らす。曲の頭より前にはみ出す分は無音で数える
    // テンポが分からなければ数えられない：そのまま始めて、曲ごとに 1 度だけ理由を知らせる
    if (s.countInBars > 0 && ! s.tempoKnown())
    {
        if (countInWarnedSong != s.songSerial)
        {
            countInWarnedSong = s.songSerial;
            postNotice (tr ("record.countIn.noTempo"));
        }
        engine->play();
        return;
    }
    if (s.countInBars <= 0)
    {
        engine->play();
        return;
    }
    const auto target = punch ? s.rangeIn : s.playhead;
    const auto from = song::countInStart (s.project.tempo, target, s.countInBars, s.sampleRate());
    engine->playWithCountIn (juce::jmax ((int64) 0, s.playhead - from), target);
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

    if (discarding)
    {
        res.file.deleteFile();   // 破棄を選んだ：採用もテイクの一覧にも入れない
        s.recordEnd = -1;
        postNotice (tr ("record.discarded"));
        return;
    }

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
    // 歌い手が聞いた伴奏の位置にそろえる（ファイルは切らない）。練習でテンポを変えて録った時（B11）は、
    // ファイルの 1 サンプルが曲では速さの倍なので、遅れも長さも曲のサンプルに直す（採用区間には入らない。目安の位置）
    const auto speed = s.recordingTempo / 100.0;
    take.startSample = res.startSample - (int64) std::llround (s.recordingLatency * speed);
    take.endSample = take.startSample + (int64) std::llround (res.length * speed);
    take.tempoPercent = s.recordingTempo;
    take.keyShift = s.recordingKey;
    take.created = juce::Time::getCurrentTime();
    take.clip = res.clipped;
    take.peak = res.peak;
    take.recMode = s.recMode;
    take.latencySamples = s.recordingLatency;

    // 練習録音は納品の採用区間に入れない（DESIGN 6.1 / 13）
    // 採用は REC を押した所から（遡及録音ならフレーズの頭。B7）、区間の録り直しなら範囲の中だけ（B10）。
    // 前後の分もファイルには残す。直前の採用は覚えておき、Ctrl / ⌘+Z で戻せる
    if (take.recMode == project::RecMode::delivery)
    {
        compBeforeTake = track->comp;
        undoTrack = type;
        undoIsCompare = false;
        project::applyTake (*track, take, s.recordStart, s.recordEnd >= 0 ? s.recordEnd : take.endSample);
        s.canUndoTake = true;
    }
    else
    {
        // 本番で録っていたら採用した範囲を覚えておく（あとで「本番に入れる」時に同じにする）
        take.useFrom = s.recordStart;
        take.useTo = s.recordEnd >= 0 ? s.recordEnd : take.endSample;
        track->takes.push_back (take);
    }

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
    const auto usedEnd = s.recordEnd >= 0 ? juce::jmin (s.recordEnd, take.endSample) : take.endSample;
    const auto range = formatTime (std::max ({ (int64) 0, take.startSample, s.recordStart }), s.sampleRate(), true) + " - "
                     + formatTime (juce::jmin (s.project.lengthSamples, usedEnd), s.sampleRate(), true);
    const bool punched = s.recordEnd >= 0;
    s.recordEnd = -1;
    if (take.recMode == project::RecMode::practice)
    {
        // 原速・原キーで録ったリハーサルは、知らせに「本番に入れる」を付ける（本番のつもりで録っていた時の救済）。
        // 知らせを出すと画面がすぐ読むので、印は先に付ける（次の知らせの番号）
        if (take.tempoPercent == 100 && take.keyShift == 0)
        {
            s.rescueNoticeSerial = s.noticeSerial + 1;
            s.rescueTrack = type;
            s.rescueTakeId = id;
        }
        postNotice (tr ("record.donePractice", name, id, range));
    }
    else if (take.clip)                             postNotice (tr ("record.doneClip", name, id, range));
    else if (punched)                               postNotice (tr ("record.donePunch", name, id, range, undoKeyName()));
    else                                            postNotice (tr ("record.done", name, id, range));
    notify (change::takes | change::tracks);
}

void UiSession::loadTakeWave (project::TrackType type, const project::Take& take)
{
    // 録ったファイルを裏で読んで概形を作る（波形レーンに出す）。読み終わる前に UiSession が消えても安全に
    const auto file = s.projectFolder.getChildFile (take.path);
    const auto key = dummy::takeWaveKey (type, take.id);
    // 音程も取る（B18）：原速・原キーのテイクだけ（練習の速さで録った物は曲の時間に並ばない）
    const bool wantPitch = take.tempoPercent == 100 && take.keyShift == 0;
    const auto takeStart = take.startSample;
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, file, key, wantPitch, takeStart]
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
            return;
        auto wave = std::make_shared<audio::WaveformOverview> ((int64) reader->lengthInSamples);
        std::shared_ptr<std::vector<audio::PitchFrame>> pitch;
        audio::PitchAnalyzer analyzer;
        std::vector<int64> pos64;
        if (wantPitch)
        {
            pitch = std::make_shared<std::vector<audio::PitchFrame>>();
            analyzer.prepare (reader->sampleRate);
        }
        juce::AudioBuffer<float> buf (1, 65536);
        for (int64 pos = 0; pos < (int64) reader->lengthInSamples; pos += buf.getNumSamples())
        {
            const auto n = (int) juce::jmin<int64> (buf.getNumSamples(), (int64) reader->lengthInSamples - pos);
            reader->read (&buf, 0, n, pos, true, false);
            const float* ch[] = { buf.getReadPointer (0) };
            wave->append (ch, 1, n);
            if (pitch != nullptr)
            {
                pos64.resize ((size_t) n);
                for (int i = 0; i < n; ++i) pos64[(size_t) i] = takeStart + pos + i;   // 曲の位置（遅れは補正済み）
                analyzer.process (buf.getReadPointer (0), pos64.data(), n, *pitch);
            }
        }
        juce::MessageManager::callAsync ([this, weak, key, wave, pitch]
        {
            if (weak.expired())
                return;
            s.takeWaves[key] = wave;
            if (pitch != nullptr)
            {
                s.takePitch[key] = pitch;
                updateTakeStats (key);
            }
            notify (change::takes | change::view);
        });
    });
}

//==============================================================================
namespace
{
    /** お手本の解析の結果（裏のスレッド → メッセージスレッド） */
    /** 音を d サンプル（時間軸 timelineRate）ずらした写し。+ は後ろへ（頭に無音）、- は前へ（頭を捨てる）。長さは同じ */
    std::shared_ptr<const audio::SongAudio> shiftedAudio (const std::shared_ptr<const audio::SongAudio>& a, juce::int64 d, double timelineRate)
    {
        if (a == nullptr || d == 0 || timelineRate <= 0.0)
            return a;
        const auto n = (int) a->length();
        const auto da = (int) std::llround ((double) d * a->sampleRate / timelineRate);   // 声・原曲はオフボの元の SR で持っている
        auto r = std::make_shared<audio::SongAudio>();
        r->sampleRate = a->sampleRate;
        r->buffer.setSize (a->buffer.getNumChannels(), n);
        r->buffer.clear();
        const auto from = juce::jmax (0, -da), to = juce::jmax (0, da);
        const auto count = n - juce::jmax (from, to);
        for (int ch = 0; ch < a->buffer.getNumChannels() && count > 0; ++ch)
            r->buffer.copyFrom (ch, to, a->buffer, ch, from, count);
        return r;
    }

    struct GuideOutcome
    {
        enum class Kind { ok, loadFailed, notAligned, needsSeparation, keyShift, tempoDiffers };
        Kind kind = Kind::notAligned;
        juce::String error;                        // loadFailed の翻訳キー
        std::vector<audio::PitchFrame> points;     // オフボ（曲の SR）の時間
        double offsetSeconds = 0.0;                // 原曲の位置 = オフボの位置 + offset
        int keyShift = 0;
        std::shared_ptr<const audio::SongAudio> vocals;   // 取り出した声（オフボの時間・オフボの元の SR・モノラル）。聴く用
        // ハモリのお手本（2026-10-02）：分離の時、リードを取れていれば。vocals はリードだけになる
        std::vector<audio::PitchFrame> harmPoints;
        std::shared_ptr<const audio::SongAudio> harmVocals;
        std::shared_ptr<const audio::SongAudio> original;   // 時間を合わせた原曲（オフボの時間・モノラル）。聞き比べ用。速さが違えば無し
        bool alignRough = false;                    // 時間合わせの確かさが低い（「推定」）
        double tempoRatio = 1.0;                    // 原曲の速さ ÷ オフボの速さ（1 でなければ線は出せない）
        std::vector<std::pair<juce::int64, juce::int64>> covered;   // お手本が使える所（オフボの時間・オフボの元の SR）
    };

    /** 合わせの結果からお手本の使える所（曲のほぼ全部なら空） */
    std::vector<std::pair<juce::int64, juce::int64>> coveredSpans (const analysis::AlignResult& align, juce::int64 length, double rate)
    {
        std::vector<std::pair<juce::int64, juce::int64>> spans;
        juce::int64 total = 0;
        for (auto& c : align.covered)
        {
            const auto a = juce::jmax ((juce::int64) 0, c.karaokeStart), b = juce::jmin (length, c.karaokeEnd);
            if (b > a) { spans.push_back ({ a, b }); total += b - a; }
        }
        if (length - total < (juce::int64) (1.0 * rate))   // 抜けが 1 秒未満なら全部ある扱い
            spans.clear();
        return spans;
    }

    /** お手本の声（オフボの時間、モノラル）から RMVPE で音程を取り直す（分離プロセスの --pitch）。
        伴奏の残りに強く、オクターブの誤りがほぼ無い（Rmvpe.h）。モデルが無い・失敗したら false（YIN の線のまま） */
    bool pitchWithModel (const std::vector<float>& vocals, double rate, std::vector<audio::PitchFrame>& points,
                         const std::vector<float>* loudFrom = nullptr)
    {
        if (vocals.empty() || ! separation::SeparatorClient::pitchAvailable())
            return false;
        const auto wav = juce::File::createTempFile (".wav");
        {
            auto fs = std::make_unique<juce::FileOutputStream> (wav);
            if (! fs->openedOk())
                return false;
            std::unique_ptr<juce::OutputStream> stream (fs.release());
            juce::WavAudioFormat format;
            auto w = format.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (1)
                                                         .withBitsPerSample (32).withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
            const float* ch[] = { vocals.data() };
            if (w == nullptr || ! w->writeFromFloatArrays (ch, 1, (int) vocals.size()))
            {
                wav.deleteFile();
                return false;
            }
        }
        std::vector<std::pair<float, float>> raw;
        const bool ok = separation::SeparatorClient::runPitch (wav, raw);
        wav.deleteFile();
        if (! ok)
            return false;
        std::vector<analysis::rmvpe::Frame> frames;
        frames.reserve (raw.size());
        for (auto& [c, s] : raw)
            frames.push_back ({ c, s });
        points = analysis::rmvpe::toPitchFrames (frames, rate);
        analysis::rmvpe::gateQuiet (points, vocals.data(), (juce::int64) vocals.size(), rate, 35.0f,
                                    loudFrom != nullptr && loudFrom->size() == vocals.size() ? loudFrom->data() : nullptr);
        return true;
    }

    /** ハモリの点のうち、同じ時刻（±10 ms）のリードと同じ音（±60 セント）の物を無声にする。ハモリの線に出るリードの消し残り */
    void dropUnison (std::vector<audio::PitchFrame>& harm, const std::vector<audio::PitchFrame>& lead, double rate)
    {
        const auto tolerance = (juce::int64) std::llround (audio::pitch::hopSeconds * rate);
        size_t j = 0;
        for (auto& p : harm)
        {
            if (p.confidence < 0.5f)
                continue;
            while (j < lead.size() && lead[j].songSample < p.songSample - tolerance)
                ++j;
            for (auto k = j; k < lead.size() && lead[k].songSample <= p.songSample + tolerance; ++k)
                if (lead[k].confidence >= 0.5f && std::abs (lead[k].midi - p.midi) <= 0.6f)
                {
                    p.midi = 0.0f;
                    p.confidence = 0.0f;
                    break;
                }
        }
    }

    std::shared_ptr<const audio::SongAudio> vocalsAudio (const std::vector<float>& v, double rate)
    {
        if (v.empty()) return nullptr;
        auto a = std::make_shared<audio::SongAudio>();
        a->sampleRate = rate;
        a->buffer.setSize (1, (int) v.size());
        a->buffer.copyFrom (0, 0, v.data(), (int) v.size());
        return a;
    }

    /** リードとハモリに分ける（どちらもオフボの時間・モノラル。2026-10-03）。ハモリ = 声 − リード。
        お手本の線と声はリードだけに、ハモリの線は RMVPE で取る（モデルが無ければハモリの線は出さない・音は聴ける）。
        実曲：本物のハモリは声全体の大きい所より約 10 dB 下、リードの消し残りは約 25 dB 下 → 20 dB の門。リードと同じ音は消し残りとして外す */
    void splitHarmony (const std::vector<float>& vocals, const std::vector<float>& lead, double rate, GuideOutcome& out)
    {
        std::vector<float> harm (vocals.size());
        for (size_t i = 0; i < vocals.size(); ++i)
            harm[i] = vocals[i] - (i < lead.size() ? lead[i] : 0.0f);
        std::vector<audio::PitchFrame> points;
        if (pitchWithModel (harm, rate, points, &vocals))
        {
            analysis::rmvpe::gateQuiet (points, harm.data(), (juce::int64) harm.size(), rate, 20.0f, vocals.data());
            dropUnison (points, out.points, rate);
            int harmVoiced = 0, mainVoiced = 0;
            for (auto& p : points)     harmVoiced += p.confidence >= 0.5f ? 1 : 0;
            for (auto& p : out.points) mainVoiced += p.confidence >= 0.5f ? 1 : 0;
            // ハモリがほとんど無い曲（メインの 3% 未満）は、ハモリの線を出さない（音は聴ける）
            if (harmVoiced * 100 >= mainVoiced * 3)
                out.harmPoints = std::move (points);
        }
        out.harmVocals = vocalsAudio (harm, rate);
    }

    std::vector<float> mono (const audio::SongAudio& a)
    {
        const auto n = a.buffer.getNumSamples(), ch = juce::jmax (1, a.buffer.getNumChannels());
        std::vector<float> m ((size_t) n, 0.0f);
        for (int c = 0; c < a.buffer.getNumChannels(); ++c)
        {
            const auto* x = a.buffer.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                m[(size_t) i] += x[i] / (float) ch;
        }
        return m;
    }

    /** 半音の差の表示（+2 / −2） */
    juce::String signedSemitones (int k)
    {
        return (k > 0 ? juce::String ("+") : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"))) + juce::String (std::abs (k));
    }

    GuideOutcome analyseGuide (const juce::File& file, const audio::SongAudio& backing)
    {
        GuideOutcome out;
        juce::AudioFormatManager formats;
        audio::registerSongFormats (formats);
        auto loaded = audio::loadSong (file, formats);
        if (! loaded.ok() || loaded.audio == nullptr)
        {
            out.kind = GuideOutcome::Kind::loadFailed;
            out.error = audio::errorKey (loaded.error);
            return out;
        }

        // 同じ SR でそろえる（オフボの元の SR に）
        auto guide = loaded.audio;
        if (std::abs (guide->sampleRate - backing.sampleRate) > 0.5)
            guide = audio::resampleSong (*guide, backing.sampleRate);
        if (guide == nullptr)
        {
            out.kind = GuideOutcome::Kind::loadFailed;
            out.error = "load.error.readFailed";
            return out;
        }

        const auto ref = mono (*guide), kar = mono (backing);
        const auto rate = backing.sampleRate;
        const auto align = analysis::alignReference (ref.data(), (juce::int64) ref.size(), kar.data(), (juce::int64) kar.size(), rate);
        if (! align.found())
            return out;   // notAligned
        out.offsetSeconds = (double) align.offsetSamples / rate;
        out.alignRough = align.quality == analysis::AlignResult::Quality::rough;
        out.tempoRatio = align.tempoRatio;
        out.covered = coveredSpans (align, (juce::int64) kar.size(), rate);

        // 速さの違うバージョン：時間はずれていき、音程も変わる。嘘の線を出さない（DESIGN 7.1.1）
        if (std::abs (align.tempoRatio - 1.0) > 0.002)
        {
            out.kind = GuideOutcome::Kind::tempoDiffers;
            return out;
        }

        // 聞き比べ用の原曲（オフボの時間へ。原曲の位置 = オフボの位置 + offset。速さが違うバージョンは作らない）
        if (std::abs (align.tempoRatio - 1.0) < 0.001)
        {
            auto o = std::make_shared<audio::SongAudio>();
            o->sampleRate = rate;
            o->buffer.setSize (1, (int) kar.size());
            o->buffer.clear();
            for (juce::int64 i = 0; i < (juce::int64) kar.size(); ++i)
            {
                const auto j = i + align.offsetSamples;
                if (j >= 0 && j < (juce::int64) ref.size())
                    o->buffer.setSample (0, (int) i, ref[(size_t) j]);
            }
            out.original = o;
        }

        // キー違いのカラオケは引けない（音程は分かっても声が取り出せない）
        const auto key = analysis::estimateKeyShift (ref.data(), (juce::int64) ref.size(), kar.data(), (juce::int64) kar.size(), rate);
        if (key.semitones != 0 && key.confidence > 0.3)
        {
            out.kind = GuideOutcome::Kind::keyShift;
            out.keyShift = key.semitones;
            return out;
        }

        std::vector<float> vocals;
        auto r = analysis::referencePitch (ref.data(), (juce::int64) ref.size(), kar.data(), (juce::int64) kar.size(), rate, align, {}, &vocals);
        switch (r.status)
        {
            case analysis::RefPitchResult::Status::ok:
                out.kind = GuideOutcome::Kind::ok;
                out.points = std::move (r.points);
                pitchWithModel (vocals, rate, out.points);   // モデルがあれば線は RMVPE で取り直す
                out.vocals = vocalsAudio (vocals, rate);   // お手本の声を聴く
                break;
            case analysis::RefPitchResult::Status::needsSeparation: out.kind = GuideOutcome::Kind::needsSeparation; break;
            case analysis::RefPitchResult::Status::notAligned:
            case analysis::RefPitchResult::Status::cancelled:       out.kind = GuideOutcome::Kind::notAligned; break;
        }
        return out;
    }
}

void UiSession::loadGuide (const juce::File& file)
{
    if (! isEngineDriven() || s.songOriginal == nullptr)
    {
        postNotice (tr ("guide.problem.noBacking"));
        return;
    }
    if (s.guideBusy)
        return;

    s.guideBusy = true;
    s.guideName = file.getFileName();
    s.guideKaraokeKey = 0;
    s.guideAlignRough = false;
    s.guideCovered.clear();
    if (s.guidePath.isNotEmpty() && ! s.projectFolder.getChildFile (s.guidePath).getFileName().equalsIgnoreCase (file.getFileName()))
        s.guideNudgeMs = 0.0;   // 別のお手本：前の手直しは引き継がない

    // 原曲もプロジェクトの中にコピーして持つ（次に開いた時に合わせ直す。書き出しやプロジェクトの外には出さない。B14）
    if (file.isAChildOf (s.projectFolder))
        s.guidePath = file.getRelativePathFrom (s.projectFolder).replaceCharacter ('\\', '/');
    else
    {
        s.guidePath = "Audio/Guide/" + file.getFileName();
        copyIntoProject (file, s.guidePath);
    }
    markDirty();
    postNotice (tr ("guide.analysing", s.guideName));
    notify (change::view);

    const auto backing = s.songOriginal;
    const auto serial = s.songSerial;
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, file, backing, serial]
    {
        auto out = std::make_shared<GuideOutcome> (analyseGuide (file, *backing));
        juce::MessageManager::callAsync ([this, weak, out, serial, songRate = backing->sampleRate]
        {
            if (weak.expired() || serial != s.songSerial)
                return;   // 消えた・別の曲を開いた
            s.guideBusy = false;
            using Kind = GuideOutcome::Kind;
            // 聞き比べ用の原曲（合わせられた時）。保存してある手直しもここで当てる
            s.guideOriginal = out->original;
            s.guideAlignRough = out->alignRough;
            setGuideCovered (out->covered, songRate);
            s.guideOriginal = shiftedAudio (s.guideOriginal, (int64) std::llround (s.guideNudgeMs * 0.001 * s.sampleRate()), s.sampleRate());
            if (s.guideOriginal == nullptr)
                s.listenOriginal = false;
            syncGuideToEngine();
            switch (out->kind)
            {
                case Kind::ok:
                {
                    // オフボの元の SR → いまの時間軸の SR（録音形式で SR をそろえていれば）
                    const auto ratio = (double) s.sampleRate() / songRate;
                    s.refPitch.clear();
                    s.refPitch.reserve (out->points.size());
                    for (auto& f : out->points)
                        s.refPitch.push_back ({ (int64) std::llround ((double) f.songSample * ratio), f.midi, f.confidence, 0.0f, true });
                    rejudgeAll();
                    updateTakeStats();
                    s.guideVocals = out->vocals;
                    s.refPitchHarm.clear();          // 引き算の声は 1 本。リードのモデルがあれば、続けてリードとハモリに分ける
                    s.guideHarmVocals = nullptr;
                    if (applyGuideNudge()) { rejudgeAll(); updateTakeStats(); }
                    syncGuideToEngine();
                    notify (change::takes | change::monitor);
                    postNotice (tr (s.guideAlignRough ? "guide.doneRough" : "guide.done", juce::String (out->offsetSeconds, 2)));
                    extractLead();
                    break;
                }
                case Kind::loadFailed:      postNotice (tr (out->error.toRawUTF8(), s.guideName)); break;
                case Kind::tempoDiffers:    postNotice (tr ("guide.problem.tempo", juce::String (out->tempoRatio * 100.0, 1))); break;
                case Kind::notAligned:      postNotice (tr ("guide.problem.notAligned")); break;
                case Kind::needsSeparation:
                case Kind::keyShift:
                    // 引き算では声が取れない（ミックス違い・キー違いのカラオケ）：分離（B16）が使えれば勧める。
                    // キー違いは分離した声の線をカラオケのキーにずらして重ねる（DESIGN 7.1.1）。
                    // モデルが無ければ理由と「分離モデルを入れる」キー（ダウンロードは使う人が押した時だけ。勝手に始めない・この知らせを出すだけ）
                    s.guideNeedsSeparation = true;
                    s.guideKaraokeKey = out->kind == Kind::keyShift ? out->keyShift : 0;
                    if (separationAvailable() && separationCached())
                        separateGuide();   // 前に分離した結果がある：聞かずにそれを使う（数秒）
                    else if (separationAvailable()) { ++s.separationOfferSerial; notify (change::notice); }
                    else
                    {
                        if (separation::SeparatorClient::executable().existsAsFile() && ! models::trustedKeys().empty())
                            s.modelDl.noticeSerial = s.noticeSerial + 1;
                        postNotice (s.guideKaraokeKey != 0 ? tr ("separation.noModelKey", signedSemitones (s.guideKaraokeKey))
                                                           : tr ("separation.noModel"));
                    }
                    break;
            }
            notify (change::view);
        });
    });
}

//==============================================================================
namespace
{
    bool writeFloatWav (const juce::File& file, const juce::AudioBuffer<float>& b, double rate)
    {
        file.getParentDirectory().createDirectory();
        const auto temp = file.getSiblingFile (file.getFileName() + ".part");
        temp.deleteFile();
        {
            auto fs = std::make_unique<juce::FileOutputStream> (temp);
            if (! fs->openedOk()) return false;
            std::unique_ptr<juce::OutputStream> stream (fs.release());
            juce::WavAudioFormat wav;
            auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (b.getNumChannels())
                                                    .withBitsPerSample (32).withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
            if (w == nullptr || ! w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples())) return false;
        }
        file.deleteFile();
        return temp.moveFileTo (file);
    }

    std::shared_ptr<audio::SongAudio> readAudio (const juce::File& f)
    {
        juce::AudioFormatManager formats;
        audio::registerSongFormats (formats);
        auto loaded = audio::loadSong (f, formats);
        if (! loaded.ok() || loaded.audio == nullptr)
            return {};
        return std::const_pointer_cast<audio::SongAudio> (loaded.audio);
    }
}

bool UiSession::separationAvailable() const
{
    // 本物のアプリ（UI_MOCK でない）で、分離プロセスとモデルがある。曲はまだ無くてよい（原曲だけで始める時）
    return engine != nullptr && separation::SeparatorClient::available();
}

double UiSession::separationEstimateSeconds() const
{
    // 4 コアの Xeon で 30 秒の曲が 109 秒（11.3）。手元のパソコンでは最初の区間を測ってから出し直す
    const auto seconds = s.sampleRate() > 0 ? (double) s.project.lengthSamples / s.sampleRate() : 0.0;
    return seconds * 3.7;
}

//==============================================================================
namespace
{
    /** 小さいファイル（一覧・署名）を取る。大きすぎる・取れなければ false */
    bool fetchSmall (models::HttpSource& http, const juce::String& url, juce::MemoryBlock& out)
    {
        auto r = http.get (url, 0, {});
        if (r.body == nullptr || r.status != 200)
            return false;
        out.reset();
        juce::MemoryOutputStream m (out, false);
        return m.writeFromInputStream (*r.body, 2 * 1024 * 1024) >= 0 && r.body->isExhausted();
    }
}

bool UiSession::modelsMissing() const
{
    using SC = separation::SeparatorClient;
    using DS = models::DownloadStatus::Stage;
    if (engine == nullptr || models::trustedKeys().empty() || ! SC::executable().existsAsFile())
        return false;
    if (s.modelDl.stage == (int) DS::downloading || s.modelDl.stage == (int) DS::verifying)
        return false;
    return ! SC::modelInstalled() || ! SC::karaokeInstalled() || ! SC::pitchModelFile().existsAsFile();
}

void UiSession::requestSeparationModel (bool onlyIfMissing)
{
    if (engine == nullptr)
        return;
    const auto keys = models::trustedKeys();
    if (keys.empty())
    {
        if (! onlyIfMissing)
            postNotice (tr ("model.notYet"));   // 配布の鍵がまだ（持ち主が用意したら使える）
        return;
    }
    auto openDialog = [this, onlyIfMissing]
    {
        if (onlyIfMissing && modelsToDownload().empty())
            return;   // 一覧にある物はすべて入っている（照合し直しは勧めない）
        // 分離・リードボーカル（ハモリのお手本）・音程のモデルのうち、まだ入っていない物をまとめて入れる（名前・ライセンス・大きさを並べる）
        juce::StringArray titles, licenses;
        juce::int64 size = 0;
        // 名前はかっこ書き（作者）を省く：3 つ並べても 1 行に収める（作者は「このアプリについて」に出す）
        for (auto& [entry, folder] : modelsToDownload())
        {
            titles.add (entry.title.upToFirstOccurrenceOf (" (", false, false));
            licenses.addIfNotAlreadyThere (entry.license);
            size += entry.totalSize();
        }
        if (titles.isEmpty() && modelEntry != nullptr)   // すべて入っている：分離のモデルを照合し直す
        {
            titles.add (modelEntry->title.upToFirstOccurrenceOf (" (", false, false));
            licenses.add (modelEntry->license);
            size = modelEntry->totalSize();
        }
        s.modelDl.known = true;
        s.modelDl.title = titles.joinIntoString (" + ");
        s.modelDl.license = licenses.joinIntoString (" / ");
        s.modelDl.size = size;
        ++s.modelDl.dialogSerial;
        notify (change::notice);
    };
    if (modelEntry != nullptr)
    {
        openDialog();
        return;
    }

    postNotice (tr ("model.checking"));
    const auto wantedId = separation::SeparatorClient::modelId();
    const auto karaokeId = separation::SeparatorClient::karaokeModelId();
    const auto pitchId = separation::SeparatorClient::pitchModelId();
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, keys, wantedId, karaokeId, pitchId, openDialog]
    {
        auto http = models::makeHttpSource();
        juce::String error;
        std::shared_ptr<models::ModelEntry> found, foundKaraoke, foundPitch;
        juce::MemoryBlock list, sig;
        const auto url = models::manifestUrl();
        if (! fetchSmall (*http, url, list) || ! fetchSmall (*http, url + ".sig", sig))
            error = "can't reach " + url;
        else
        {
            // 署名を確かめてから中身を読む（HTTPS だけに頼らない。11.7）
            bool signedOk = false;
            for (auto& k : keys)
                signedOk = signedOk || models::verifySignature (list, sig.toString(), k);
            models::Manifest m;
            if (! signedOk)
                error = "the model list's signature doesn't match";
            else if (! models::parseManifest (list.toString(), m, error))
                error = "bad model list: " + error;
            else if (const auto* e = m.find (wantedId))
            {
                found = std::make_shared<models::ModelEntry> (*e);
                // 無くても分離は入れられる（ハモリのお手本が出ない・線は YIN のまま）
                if (const auto* k = m.find (karaokeId)) foundKaraoke = std::make_shared<models::ModelEntry> (*k);
                if (const auto* p = m.find (pitchId))   foundPitch = std::make_shared<models::ModelEntry> (*p);
            }
            else
                error = "the model isn't in the list";
        }
        juce::MessageManager::callAsync ([this, weak, found, foundKaraoke, foundPitch, error, openDialog]
        {
            if (weak.expired()) return;
            if (found == nullptr)
            {
                postNotice (tr ("model.listFailed", error));
                return;
            }
            modelEntry = std::make_unique<models::ModelEntry> (*found);
            if (foundKaraoke != nullptr) karaokeEntry = std::make_unique<models::ModelEntry> (*foundKaraoke);
            if (foundPitch != nullptr)   pitchEntry = std::make_unique<models::ModelEntry> (*foundPitch);
            openDialog();
        });
    });
}

std::vector<std::pair<models::ModelEntry, juce::File>> UiSession::modelsToDownload() const
{
    // 分離 → リードボーカル → 音程の順。入っている物は飛ばす
    std::vector<std::pair<models::ModelEntry, juce::File>> list;
    using SC = separation::SeparatorClient;
    if (modelEntry != nullptr && ! SC::modelInstalled())
        list.push_back ({ *modelEntry, SC::modelFolder() });
    if (karaokeEntry != nullptr && ! SC::karaokeInstalled())
        list.push_back ({ *karaokeEntry, SC::karaokeModelFolder() });
    if (pitchEntry != nullptr && ! SC::pitchModelFile().existsAsFile())
        list.push_back ({ *pitchEntry, SC::pitchModelFile().getParentDirectory() });
    return list;
}

void UiSession::startModelDownload()
{
    if (modelEntry == nullptr || s.isRecording)
        return;
    if (modelDownloader == nullptr)
        modelDownloader = std::make_unique<models::ModelDownloader> (models::makeHttpSource());
    if (modelDownloader->isBusy())
        return;
    s.modelDl.stage = (int) models::DownloadStatus::Stage::downloading;
    s.modelDl.received = 0;
    s.modelDl.error = {};
    // 進み具合はまとめた大きさで出す。すべて入っていれば分離のモデルを照合し直す
    downloadQueue = modelsToDownload();
    if (downloadQueue.empty())
        downloadQueue.push_back ({ *modelEntry, separation::SeparatorClient::modelFolder() });
    juce::int64 total = 0;
    for (auto& q : downloadQueue)
        total += q.first.totalSize();
    startQueuedModel (0, 0, total);
    notify (change::view | change::notice);
}

void UiSession::startQueuedModel (size_t index, juce::int64 offset, juce::int64 total)
{
    if (index >= downloadQueue.size())
        return;
    std::weak_ptr<bool> weak = alive;
    const bool more = index + 1 < downloadQueue.size();
    modelDownloader->start (downloadQueue[index].first, downloadQueue[index].second,
                            [this, weak, index, offset, total, more] (const models::DownloadStatus& st)
    {
        if (weak.expired()) return;
        const auto before = s.modelDl.stage;
        auto stage = st.stage;
        // 1 つ終わっても次が残っていれば、まだ「受け取り中」
        const bool next = more && stage == models::DownloadStatus::Stage::done;
        if (next)
            stage = models::DownloadStatus::Stage::downloading;
        s.modelDl.stage = (int) stage;
        s.modelDl.received = offset + st.received;
        s.modelDl.size = juce::jmax (total, offset + st.total);
        s.modelDl.bytesPerSecond = st.bytesPerSecond;
        s.modelDl.retryIn = st.retryInSeconds;
        s.modelDl.attempt = st.attempt;
        s.modelDl.paused = st.paused;
        s.modelDl.error = st.error;
        notify (before != s.modelDl.stage ? (juce::uint32) (change::view | change::notice) : (juce::uint32) change::view);
        if (next)
            startQueuedModelWhenFree (index + 1, offset + st.total, total);
    });
}

void UiSession::startQueuedModelWhenFree (size_t index, juce::int64 offset, juce::int64 total)
{
    // 前の物を受け取ったスレッドが終わるのを待ってから始める（終わる前は start が断る）
    std::weak_ptr<bool> weak = alive;
    juce::Timer::callAfterDelay (100, [this, weak, index, offset, total]
    {
        if (weak.expired() || modelDownloader == nullptr || s.modelDl.stage < 0)
            return;   // 消えた・やめた
        if (modelDownloader->isBusy())
            startQueuedModelWhenFree (index, offset, total);
        else
            startQueuedModel (index, offset, total);
    });
}

void UiSession::cancelModelDownload()
{
    if (modelDownloader != nullptr)
        modelDownloader->cancel();   // 届いた分（.part）は残す。次は続きから
    s.modelDl.stage = -1;
    notify (change::view | change::notice);
}

void UiSession::stopSeparation()
{
    if (separator != nullptr)
        separator->stop();
}

juce::File UiSession::separationCacheFolder() const
{
    // キャッシュ（Cache/ は消しても作り直せる。DESIGN 8）：原曲のファイル・大きさ・日時・モデルが同じなら分離し直さない
    const auto guide = s.projectFolder.getChildFile (s.guidePath);
    const auto key = juce::String::toHexString ((juce::int64) (s.guidePath + "|" + juce::String (guide.getSize()) + "|"
                                                               + juce::String (guide.getLastModificationTime().toMilliseconds()) + "|"
                                                               + separation::SeparatorClient::modelId()).hashCode64());
    return s.projectFolder.getChildFile ("Cache/separation/" + key);
}

bool UiSession::separationCached() const
{
    if (s.guidePath.isEmpty() || s.projectFolder == juce::File())
        return false;
    const auto dir = separationCacheFolder();
    return dir.getChildFile ("vocals.wav").existsAsFile() && dir.getChildFile ("backing.wav").existsAsFile();
}

void UiSession::separateGuide()
{
    if (! separationAvailable() || s.guidePath.isEmpty() || s.separating || s.songOriginal == nullptr)
        return;
    const auto guide = s.projectFolder.getChildFile (s.guidePath);
    if (! guide.existsAsFile())
        return;

    const auto dir = separationCacheFolder();
    const auto vocals = dir.getChildFile ("vocals.wav"), backing = dir.getChildFile ("backing.wav"), mix = dir.getChildFile ("mix.wav"),
               lead = dir.getChildFile ("lead.wav");   // リードボーカル（karaoke のモデルが入っていれば。ハモリのお手本）
    if (vocals.existsAsFile() && backing.existsAsFile())
    {
        analyseSeparated (vocals, backing);
        return;
    }

    s.separating = true;
    s.separationProgress = 0.0f;
    s.separationEta = -1.0;
    s.guideBusy = true;
    postNotice (tr ("separation.started"));
    notify (change::view);

    // 44.1 kHz ステレオにそろえて渡す（モデルの約束。11.3）
    std::weak_ptr<bool> weak = alive;
    const auto serial = s.songSerial;
    juce::Thread::launch ([this, weak, guide, mix, vocals, backing, lead, serial]
    {
        bool ok = false;
        if (auto a = readAudio (guide))
        {
            std::shared_ptr<const audio::SongAudio> at44 = a;
            if (std::abs (a->sampleRate - analysis::separation::sampleRate) > 0.5)
                at44 = audio::resampleSong (*a, analysis::separation::sampleRate);
            ok = at44 != nullptr && writeFloatWav (mix, at44->buffer, analysis::separation::sampleRate);
        }
        juce::MessageManager::callAsync ([this, weak, ok, mix, vocals, backing, lead, serial]
        {
            if (weak.expired())
                return;
            auto fail = [this] (const juce::String& why)
            {
                s.separating = false;
                s.guideBusy = false;
                postNotice (why);
                notify (change::view);
            };
            if (serial != s.songSerial) { fail ({}); return; }
            if (! ok) { fail (tr ("separation.failed", "can't prepare the input")); return; }

            if (separator == nullptr)
                separator = std::make_unique<separation::SeparatorClient>();
            separation::SeparatorClient::Callbacks cb;
            cb.progress = [this, weak] (float p, double eta)
            {
                if (weak.expired()) return;
                s.separationProgress = p;
                s.separationEta = eta;
                notify (change::view);
            };
            cb.done = [this, weak, mix, vocals, backing, serial, fail] (bool done, const juce::String& error)
            {
                if (weak.expired()) return;
                mix.deleteFile();   // 入力の写しは消す（結果だけ残す）
                s.separating = false;
                if (serial != s.songSerial) { s.guideBusy = false; return; }
                if (! done)
                {
                    fail (error == "stopped" ? tr ("separation.stopped") : tr ("separation.failed", error));
                    return;
                }
                analyseSeparated (vocals, backing);
            };
            if (! separator->start (mix, vocals, backing, std::move (cb), lead))
                fail (tr ("separation.failed", "busy"));
        });
    });
}

void UiSession::makeOffVocal (const juce::File& original, std::function<void (juce::File, juce::String)> done)
{
    auto fail = [done] (const juce::String& why) { if (done) done ({}, why); };
    if (! separationAvailable()) { fail (tr ("separation.noModelOriginal")); return; }
    if (s.separating)            { fail (tr ("separation.failed", "busy")); return; }

    // 作ったオフボはアプリ共通のキャッシュ（設定の「キャッシュの場所」）の offvocal/ に置く（曲を開くとプロジェクトの中にコピーされる）。
    // 同じ原曲・モデルなら作り直さない
    const auto key = juce::String::toHexString ((juce::int64) (original.getFullPathName() + "|" + juce::String (original.getSize()) + "|"
                                                               + juce::String (original.getLastModificationTime().toMilliseconds()) + "|"
                                                               + separation::SeparatorClient::modelId()).hashCode64());
    const auto dir = cacheFolder().getChildFile ("offvocal/" + key);
    const auto out = dir.getChildFile (juce::File::createLegalFileName (original.getFileNameWithoutExtension() + " (off vocal)") + ".wav");
    if (out.existsAsFile())
    {
        if (done) done (out, {});
        return;
    }
    const auto mix = dir.getChildFile ("mix.wav"), vocals = dir.getChildFile ("vocals.wav"), backing = dir.getChildFile ("backing.wav");

    s.separating = true;
    s.separationProgress = 0.0f;
    s.separationEta = -1.0;
    notify (change::view);

    std::weak_ptr<bool> weak = alive;
    auto finish = [this, weak, done, mix, vocals, backing] (const juce::File& made, const juce::String& error)
    {
        if (weak.expired()) return;
        mix.deleteFile();
        vocals.deleteFile();   // 声は残さない（お手本の線は開いた後に「原曲 − オフボ」で出す）
        backing.deleteFile();
        s.separating = false;
        notify (change::view);
        if (done) done (made, error);
    };

    // 44.1 kHz ステレオにそろえて渡す（モデルの約束。11.3）
    juce::Thread::launch ([this, weak, original, mix, vocals, backing, out, finish]
    {
        bool ok = false;
        if (auto a = readAudio (original))
        {
            std::shared_ptr<const audio::SongAudio> at44 = a;
            if (std::abs (a->sampleRate - analysis::separation::sampleRate) > 0.5)
                at44 = audio::resampleSong (*a, analysis::separation::sampleRate);
            ok = at44 != nullptr && writeFloatWav (mix, at44->buffer, analysis::separation::sampleRate);
        }
        juce::MessageManager::callAsync ([this, weak, ok, original, mix, vocals, backing, out, finish]
        {
            if (weak.expired()) return;
            if (! ok) { finish ({}, tr ("separation.failed", "can't read the original")); return; }

            if (separator == nullptr)
                separator = std::make_unique<separation::SeparatorClient>();
            separation::SeparatorClient::Callbacks cb;
            cb.progress = [this, weak] (float p, double eta)
            {
                if (weak.expired()) return;
                s.separationProgress = p;
                s.separationEta = eta;
                notify (change::view);
            };
            cb.done = [weak, original, vocals, out, finish] (bool separated, const juce::String& error)
            {
                if (weak.expired()) return;
                if (! separated)
                {
                    finish ({}, error == "stopped" ? tr ("separation.stopped") : tr ("separation.failed", error));
                    return;
                }
                // 原曲の SR・長さのまま、声を引いてオフボにする（裏で）
                juce::Thread::launch ([original, vocals, out, finish]
                {
                    bool wrote = false;
                    auto a = readAudio (original);
                    auto v = readAudio (vocals);
                    if (a != nullptr && v != nullptr)
                        wrote = writeFloatWav (out, analysis::offVocalFrom (*a, *v), a->sampleRate);
                    juce::MessageManager::callAsync ([wrote, out, finish]
                    {
                        if (wrote) finish (out, {});
                        else       finish ({}, tr ("separation.failed", "can't write the off vocal"));
                    });
                });
            };
            if (! separator->start (mix, vocals, backing, std::move (cb)))
                finish ({}, tr ("separation.failed", "busy"));
        });
    });
}

//==============================================================================
// ハモリのお手本（2026-10-03）：原曲 − カラオケ（引き算）で取ったお手本も、リードボーカルのモデルがあればリードとハモリに分ける。
// 原曲にリードのモデルだけを回し（分離プロセスの --model にリードのモデル）、リード（原曲の時間）をオフボの時間へ写して、ハモリ = 取り出した声 − リード
juce::File UiSession::leadCacheFolder() const
{
    const auto guide = s.projectFolder.getChildFile (s.guidePath);
    const auto key = juce::String::toHexString ((juce::int64) (s.guidePath + "|" + juce::String (guide.getSize()) + "|"
                                                               + juce::String (guide.getLastModificationTime().toMilliseconds()) + "|"
                                                               + separation::SeparatorClient::karaokeModelId()).hashCode64());
    return s.projectFolder.getChildFile ("Cache/lead/" + key);
}

void UiSession::extractLead()
{
    using SC = separation::SeparatorClient;
    if (engine == nullptr || ! SC::executable().existsAsFile() || ! SC::karaokeInstalled() || s.guidePath.isEmpty()
        || s.guideVocals == nullptr || s.songOriginal == nullptr || s.separating)
        return;
    const auto guide = s.projectFolder.getChildFile (s.guidePath);
    if (! guide.existsAsFile())
        return;
    const auto dir = leadCacheFolder();
    const auto lead = dir.getChildFile ("lead.wav"), rest = dir.getChildFile ("rest.wav"), mix = dir.getChildFile ("mix.wav");
    if (lead.existsAsFile())
    {
        analyseLead (lead);
        return;
    }

    s.separating = true;
    s.separationProgress = 0.0f;
    s.separationEta = -1.0;
    postNotice (tr ("separation.leadStarted"));
    notify (change::view);

    std::weak_ptr<bool> weak = alive;
    const auto serial = s.songSerial;
    juce::Thread::launch ([this, weak, guide, mix, lead, rest, serial]
    {
        bool ok = false;
        if (auto a = readAudio (guide))
        {
            std::shared_ptr<const audio::SongAudio> at44 = a;
            if (std::abs (a->sampleRate - analysis::separation::sampleRate) > 0.5)
                at44 = audio::resampleSong (*a, analysis::separation::sampleRate);
            mix.getParentDirectory().createDirectory();
            ok = at44 != nullptr && writeFloatWav (mix, at44->buffer, analysis::separation::sampleRate);
        }
        juce::MessageManager::callAsync ([this, weak, ok, mix, lead, rest, serial]
        {
            if (weak.expired())
                return;
            auto fail = [this] (const juce::String& why)
            {
                s.separating = false;
                if (why.isNotEmpty())
                    postNotice (why);
                notify (change::view);
            };
            if (serial != s.songSerial) { fail ({}); return; }
            if (! ok) { fail (tr ("separation.failed", "can't prepare the input")); return; }
            if (separator == nullptr)
                separator = std::make_unique<separation::SeparatorClient>();
            separation::SeparatorClient::Callbacks cb;
            cb.progress = [this, weak] (float p, double eta)
            {
                if (weak.expired()) return;
                s.separationProgress = p;
                s.separationEta = eta;
                notify (change::view);
            };
            cb.done = [this, weak, mix, lead, rest, serial, fail] (bool done, const juce::String& error)
            {
                if (weak.expired()) return;
                mix.deleteFile();
                rest.deleteFile();   // リード以外（声の残り＋伴奏）は使わない
                s.separating = false;
                if (serial != s.songSerial) return;
                if (! done)
                {
                    fail (error == "stopped" ? tr ("separation.stopped") : tr ("separation.failed", error));
                    return;
                }
                analyseLead (lead);
            };
            if (! separator->start (mix, lead, rest, std::move (cb), {}, separation::SeparatorClient::karaokeModelFolder()))
                fail (tr ("separation.failed", "busy"));
        });
    });
}

void UiSession::analyseLead (const juce::File& leadFile)
{
    const auto karaoke = s.songOriginal;
    const auto vocals = s.guideVocals;   // 取り出した声（オフボの時間・オフボの元の SR）
    const auto guide = s.projectFolder.getChildFile (s.guidePath);
    if (karaoke == nullptr || vocals == nullptr)
        return;
    const auto serial = s.songSerial;
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, leadFile, guide, karaoke, vocals, serial]
    {
        auto out = std::make_shared<GuideOutcome>();
        const auto rate = karaoke->sampleRate;
        std::shared_ptr<const audio::SongAudio> l = readAudio (leadFile), g = readAudio (guide);
        if (l != nullptr && std::abs (l->sampleRate - rate) > 0.5) l = audio::resampleSong (*l, rate);
        if (g != nullptr && std::abs (g->sampleRate - rate) > 0.5) g = audio::resampleSong (*g, rate);
        if (l != nullptr && g != nullptr)
        {
            const auto ref = mono (*g), kar = mono (*karaoke), leadMono = mono (*l);
            const auto align = analysis::alignReference (ref.data(), (juce::int64) ref.size(), kar.data(), (juce::int64) kar.size(), rate);
            std::vector<float> leadOnBacking;
            auto r = analysis::pitchFromVocals (leadMono.data(), (juce::int64) leadMono.size(), (juce::int64) kar.size(), rate, align, {}, &leadOnBacking);
            if (align.found() && r.status == analysis::RefPitchResult::Status::ok)
            {
                out->kind = GuideOutcome::Kind::ok;
                out->points = std::move (r.points);
                pitchWithModel (leadOnBacking, rate, out->points);
                out->vocals = vocalsAudio (leadOnBacking, rate);
                const auto all = mono (*vocals);
                splitHarmony (all, leadOnBacking, rate, *out);
            }
        }
        juce::MessageManager::callAsync ([this, weak, out, serial, songRate = karaoke->sampleRate]
        {
            if (weak.expired() || serial != s.songSerial || out->kind != GuideOutcome::Kind::ok)
                return;   // 取れなければ今の（分けていない）お手本のまま
            const auto ratio = (double) s.sampleRate() / songRate;
            auto toRef = [ratio] (const std::vector<audio::PitchFrame>& points)
            {
                std::vector<dummy::PitchPoint> ref;
                ref.reserve (points.size());
                for (auto& f : points)
                    ref.push_back ({ (int64) std::llround ((double) f.songSample * ratio), f.midi, f.confidence, 0.0f, true });
                return ref;
            };
            s.refPitch = toRef (out->points);
            s.refPitchHarm = toRef (out->harmPoints);
            rejudgeAll();
            updateTakeStats();
            s.guideVocals = out->vocals;
            s.guideHarmVocals = out->harmVocals;
            if (applyGuideNudge()) { rejudgeAll(); updateTakeStats(); }
            syncGuideToEngine();
            notify (change::takes | change::monitor | change::view);
            postNotice (tr ("guide.harmonyDone"));
        });
    });
}

void UiSession::analyseSeparated (const juce::File& vocalsFile, const juce::File& backingFile)
{
    // 分離した声（原曲の時間）→ オフボの SR にそろえる → 分離した伴奏とオフボで時間を合わせる → 声の音程をオフボの時間へ
    s.guideBusy = true;
    notify (change::view);
    const auto karaoke = s.songOriginal;
    const auto serial = s.songSerial;
    const auto key = s.guideKaraokeKey;   // キー違いのカラオケ：お手本の声もカラオケのキーへずらして鳴らす
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, vocalsFile, backingFile, karaoke, serial, key]
    {
        auto out = std::make_shared<GuideOutcome>();
        auto voc = readAudio (vocalsFile);
        auto back = readAudio (backingFile);
        std::shared_ptr<const audio::SongAudio> v = voc, b = back;
        if (v != nullptr && std::abs (v->sampleRate - karaoke->sampleRate) > 0.5) v = audio::resampleSong (*v, karaoke->sampleRate);
        if (b != nullptr && std::abs (b->sampleRate - karaoke->sampleRate) > 0.5) b = audio::resampleSong (*b, karaoke->sampleRate);
        if (v == nullptr || b == nullptr)
        {
            out->kind = GuideOutcome::Kind::loadFailed;
            out->error = "load.error.readFailed";
        }
        else
        {
            const auto rate = karaoke->sampleRate;
            const auto backMono = mono (*b), kar = mono (*karaoke), vm = mono (*v);
            const auto align = analysis::alignReference (backMono.data(), (juce::int64) backMono.size(), kar.data(), (juce::int64) kar.size(), rate);
            if (align.found())
            {
                out->offsetSeconds = (double) align.offsetSamples / rate;
                out->alignRough = align.quality == analysis::AlignResult::Quality::rough;
                out->covered = coveredSpans (align, (juce::int64) kar.size(), rate);
                // リードボーカル（karaoke のモデル、2026-10-02）があれば、お手本はリード、ハモリ = 声 − リード
                std::vector<float> leadMono;
                if (auto l = readAudio (vocalsFile.getSiblingFile ("lead.wav")))
                {
                    std::shared_ptr<const audio::SongAudio> la = l;
                    if (std::abs (la->sampleRate - rate) > 0.5) la = audio::resampleSong (*la, rate);
                    if (la != nullptr)
                        leadMono = mono (*la);
                }
                const bool split = ! leadMono.empty();
                leadMono.resize (vm.size(), 0.0f);

                std::vector<float> onBacking;   // お手本の声（オフボの時間）。聴く用
                const auto& mainSrc = split ? leadMono : vm;
                auto r = analysis::pitchFromVocals (mainSrc.data(), (juce::int64) mainSrc.size(), (juce::int64) kar.size(), rate, align, {}, &onBacking);
                if (r.status == analysis::RefPitchResult::Status::ok)
                {
                    out->kind = GuideOutcome::Kind::ok;
                    out->points = std::move (r.points);
                    pitchWithModel (onBacking, rate, out->points);   // モデルがあれば線は RMVPE で取り直す
                    out->vocals = vocalsAudio (onBacking, rate);
                }
                if (split && out->kind == GuideOutcome::Kind::ok)
                {
                    std::vector<float> allOnBacking;   // 声全体（オフボの時間）
                    analysis::pitchFromVocals (vm.data(), (juce::int64) vm.size(), (juce::int64) kar.size(), rate, align, {}, &allOnBacking);
                    splitHarmony (allOnBacking, onBacking, rate, *out);
                }
                if (key != 0 && out->kind == GuideOutcome::Kind::ok)
                {
                    // 線は合わせた後に半音ずらす（メッセージスレッド）。聴く声はここで高さだけ変える（長さ・位置はそのまま）
                    auto shifted = [key] (const std::shared_ptr<const audio::SongAudio>& a) -> std::shared_ptr<const audio::SongAudio>
                    {
                        if (a == nullptr)
                            return nullptr;
                        auto r = std::make_shared<audio::SongAudio>();
                        r->sampleRate = a->sampleRate;
                        const auto y = audio::shiftPitch (a->buffer.getReadPointer (0), a->length(), a->sampleRate, key);
                        r->buffer.setSize (1, (int) y.size());
                        r->buffer.copyFrom (0, 0, y.data(), (int) y.size());
                        return r;
                    };
                    out->vocals = shifted (out->vocals);
                    out->harmVocals = shifted (out->harmVocals);
                }
            }
        }
        juce::MessageManager::callAsync ([this, weak, out, serial, key, songRate = karaoke->sampleRate]
        {
            if (weak.expired() || serial != s.songSerial)
                return;
            s.guideBusy = false;
            if (out->kind == GuideOutcome::Kind::ok)
            {
                const auto ratio = (double) s.sampleRate() / songRate;
                auto toRef = [ratio] (const std::vector<audio::PitchFrame>& points)
                {
                    std::vector<dummy::PitchPoint> ref;
                    ref.reserve (points.size());
                    for (auto& f : points)
                        ref.push_back ({ (int64) std::llround ((double) f.songSample * ratio), f.midi, f.confidence, 0.0f, true });
                    return ref;
                };
                s.refPitch = toRef (out->points);
                s.refPitchHarm = toRef (out->harmPoints);
                s.guideAlignRough = out->alignRough;
                setGuideCovered (out->covered, songRate);
                if (key != 0)
                {
                    // キー違いのカラオケ：線もカラオケのキーへ（聴く声は裏でずらしてある）
                    for (auto* ref : { &s.refPitch, &s.refPitchHarm })
                        for (auto& p : *ref)
                            p.midi += (float) key;
                }
                rejudgeAll();
                updateTakeStats();
                s.guideVocals = out->vocals;
                s.guideHarmVocals = out->harmVocals;
                if (applyGuideNudge()) { rejudgeAll(); updateTakeStats(); }
                syncGuideToEngine();
                notify (change::takes | change::monitor);
                if (key != 0)
                    postNotice (tr ("separation.doneKey", juce::String (out->offsetSeconds, 2), signedSemitones (key)));
                else
                    postNotice (tr (out->harmVocals != nullptr ? "separation.doneHarmony" : "separation.done", juce::String (out->offsetSeconds, 2)));
            }
            else if (out->kind == GuideOutcome::Kind::loadFailed) postNotice (tr ("separation.failed", "can't read the result"));
            else                                                  postNotice (tr ("guide.problem.notAligned"));
            notify (change::view);
        });
    });
}

void UiSession::judge (dummy::PitchPoint& p) const
{
    // 同じ位置（10 ms 以内）のお手本の点と比べる。どちらかに声が無ければ「比べていない」
    p.judged = false;
    p.centsOff = 0.0f;
    if (s.activeRef().empty() || p.confidence < 0.5f)
        return;
    const auto tolerance = (int64) (audio::pitch::hopSeconds * s.sampleRate());
    auto it = std::lower_bound (s.activeRef().begin(), s.activeRef().end(), p.sample - tolerance,
                                [] (const dummy::PitchPoint& r, int64 v) { return r.sample < v; });
    const dummy::PitchPoint* best = nullptr;
    for (; it != s.activeRef().end() && it->sample <= p.sample + tolerance; ++it)
        if (best == nullptr || std::abs (it->sample - p.sample) < std::abs (best->sample - p.sample))
            best = &*it;
    if (best == nullptr || best->confidence < 0.5f)
        return;

    auto cents = (p.midi - (best->midi + (float) s.keyShift)) * 100.0f;   // 練習でキーを変えたら、お手本もその分ずらして比べる（B11）
    if (s.octaveAlign)   // オクターブ違い（男女・裏声）は同じ音として比べる
        cents -= 1200.0f * std::round (cents / 1200.0f);
    p.centsOff = cents;
    p.judged = true;
}

namespace
{
    /** お手本の線（オフボの時間）を解析の形に */
    std::vector<audio::PitchFrame> guideFrames (const std::vector<dummy::PitchPoint>& ref)
    {
        std::vector<audio::PitchFrame> guide;
        guide.reserve (ref.size());
        for (auto& p : ref)
            guide.push_back ({ p.sample, p.midi, p.confidence, -20.0f });
        return guide;
    }

    /** テイクの音程を [from, to) の中だけお手本と比べる（札はテイク全体、テイク比較は比べる範囲。B18 / B18c） */
    dummy::Session::TakeStats statsFor (const std::vector<audio::PitchFrame>& guide, const std::vector<audio::PitchFrame>& frames,
                                        double rate, int64 from, int64 to, float toleranceCents)
    {
        const auto onset = analysis::onsetStats (guide, frames, rate, from, to);
        const auto acc = analysis::pitchAccuracy (guide, frames, rate, from, to, toleranceCents);
        dummy::Session::TakeStats st;
        st.entries = onset.entries;
        st.matched = (int) onset.items.size();
        st.onsetMs = onset.medianMs;
        st.inBand = acc.inBand;
        st.meanAbsCents = acc.meanAbsCents;
        st.pitchFrames = acc.frames;
        for (auto& v : analysis::vibratos (frames, rate))
        {
            if (v.end <= from || v.start >= to)
                continue;   // 範囲の外の音符は数えない
            st.vibRateHz += v.rateHz;
            st.vibDepthCents += v.depthCents;
            ++st.vibNotes;
        }
        if (st.vibNotes > 0) { st.vibRateHz /= (float) st.vibNotes; st.vibDepthCents /= (float) st.vibNotes; }
        return st;
    }
}

void UiSession::updateTakeStats (const juce::String& onlyKey)
{
    // お手本と比べる（B18）。お手本が無ければ消すだけ。ハモリのトラックはハモリのお手本（あれば）と
    if (s.refPitch.empty())
    {
        s.takeStats.clear();
        return;
    }
    const auto guide = guideFrames (s.refPitch);
    const auto harmGuide = s.refPitchHarm.empty() ? std::vector<audio::PitchFrame>() : guideFrames (s.refPitchHarm);
    const auto rate = (double) s.sampleRate();

    for (auto& [key, frames] : s.takePitch)
    {
        if ((onlyKey.isNotEmpty() && key != onlyKey) || frames == nullptr || frames->empty())
            continue;
        const bool harm = key.startsWith (juce::String (project::trackKey (project::TrackType::harm1)) + "/")
                       || key.startsWith (juce::String (project::trackKey (project::TrackType::harm2)) + "/");
        const auto from = frames->front().songSample, to = frames->back().songSample + 1;
        s.takeStats[key] = statsFor (harm && ! harmGuide.empty() ? harmGuide : guide, *frames, rate, from, to, s.pitchToleranceCents);
    }
}

const dummy::Session::TakeStats* UiSession::latestTakeStats() const
{
    // いま選んでいるトラックの、いちばん新しい（解析の済んだ）テイク
    const auto* track = s.project.findTrack (s.currentTrack().type);
    if (track == nullptr)
        return nullptr;
    const project::Take* newest = nullptr;
    for (auto& t : track->takes)
        if (s.takeStats.count (dummy::takeWaveKey (track->type, t.id)) > 0 && (newest == nullptr || t.created > newest->created))
            newest = &t;
    if (newest == nullptr)
        return nullptr;
    return &s.takeStats.at (dummy::takeWaveKey (track->type, newest->id));
}

void UiSession::rejudgeAll()
{
    if (! isEngineDriven())
        return;   // 見本（UI_MOCK・デモ）の線は見本のずれのまま
    for (auto& p : s.myPitch)
        judge (p);
}

//==============================================================================
void UiSession::pollPitch()
{
    if (! isEngineDriven())
        return;
    pitchFrames.clear();
    engine->popPitch (pitchFrames);
    if (pitchFrames.empty())
        return;

    // 声域を測っている間の点（曲の外の位置）は曲の線に入れない。測っている時だけ集める
    {
        const auto firstFree = std::stable_partition (pitchFrames.begin(), pitchFrames.end(),
                                                      [] (const audio::PitchFrame& f) { return f.songSample < audio::PitchTracker::freeRunBase; });
        if (rangeMeasuring)
            for (auto it = firstFree; it != pitchFrames.end(); ++it)
                if (it->midi > 0.0f && it->confidence >= 0.6f && rangeNotes.size() < 6000)
                    rangeNotes.push_back (it->midi);
        pitchFrames.erase (firstFree, pitchFrames.end());
        if (pitchFrames.empty())
            return;
    }

    // 位置を歌い手が聞いた伴奏の位置に直す（往復の遅れの分だけ前へ。録音と同じ。B6）
    const auto ld = latencyDisplay (s);
    // 練習でテンポを変えている時（B11）、遅れ（出力のサンプル）は曲のサンプルではその速さの倍
    const auto lat = ld.known ? (int64) std::llround (juce::jmax ((int64) 0, ld.samples) * s.tempoPercent / 100.0) : 0;
    const auto rate = s.sampleRate();
    s.myPitchLag = lat + (int64) ((audio::PitchTracker::processingDelaySeconds() + 0.05) * rate);

    // 新しく歌った所は前の線を消して置き換える（同じ所を歌い直したら新しい線）。シークで戻った所で区切る
    const auto hop = (int64) (audio::pitch::hopSeconds * rate);
    auto& line = s.myPitch;
    auto put = [&] (size_t from, size_t to)
    {
        const auto lo = pitchFrames[from].songSample - lat, hi = pitchFrames[to - 1].songSample - lat + hop / 2;
        auto a = std::lower_bound (line.begin(), line.end(), lo, [] (const dummy::PitchPoint& p, int64 v) { return p.sample < v; });
        auto b = std::lower_bound (a, line.end(), hi, [] (const dummy::PitchPoint& p, int64 v) { return p.sample < v; });
        std::vector<dummy::PitchPoint> pts;
        for (auto i = from; i < to; ++i)
        {
            const auto& f = pitchFrames[i];
            if (f.songSample - lat >= 0)
            {
                dummy::PitchPoint p { f.songSample - lat, f.midi, f.confidence, 0.0f, false };
                judge (p);
                pts.push_back (p);
            }
        }
        a = line.erase (a, b);
        line.insert (a, pts.begin(), pts.end());
    };
    size_t start = 0;
    for (size_t i = 1; i <= pitchFrames.size(); ++i)
        if (i == pitchFrames.size() || pitchFrames[i].songSample <= pitchFrames[i - 1].songSample)
        {
            put (start, i);
            start = i;
        }
    notify (change::view);
}

//==============================================================================
int64 UiSession::prerollSamples() const
{
    // 区間の録り直し（B10）の前に鳴らす長さ（2026-10-02）：COUNT が 1 / 2 でテンポが分かれば、範囲の頭の小節の 1 拍目から数えてその小節数
    // （その間は拍を鳴らして数える。startWithCountIn）。Off・テンポが分からなければ 2 秒の助走（数えない。クリック On なら拍は鳴る）。
    // 曲の頭より前にはみ出す分は、呼ぶ側で 0 から鳴らし、はみ出した分を無音で数える
    const auto rate = s.sampleRate();
    if (s.countInBars > 0 && s.tempoKnown())
        return s.rangeIn - song::countInStart (s.project.tempo, s.rangeIn, s.countInBars, rate);
    return (int64) (2.0 * rate);
}

bool UiSession::promoteRehearsalTake (project::TrackType type, const juce::String& takeId)
{
    if (s.isRecording || s.projectFolder == juce::File())
        return false;
    auto* track = const_cast<project::Track*> (s.project.findTrack (type));
    if (track == nullptr)
        return false;
    auto it = std::find_if (track->takes.begin(), track->takes.end(),
                            [&] (const project::Take& k) { return k.id == takeId && k.recMode == project::RecMode::practice; });
    if (it == track->takes.end())
        return false;

    auto take = *it;
    if (take.tempoPercent != 100 || take.keyShift != 0)
    {
        postNotice (tr ("rescue.notOriginal", take.id, take.tempoPercent, (take.keyShift > 0 ? "+" : "") + juce::String (take.keyShift)));
        return false;
    }

    // ファイルを本番のテイクの所へ移す（同じ名前があれば番号を進める。録った声は上書きしない）
    track->takes.erase (it);
    const auto key = juce::String (project::trackKey (type));
    auto id = project::nextTakeId (*track);
    auto rel = [&] { return "Audio/Takes/" + key + "_" + id + ".wav"; };
    for (int n = id.substring (4).getIntValue(); s.projectFolder.getChildFile (rel()).exists(); )
        id = "take" + juce::String (++n);
    const auto src = s.projectFolder.getChildFile (take.path), dst = s.projectFolder.getChildFile (rel());
    dst.getParentDirectory().createDirectory();
    if (! src.existsAsFile() || ! src.moveFileTo (dst))
    {
        track->takes.push_back (take);   // 戻す
        postNotice (tr ("rescue.failed", src.getFileName()));
        return false;
    }

    const auto oldWave = dummy::takeWaveKey (type, take.id);
    take.id = id;
    take.path = rel();
    take.recMode = project::RecMode::delivery;
    const auto from = take.useFrom >= 0 ? take.useFrom : juce::jmax ((int64) 0, take.startSample + take.latencySamples);
    const auto to = take.useTo > from ? take.useTo : take.endSample;
    take.useFrom = take.useTo = -1;

    compBeforeTake = track->comp;
    undoTrack = type;
    undoIsCompare = false;
    project::applyTake (*track, take, from, to);
    s.canUndoTake = true;
    s.takeWaves.erase (oldWave);
    loadTakeWave (type, take);
    s.rescueNoticeSerial = -1;

    const auto range = formatTime (juce::jmax ((int64) 0, from), s.sampleRate(), true) + " - "
                     + formatTime (juce::jmin (s.project.lengthSamples, to), s.sampleRate(), true);
    saveProject();   // ファイルはもう Audio/Takes にある：自動保存を待たずに書く（落ちても .vbooth と食い違わない）
    postNotice (tr ("rescue.done", trackName (type), id, range));
    notify (change::takes | change::tracks);
    return true;
}

bool UiSession::undoTake()
{
    if (! s.canUndoTake || s.isRecording)
        return false;
    auto* track = const_cast<project::Track*> (s.project.findTrack (undoTrack));
    if (track == nullptr)
        return false;
    track->comp = compBeforeTake;   // テイクとファイルは残す（採用から外すだけ）
    s.canUndoTake = false;
    postNotice (tr (undoIsCompare ? "compare.undone" : "record.undone"));
    notify (change::takes | change::tracks);
    return true;
}

//==============================================================================
bool UiSession::canCompareTakes() const
{
    // 簡単モードには無い（DESIGN 2）。録音中・本物のアプリで曲を開く前（見本のテイクしか無い）も出さない
    if (s.mode == project::Mode::easy || s.isRecording || (s.engineAttached && s.songOriginal == nullptr))
        return false;
    const auto* track = s.project.findTrack (s.currentTrack().type);
    return track != nullptr && ! project::compareCandidates (*track, 0, s.project.lengthSamples).empty();
}

bool UiSession::beginTakeCompare (int64 from, int64 to, dummy::Session::TakeCompare::Scope scope)
{
    if (audition.isActive())
        endTakeCompare (false);
    if (! canCompareTakes())
        return false;
    auto* track = const_cast<project::Track*> (s.project.findTrack (s.currentTrack().type));
    from = juce::jlimit ((int64) 0, s.project.lengthSamples, from);
    to = juce::jlimit ((int64) 0, s.project.lengthSamples, to);
    if (track == nullptr || project::compareCandidates (*track, from, to).empty() || ! audition.begin (*track, from, to))
        return false;

    // 範囲を比べる時はその範囲をループで聴く（同じ所を何度も聴き比べる）。IN / OUT とループは終わったら元に戻す
    compareRangeIn = s.rangeIn;
    compareRangeOut = s.rangeOut;
    compareLoopOn = s.loopOn;
    compareChangedRange = scope != dummy::Session::TakeCompare::Scope::song;
    compareStartedPlay = false;
    if (compareChangedRange)
    {
        s.rangeIn = from;
        s.rangeOut = to;
        s.loopOn = true;
        syncLoopToEngine();
        if (s.isPlaying && (s.playhead < from || s.playhead >= to))
            seek (from);

        // パネルはレーンの右側に重なる：範囲の頭が左半分に無ければ、表示をずらして見えるようにする（拡大率はそのまま）
        const auto span = s.viewEnd - s.viewStart;
        if (span > 0 && (from < s.viewStart || from > s.viewStart + span / 2))
        {
            const auto start = juce::jlimit ((int64) 0, juce::jmax ((int64) 0, s.project.lengthSamples - span), from - span / 12);
            s.viewStart = start;
            s.viewEnd = start + span;
        }
    }

    auto& c = s.compare;
    c.active = true;
    ++c.serial;
    c.track = track->type;
    c.from = from;
    c.to = to;
    c.scope = scope;
    c.previewing.clear();
    c.original = track->comp;
    notify (change::tracks | change::range | change::transport | change::view);
    return true;
}

void UiSession::previewCompareTake (const juce::String& takeId)
{
    if (! audition.isActive() || s.isRecording)
        return;
    auto* track = const_cast<project::Track*> (s.project.findTrack (audition.track()));
    if (track == nullptr || ! audition.preview (*track, takeId))
        return;
    s.compare.previewing = audition.previewing();
    // 採用区間が変わった＝トラックの再生を作り直す（B12。書き出しと同じ計算・同じ継ぎ目）。保存は確定している形（saveProject）
    notify (change::takes | change::tracks);
}

void UiSession::toggleCompareAudition()
{
    if (! audition.isActive())
        return;
    if (s.isPlaying)
    {
        setPlaying (false);
        return;
    }
    const auto& c = s.compare;
    if (compareChangedRange)
    {
        if (s.playhead < c.from || s.playhead >= c.to)
            seek (c.from);
    }
    else if (const auto* track = s.project.findTrack (c.track))
    {
        // 曲全体：試聴中のテイクが聞こえる所にいなければ、そのテイクの頭から
        for (auto& k : track->takes)
            if (k.id == c.previewing)
            {
                const auto span = project::usableSpan (k, c.from, c.to);
                if (span.second > span.first && (s.playhead < span.first || s.playhead >= span.second))
                    seek (span.first);
            }
    }
    compareStartedPlay = true;
    setPlaying (true);
}

void UiSession::endTakeCompare (bool commit)
{
    if (! audition.isActive())
        return;
    auto* track = const_cast<project::Track*> (s.project.findTrack (audition.track()));
    const auto type = audition.track();
    const auto from = audition.from(), to = audition.to();
    bool used = false, unchanged = false;
    auto id = audition.previewing();
    bool movedFile = false;
    if (track != nullptr && commit && id.isNotEmpty())
    {
        // 原速のリハーサルを選んだ時は、本番のテイクに移してから決める（移せなければやめる）
        const auto it = std::find_if (track->takes.begin(), track->takes.end(), [&] (const project::Take& k) { return k.id == id; });
        if (it != track->takes.end() && it->recMode == project::RecMode::practice)
        {
            const auto moved = moveRehearsalToTakes (*track, id);
            if (moved.isEmpty())
                commit = false;
            movedFile = moved.isNotEmpty();
            id = moved;
        }
    }
    if (track != nullptr && commit)
    {
        unchanged = id.isNotEmpty();   // 選んだテイクが元からその範囲に入っていた（下で上書き）
        if (auto before = audition.commit (*track))
        {
            unchanged = false;
            // 録音の取り消し（B10）と同じ 1 段の Ctrl / ⌘+Z で、選び直す前の採用区間に戻せる
            compBeforeTake = std::move (*before);
            undoTrack = type;
            undoIsCompare = true;
            s.canUndoTake = true;
            used = true;
        }
    }
    else if (track != nullptr)
        audition.cancel (*track);
    audition = {};

    if (compareStartedPlay && s.isPlaying)
        setPlaying (false);   // 試聴で鳴らした分は止める（比べる前から鳴っていたらそのまま）
    if (compareChangedRange)
    {
        s.rangeIn = compareRangeIn;
        s.rangeOut = compareRangeOut;
        s.loopOn = compareLoopOn;
        syncLoopToEngine();
    }
    compareChangedRange = compareStartedPlay = false;
    s.compare.active = false;
    s.compare.previewing.clear();
    s.compare.original.clear();

    if (movedFile)
        saveProject();   // ファイルはもう Audio/Takes にある：自動保存（1.5 秒後）を待たずに書く（落ちても .vbooth と食い違わない）
    if (used)
        postNotice (tr ("compare.used", trackName (type), id,
                        formatTime (from, s.sampleRate(), true) + " - " + formatTime (to, s.sampleRate(), true), undoKeyName()));
    else if (unchanged)
        postNotice (tr ("compare.unchanged", id));
    notify (change::takes | change::tracks | change::range | change::transport);
}

juce::String UiSession::moveRehearsalToTakes (project::Track& track, const juce::String& takeId)
{
    auto it = std::find_if (track.takes.begin(), track.takes.end(),
                            [&] (const project::Take& k) { return k.id == takeId && k.recMode == project::RecMode::practice; });
    if (it == track.takes.end() || s.projectFolder == juce::File())
        return {};

    // 本番のテイクの所へ（同じ名前があれば番号を進める。録った声は上書きしない）。救済（promoteRehearsalTake）と同じ置き場
    const auto key = juce::String (project::trackKey (track.type));
    auto id = project::nextTakeId (track);
    auto rel = [&] { return "Audio/Takes/" + key + "_" + id + ".wav"; };
    for (int n = id.substring (4).getIntValue(); s.projectFolder.getChildFile (rel()).exists(); )
        id = "take" + juce::String (++n);
    const auto src = s.projectFolder.getChildFile (it->path), dst = s.projectFolder.getChildFile (rel());
    dst.getParentDirectory().createDirectory();
    if (! src.existsAsFile() || ! src.moveFileTo (dst))
    {
        postNotice (tr ("rescue.failed", src.getFileName()));
        return {};
    }

    const auto oldWave = dummy::takeWaveKey (track.type, it->id);
    it->id = id;
    it->path = rel();
    it->recMode = project::RecMode::delivery;
    it->useFrom = it->useTo = -1;
    for (auto& c : track.comp)
        if (c.takeId == takeId)
            c.takeId = id;
    if (s.rescueTakeId == takeId && s.rescueTrack == track.type)
        s.rescueNoticeSerial = -1;   // 救済の「本番に入れる」はもう効かない
    s.takeWaves.erase (oldWave);
    s.takePitch.erase (oldWave);
    loadTakeWave (track.type, *it);
    return id;
}

std::optional<dummy::Session::TakeStats> UiSession::takeStatsIn (project::TrackType type, const juce::String& takeId, int64 from, int64 to) const
{
    const auto key = dummy::takeWaveKey (type, takeId);
    if (! s.engineAttached)
    {
        // 見本（UI_MOCK）：見本の値
        const auto it = s.takeStats.find (key);
        return it != s.takeStats.end() ? std::optional<dummy::Session::TakeStats> (it->second) : std::nullopt;
    }
    const auto it = s.takePitch.find (key);
    if (s.refPitch.empty() || it == s.takePitch.end() || it->second == nullptr || it->second->empty())
        return std::nullopt;
    return statsFor (guideFrames (s.refFor (type)), *it->second, (double) s.sampleRate(), from, to, s.pitchToleranceCents);
}

juce::String undoKeyName()
{
   #if JUCE_MAC
    return juce::String::fromUTF8 ("\xe2\x8c\x98Z");   // ⌘Z
   #else
    return "Ctrl+Z";
   #endif
}

//==============================================================================
void UiSession::updateShadow()
{
    if (! isEngineDriven() || s.isRecording)
        return;

    // 裏で録るのは、再生中で、REC を押せば録れる時だけ（アーム・入力・SR がそろっている）。
    // 練習のテンポ・キーを変えている間は録らない（曲の位置と録った声の長さが合わないので、遡りに使えない。B11）
    const bool want = s.isPlaying && recordProblem().isEmpty() && ! practiceShifted();
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
    exporter::Options eo;
    eo.crossfadeMs = s.crossfadeMs;
    s.exporting = true;
    notify (change::takes);

    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, project, folder, dest, song, types, eo]
    {
        juce::StringArray failed;
        int written = 0;
        for (auto t : types)
        {
            const auto res = exporter::ExportService::exportTrackDry (project, t, folder,
                                                                      dest.getChildFile (exporter::ExportService::dryFileName (song, t)), eo);
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

juce::StringArray UiSession::unrecordedSummary (const std::vector<project::TrackType>& types) const
{
    juce::StringArray lines;
    const auto rate = s.sampleRate();
    // お手本の声の区間：声のある点（信頼度 0.5 以上）を、0.3 秒未満の切れ目はつないでまとめる
    auto voicedSpans = [rate] (const std::vector<dummy::PitchPoint>& ref)
    {
        std::vector<project::Span> spans;
        const auto join = (int64) (0.3 * rate), hop = (int64) (0.02 * rate);
        for (auto& p : ref)
        {
            if (p.confidence < 0.5f)
                continue;
            if (! spans.empty() && p.sample - spans.back().end <= join)
                spans.back().end = p.sample + hop;
            else
                spans.push_back ({ p.sample, p.sample + hop });
        }
        return spans;
    };
    const auto mainSpans = voicedSpans (s.refPitch), harmSpans = voicedSpans (s.refPitchHarm);

    for (auto type : types)
    {
        const bool harm = type == project::TrackType::harm1 || type == project::TrackType::harm2;
        const auto& spans = harm ? harmSpans : mainSpans;   // ダブルはメインと同じ所を歌う
        const auto* track = s.project.findTrack (type);
        if (spans.empty() || track == nullptr)
            continue;
        const auto gaps = project::uncoveredSpans (*track, spans, (int64) rate);   // 1 秒未満の抜けは言わない
        if (gaps.empty())
            continue;
        juce::StringArray ranges;
        for (size_t i = 0; i < gaps.size() && i < 3; ++i)
            ranges.add (formatTime (gaps[i].start, rate, false) + utf8 ("\xe2\x80\x93") + formatTime (gaps[i].end, rate, false));
        auto line = tr ("export.unrecorded.line", trackName (type), ranges.joinIntoString (", "));
        if (gaps.size() > 3)
            line << " " << tr ("export.unrecorded.more", (int) gaps.size() - 3);
        lines.add (line);
    }
    return lines;
}

void UiSession::exportPack (const std::vector<project::TrackType>& types, int bitDepth, bool refmix)
{
    if (s.exporting || s.project.lengthSamples <= 0 || s.projectFolder == juce::File() || types.empty())
        return;

    exporter::PackOptions o;
    o.songName = s.songName;
    o.tracks = types;
    o.bitDepth = bitDepth == 16 || bitDepth == 24 || bitDepth == 32 ? bitDepth : s.project.bitDepthExport;
    o.takeMap = s.mode == project::Mode::pro;   // 標準は WAV + 基本、プロはフルパック（DESIGN 2）
    o.zip = true;
    o.crossfadeMs = s.crossfadeMs;
    if (s.project.key.known())   o.songKey = s.project.key.shortName();
    if (s.project.tempo.known()) o.bpm = s.project.tempo.bpm;

    // 確認用ミックスは「いま聞いている音量」で（オフボのフェーダーと M、トラックの音量・M / S）
    if (refmix && s.songCurrent != nullptr && s.songCurrent->length() == s.project.lengthSamples)
        o.backing = std::shared_ptr<const juce::AudioBuffer<float>> (s.songCurrent, &s.songCurrent->buffer);
    o.backingGain = s.backingMuted ? 0.0f : audio::PlaybackCore::faderToGain (s.offVocalGain);
    bool anySolo = false;
    for (auto& t : s.trackUi) anySolo = anySolo || t.solo;
    for (auto& t : s.trackUi)
        o.vocalGains[t.type] = (! t.mute && (! anySolo || t.solo)) ? audio::PlaybackCore::faderToGain (t.monitorGain) : 0.0f;

    // take_map の区間名（いまの言語で。裏のスレッドから読むので写しを渡す）
    const auto sections = s.project.sections;
    o.sectionAt = [sections] (int64 sample)
    {
        int index = -1;
        for (int i = 0; i < (int) sections.size(); ++i)
            if (sections[(size_t) i].startSample <= sample)
                index = i;
        return index >= 0 ? marks::sectionName (sections, index) : juce::String();
    };

    auto project = s.project;
    const auto folder = s.projectFolder;
    s.exporting = true;
    notify (change::takes);

    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, project, folder, o]
    {
        const auto r = exporter::DeliveryPack::write (project, folder, o);
        juce::MessageManager::callAsync ([this, weak, r]
        {
            if (weak.expired())
                return;
            s.exporting = false;
            if (r.ok) postNotice (tr ("export.pack.done", r.files.size(), r.zipFile.getFullPathName()));
            else      postNotice (tr ("export.failed", r.message));
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
        {
            notify (change::device | change::meter);
            checkDeviceRate();
        }
    }

    pollLatencyProbe();
    updateShadow();
    pollPitch();

    if (stemsDirty && ! s.conforming)
        renderStems();

    // モデルのダウンロードは再生・録音の間は止める（音切れを起こさない。11.7）
    if (modelDownloader != nullptr && modelDownloader->isBusy())
        modelDownloader->setPaused (s.isPlaying || s.isRecording);

    // 自動保存（B14）：変更から 1.5 秒たったら。録音中・SR をそろえている間は待つ
    if (dirty && ! s.isRecording && ! s.conforming && juce::Time::getMillisecondCounter() - dirtySince > 1500)
        saveProject();

    // カウントイン中（2026-10-02）：BAR.BEAT は数えている拍を出す（曲の位置は数え終わるまで動かない）
    const bool counting = s.isPlaying && isEngineDriven() && engine->isCountingIn();
    if (counting)
        s.countInPosition = engine->getCountInPosition();
    if (counting != s.countingIn)
    {
        s.countingIn = counting;
        notify (change::transport | change::playhead);
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
        followPlayhead (seconds);

        // 区間の録り直し（B10）：範囲の終わりの 0.5 秒後で止める（歌い終わりの余韻もファイルに残す）
        if (s.isRecording && s.recordEnd >= 0 && pos >= s.recordEnd + (int64) (0.5 * s.sampleRate()))
        {
            setPlaying (false);
            return;
        }

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

void UiSession::setClick (bool c)
{
    // テンポが分からなければ拍の位置が無い：入れずに理由を知らせる（押して何も起きないキーにしない）
    if (c && ! s.tempoKnown())
    {
        postNotice (tr ("transport.click.noTempo"));
        notify (change::transport);   // キーの見た目を戻す
        return;
    }
    s.clickOn = c;
    notify (change::transport);
}

void UiSession::setClickLevel (float fader)
{
    s.clickLevel = juce::jlimit (0.0f, 1.0f, fader);
    notify (change::monitor);
}

void UiSession::syncClickToEngine()
{
    if (engine == nullptr)
        return;
    // 拍の並びは曲のサンプル（時間軸の SR）。変わった時だけ渡す（渡すと次の拍を探し直す）
    const auto& t = s.project.tempo;
    const auto spb = t.known() ? t.samplesPerBeat (s.sampleRate()) : 0.0;
    const auto perBar = t.signature.beatsPerBar();
    if (! juce::exactlyEqual (spb, sentClickSpb) || perBar != sentClickPerBar || t.downbeatSample != sentClickDownbeat)
    {
        sentClickSpb = spb;
        sentClickPerBar = perBar;
        sentClickDownbeat = t.downbeatSample;
        engine->setClickGrid (spb, perBar, t.downbeatSample);
    }
    engine->setClick (s.clickOn && t.known(), audio::PlaybackCore::faderToGain (s.clickLevel));
}

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
void UiSession::shiftGuideData (int64 d, bool withOriginal)
{
    if (d == 0)
        return;
    for (auto* ref : { &s.refPitch, &s.refPitchHarm })
        for (auto& p : *ref)
            p.sample += d;

    const auto rate = s.sampleRate();
    s.guideVocals = shiftedAudio (s.guideVocals, d, rate);
    s.guideHarmVocals = shiftedAudio (s.guideHarmVocals, d, rate);
    if (withOriginal)
        s.guideOriginal = shiftedAudio (s.guideOriginal, d, rate);
}

bool UiSession::applyGuideNudge()
{
    const auto d = (int64) std::llround (s.guideNudgeMs * 0.001 * s.sampleRate());
    shiftGuideData (d, false);   // 原曲は合わせた時にもう当ててある
    return d != 0;
}

void UiSession::nudgeGuide (double deltaMs)
{
    if (s.refPitch.empty() && s.guideVocals == nullptr)
        return;
    const auto rate = s.sampleRate();
    const auto oldMs = s.guideNudgeMs;
    const auto newMs = juce::jlimit (-500.0, 500.0, oldMs + deltaMs);
    const auto d = (int64) std::llround (newMs * 0.001 * rate) - (int64) std::llround (oldMs * 0.001 * rate);
    s.guideNudgeMs = newMs;
    shiftGuideData (d);
    rejudgeAll();
    updateTakeStats();
    syncGuideToEngine();
    markDirty();
    notify (change::takes | change::monitor | change::view);
    postNotice (tr ("guide.nudged", (newMs > 0 ? "+" : "") + juce::String (newMs, 0)));
}

void UiSession::setGuideCovered (const std::vector<std::pair<int64, int64>>& spans, double songRate)
{
    // オフボの元の SR → いまの時間軸の SR
    const auto ratio = songRate > 0.0 ? s.sampleRate() / songRate : 1.0;
    s.guideCovered.clear();
    for (auto& [a, b] : spans)
        s.guideCovered.push_back ({ (int64) std::llround ((double) a * ratio), (int64) std::llround ((double) b * ratio) });
}

void UiSession::setListenOriginal (bool on)
{
    on = on && s.guideOriginal != nullptr;
    if (s.listenOriginal == on)
        return;
    s.listenOriginal = on;
    syncBackingLevel();
    syncGuideGain();
    notify (change::monitor | change::view);
}

void UiSession::alignGuideAt (int64 sample)
{
    if (s.guideOriginal == nullptr || s.songOriginal == nullptr || s.sampleRate() <= 0.0 || s.guideBusy)
        return;
    const auto original = s.guideOriginal, karaoke = s.songOriginal;
    const auto audioRate = original->sampleRate;
    const auto centre = (int64) std::llround ((double) sample * audioRate / s.sampleRate());   // 原曲・カラオケは元の SR で持っている
    const auto window = (int64) (1.5 * audioRate), maxLag = (int64) (0.25 * audioRate);
    const auto serial = s.songSerial;
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, original, karaoke, audioRate, centre, window, maxLag, serial]
    {
        // 比べる所だけ切り出す（モノラル）
        const auto lo = juce::jmax ((int64) 0, centre - window - maxLag - 64);
        const auto hi = juce::jmin (karaoke->length(), centre + window + maxLag + 64);
        std::vector<float> a, b;
        for (auto i = lo; i < hi; ++i)
        {
            a.push_back (i < original->length() ? original->buffer.getSample (0, (int) i) : 0.0f);
            float m = 0.0f;
            for (int ch = 0; ch < karaoke->buffer.getNumChannels(); ++ch)
                m += karaoke->buffer.getSample (ch, (int) i);
            b.push_back (m / (float) juce::jmax (1, karaoke->buffer.getNumChannels()));
        }
        const auto r = analysis::localLag (a.data(), (juce::int64) a.size(), b.data(), (juce::int64) b.size(), centre - lo, window, maxLag);
        juce::MessageManager::callAsync ([this, weak, r, audioRate, serial]
        {
            if (weak.expired() || serial != s.songSerial)
                return;
            if (! r.found || r.correlation < 0.3)
            {
                postNotice (tr ("guide.alignHere.failed"));
                return;
            }
            // 原曲の中身が lag だけ遅れている → お手本をその分前へ
            const auto ms = -(double) r.lag * 1000.0 / audioRate;
            const auto rounded = std::round (ms);
            const auto now = [this] { return (s.guideNudgeMs > 0 ? "+" : "") + juce::String (s.guideNudgeMs, 0); };
            if (std::abs (rounded) < 1.0)
            {
                postNotice (tr ("guide.alignHere.same", now()));   // ずれていない（1 ms 未満）
                return;
            }
            nudgeGuide (rounded);
            postNotice (tr ("guide.alignHere.done", (rounded > 0 ? "+" : "") + juce::String (rounded, 0), now()));
        });
    });
}

void UiSession::resetGuideNudge()
{
    nudgeGuide (-s.guideNudgeMs);
}

void UiSession::setView (int64 start, int64 end)
{
    s.viewStart = start;
    s.viewEnd = end;
    notify (change::view);
}

void UiSession::zoomView (int64 anchor, double factor)
{
    const auto length = s.project.lengthSamples;
    const auto span = s.viewEnd - s.viewStart;
    if (length <= 0 || span <= 0)
        return;

    const auto minSpan = juce::jmin (length, (int64) (2.0 * s.sampleRate()));
    const auto next = juce::jlimit (minSpan, length, (int64) std::llround ((double) span * factor));
    if (next == span)
        return;

    anchor = juce::jlimit (s.viewStart, s.viewEnd, anchor);
    const auto k = (double) (anchor - s.viewStart) / (double) span;
    const auto start = juce::jlimit ((int64) 0, length - next, anchor - (int64) std::llround (k * (double) next));
    setView (start, start + next);
}

void UiSession::scrollView (double fraction)
{
    const auto length = s.project.lengthSamples;
    const auto span = s.viewEnd - s.viewStart;
    if (length <= 0 || span <= 0)
        return;

    const auto start = juce::jlimit ((int64) 0, juce::jmax ((int64) 0, length - span),
                                     s.viewStart + (int64) std::llround (fraction * (double) span));
    if (start != s.viewStart)
        setView (start, start + span);
}

void UiSession::setCrossfade (double ms)
{
    ms = juce::jlimit (0.0, 50.0, ms);
    if (juce::exactlyEqual (s.crossfadeMs, ms)) return;
    s.crossfadeMs = ms;
    stemsDirty = true;   // 試聴のトラックも同じ継ぎ目で作り直す（renderStems が変わった物だけ）
    notify (change::mode);   // 設定に保存する
}

void UiSession::setShowLyrics (bool b)
{
    if (s.showLyrics == b) return;
    s.showLyrics = b;
    if (! b) s.lyricSyncing = false;
    notify (change::mode);   // 画面の並びが変わる（設定にも保存する）
}

void UiSession::setOctaveAlign (bool b) { s.octaveAlign = b; rejudgeAll(); notify (change::view); }
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

void UiSession::setTrackGain (int i, float fader)
{
    if (! juce::isPositiveAndBelow (i, (int) s.trackUi.size()))
        return;
    s.trackUi[(size_t) i].monitorGain = juce::jlimit (0.0f, 1.0f, fader);
    notify (change::tracks);
}

//==============================================================================
namespace
{
    constexpr project::TrackType stemTypes[] { project::TrackType::main, project::TrackType::doubleTrack,
                                               project::TrackType::harm1, project::TrackType::harm2 };
}

void UiSession::renderStems()
{
    stemsDirty = false;
    if (engine == nullptr || s.project.lengthSamples <= 0)
        return;

    // 採用区間・テイクが変わったトラックだけ作り直す（書き出しと同じ計算。exporter::renderTrackDry）
    std::vector<int> slots;
    for (int k = 0; k < 4; ++k)
    {
        juce::String sig;
        if (const auto* t = s.project.findTrack (stemTypes[k]))
        {
            for (auto& c : t->comp)
                sig << c.takeId << ':' << c.startSample << '-' << c.endSample << ';';
            for (auto& tk : t->takes)
                sig << tk.id << '=' << tk.path << '@' << tk.startSample << ';';
        }
        sig << '|' << s.project.lengthSamples << '|' << s.project.sampleRate << '|' << s.crossfadeMs;
        if (sig != stemSignature[k])
        {
            stemSignature[k] = sig;
            slots.push_back (k);
        }
    }
    if (slots.empty())
        return;

    const auto generation = ++stemGeneration;
    for (auto k : slots)
        stemSlotGeneration[k] = generation;
    const auto project = s.project;
    const auto folder = s.projectFolder;
    exporter::Options eo;
    eo.crossfadeMs = s.crossfadeMs;
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, generation, project, folder, slots, eo]
    {
        using Buffer = std::shared_ptr<const juce::AudioBuffer<float>>;
        auto made = std::make_shared<std::vector<std::pair<int, Buffer>>>();
        for (auto k : slots)
        {
            Buffer b;
            const auto* t = project.findTrack (stemTypes[k]);
            if (t != nullptr && ! t->comp.empty())
            {
                auto buf = std::make_shared<juce::AudioBuffer<float>>();
                if (exporter::ExportService::renderTrackDry (project, stemTypes[k], folder, *buf, eo).ok)
                    b = std::move (buf);
            }
            made->push_back ({ k, std::move (b) });
        }
        juce::MessageManager::callAsync ([this, weak, generation, made]
        {
            if (weak.expired() || engine == nullptr)
                return;
            for (auto& [k, b] : *made)
                if (stemSlotGeneration[k] == generation)   // そのトラックを後で作り直していれば、古い方は捨てる
                    engine->setVocalStem (k, b);
            syncStemGains();
        });
    });
}

void UiSession::syncStemGains()
{
    if (engine == nullptr)
        return;
    bool anySolo = false;
    for (auto& t : s.trackUi)
        anySolo = anySolo || t.solo;
    for (int k = 0; k < 4; ++k)
    {
        float gain = 0.0f;
        for (auto& t : s.trackUi)
            if (t.type == stemTypes[k])
            {
                // 録っているトラックは鳴らさない（自分の声だけを聞く。録り直す前の声と重ならない）
                const bool recordingHere = s.isRecording && s.recordingTrack == t.type;
                const bool audible = ! t.mute && (! anySolo || t.solo) && ! recordingHere && isTrackVisible (t.type)
                                  && ! anyMonitorSolo();   // モニターの S（お手本・オフボ・自分だけを聴く）
                gain = audible ? audio::PlaybackCore::faderToGain (t.monitorGain) : 0.0f;
            }
        engine->setVocalGain (k, gain);
    }
    syncGuideGain();
}

//==============================================================================
// お手本の声を聴く（2026-10-02）：取り出した声を、録ったトラックと同じ経路（練習のテンポ・キーも掛かる）で鳴らす
void UiSession::syncBackingLevel()
{
    if (engine == nullptr) return;
    // モニターの S はそれだけを鳴らす：ほかの S が点いていればオフボは止める
    const bool soloedAway = anyMonitorSolo() && ! s.backingSolo;
    engine->setBackingLevel (s.offVocalGain, s.backingMuted || soloedAway || (s.listenOriginal && s.guideOriginal != nullptr));
    syncGuideGain();   // 原曲で聴く時は、原曲がオフボの音量・M・S に従う
}

void UiSession::syncGuideGain()
{
    if (engine == nullptr) return;
    const bool audible = s.guideVocals != nullptr && ! s.guideMuted && (! anyMonitorSolo() || s.guideSolo);
    engine->setVocalGain (audio::PlaybackCore::guideSlot, audible ? audio::PlaybackCore::faderToGain (s.mainGain) : 0.0f);
    const bool harmAudible = s.guideHarmVocals != nullptr && ! s.guideHarmMuted && (! anyMonitorSolo() || s.guideHarmSolo);
    engine->setVocalGain (audio::PlaybackCore::harmGuideSlot, harmAudible ? audio::PlaybackCore::faderToGain (s.harmonyGain) : 0.0f);
    // 原曲で聴く：オフボの代わりに、オフボと同じ音量・M・S で
    const bool originalAudible = s.listenOriginal && s.guideOriginal != nullptr && ! s.backingMuted && (! anyMonitorSolo() || s.backingSolo);
    engine->setVocalGain (audio::PlaybackCore::originalSlot, originalAudible ? audio::PlaybackCore::faderToGain (s.offVocalGain) : 0.0f);
}

void UiSession::syncGuideToEngine()
{
    if (engine == nullptr) return;
    const auto rate = s.sampleRate();
    const auto generation = ++guideGeneration;
    // お手本（リード、または分けていない声）とハモリのお手本。録音形式で SR をそろえている時は裏でそろえてから鳴らす
    const std::pair<int, std::shared_ptr<const audio::SongAudio>> stems[] = { { audio::PlaybackCore::guideSlot, s.guideVocals },
                                                                             { audio::PlaybackCore::harmGuideSlot, s.guideHarmVocals },
                                                                             { audio::PlaybackCore::originalSlot, s.guideOriginal } };
    for (auto& [slot, guide] : stems)
    {
        if (guide == nullptr || rate <= 0.0)
        {
            engine->setVocalStem (slot, nullptr);
            continue;
        }
        if (std::abs (guide->sampleRate - rate) < 0.5)
        {
            engine->setVocalStem (slot, std::shared_ptr<const juce::AudioBuffer<float>> (guide, &guide->buffer));
            continue;
        }
        std::weak_ptr<bool> weak = alive;
        juce::Thread::launch ([this, weak, guide, rate, generation, slot = slot]
        {
            std::shared_ptr<const audio::SongAudio> r = audio::resampleSong (*guide, rate);
            juce::MessageManager::callAsync ([this, weak, r, generation, slot]
            {
                if (weak.expired() || engine == nullptr || generation != guideGeneration) return;
                engine->setVocalStem (slot, r != nullptr ? std::shared_ptr<const juce::AudioBuffer<float>> (r, &r->buffer) : nullptr);
                syncGuideGain();
            });
        });
    }
    syncGuideGain();
}

void UiSession::setGuideLevel (float fader)
{
    s.mainGain = juce::jlimit (0.0f, 1.0f, fader);
    syncGuideGain();
    notify (change::monitor);
}

void UiSession::setGuideMuted (bool m)   { s.guideMuted = m; syncGuideGain(); notify (change::monitor); }

void UiSession::setHarmGuideLevel (float fader)
{
    s.harmonyGain = juce::jlimit (0.0f, 1.0f, fader);
    syncGuideGain();
    notify (change::monitor);
}

void UiSession::setHarmGuideMuted (bool m) { s.guideHarmMuted = m; syncGuideGain(); notify (change::monitor); }

void UiSession::setHarmGuideSolo (bool on)
{
    s.guideHarmSolo = on;
    if (on) s.guideSolo = s.backingSolo = s.selfSolo = false;
    syncBackingLevel();
    syncStemGains();
    notify (change::monitor);
}

void UiSession::setGuideSolo (bool on)
{
    s.guideSolo = on;
    if (on) s.backingSolo = s.selfSolo = s.guideHarmSolo = false;
    syncBackingLevel();
    syncStemGains();
    notify (change::monitor);
}

void UiSession::setBackingSolo (bool on)
{
    s.backingSolo = on;
    if (on) s.guideSolo = s.selfSolo = s.guideHarmSolo = false;
    syncBackingLevel();
    syncStemGains();
    notify (change::monitor);
}

void UiSession::setSelfSolo (bool on)
{
    // 自分の声のモニター（入力 → ヘッドホン）はエンジンの別の経路なので、ここでは他を止めるだけ（自分の量・M はそのまま）
    s.selfSolo = on;
    if (on) s.guideSolo = s.backingSolo = s.guideHarmSolo = false;
    syncBackingLevel();
    syncStemGains();   // お手本（syncGuideGain）もここで
    notify (change::monitor);
}

//==============================================================================
// 声域とおすすめのキー（2026-10-02。DESIGN 18.1）
void UiSession::setVoiceRange (int low, int high)
{
    if (low > high) std::swap (low, high);
    s.voiceLow = juce::jlimit (-1, 108, low);
    s.voiceHigh = juce::jlimit (-1, 108, high);
    notify (change::practice);
}

analysis::KeySuggestion UiSession::keySuggestion() const
{
    return analysis::suggestKey (guideRange(), s.voiceLow, s.voiceHigh);
}

analysis::SongRange UiSession::guideRange() const
{
    // お手本の広がりは点が変わった時だけ数え直す
    const auto key = (juce::int64) s.refPitch.size() * 1000003 + (s.refPitch.empty() ? 0 : s.refPitch.front().sample);
    if (key != songRangeKey)
    {
        std::vector<float> midi;   // 10 ms ごとの並び（時間の飛び・自信の無い点は無声として切る）
        midi.reserve (s.refPitch.size());
        const auto hop = (int64) (audio::pitch::hopSeconds * s.sampleRate() * 1.5);
        for (size_t i = 0; i < s.refPitch.size(); ++i)
        {
            const auto& p = s.refPitch[i];
            if (i > 0 && p.sample - s.refPitch[i - 1].sample > hop)
                midi.push_back (0.0f);
            midi.push_back (p.confidence >= 0.5f ? p.midi : 0.0f);
        }
        cachedSongRange = analysis::songRange (analysis::sustainedNotes (midi));
        songRangeKey = key;
    }
    return cachedSongRange;
}

void UiSession::applySuggestedKey()
{
    const auto k = keySuggestion();
    if (! k.ok || deliveryLocked()) return;
    if (k.octave != 0 && ! s.octaveAlign)
        setOctaveAlign (true);   // 1 オクターブ下（上）で歌う：同じ音として比べる
    setKey (k.shift);
}

void UiSession::startRangeMeasure()
{
    if (engine == nullptr) return;
    if (s.isPlaying) setPlaying (false);
    rangeNotes.clear();
    rangeMeasuring = true;
    engine->setPitchFreeRun (true);
}

void UiSession::stopRangeMeasure()
{
    rangeMeasuring = false;
    if (engine != nullptr) engine->setPitchFreeRun (false);
}

void UiSession::setMute (int i, bool m) { if (juce::isPositiveAndBelow (i, (int) s.trackUi.size())) { s.trackUi[(size_t) i].mute = m; notify (change::tracks); } }
void UiSession::setSolo (int i, bool v) { if (juce::isPositiveAndBelow (i, (int) s.trackUi.size())) { s.trackUi[(size_t) i].solo = v; notify (change::tracks); } }

//==============================================================================
void UiSession::setTempo (int p)              { setPractice (p, s.keyShift); }
void UiSession::setKey (int k)                { setPractice (s.tempoPercent, k); }

void UiSession::setPractice (int tempoPercent, int keyShift)
{
    tempoPercent = juce::jlimit (50, 150, tempoPercent);
    keyShift = juce::jlimit (-6, 6, keyShift);
    if (tempoPercent == s.tempoPercent && keyShift == s.keyShift)
    {
        notify (change::practice);   // つまみの表示を今の値に戻す
        return;
    }
    // 録音中は変えない（テイクの途中で速さが変わると、曲の位置に戻せない。B11）
    if (s.isRecording && isEngineDriven())
    {
        postNotice (tr ("practice.lockedRecording"));
        notify (change::practice);
        return;
    }
    s.tempoPercent = tempoPercent;
    s.keyShift = keyShift;
    // 裏録り（B7）は原速・原キーで鳴らした所だけ使える。変えたら消す（原速に戻れば次から録り直す）
    if (shadowActive && ! s.isRecording)
        finishRecording();
    syncPracticeToEngine();
    notify (change::practice);
}
void UiSession::setRecMode (project::RecMode m) { s.recMode = m; notify (change::practice); }
void UiSession::setPitchTolerance (float c)   { s.pitchToleranceCents = c; updateTakeStats(); notify (change::view); }

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
