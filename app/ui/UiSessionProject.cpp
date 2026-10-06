#include "UiSession.h"
#include "system/Background.h"
#include "audio/SongLoader.h"
#include "audio/Resample.h"
#include "analysis/MusicInfo.h"
#include "analysis/AnalysisCache.h"
#include "project/Comp.h"
#include "Timeline.h"

/*  プロジェクトを開く・保存する（B14。2026-10-04 に UiSession.cpp から分けた。動きは変えていない）
    曲を開く（loadSong）・続きから戻す（restoreProject）・保存（saveProject・flushSave・自動保存の印）・
    プロジェクトへのコピー・録音形式と伴奏の SR をそろえる（conformSong）。
    確かめるテスト：tests/session/SessionSaveTests.cpp（VoiceBoothSessionTests） */

namespace vb
{
//==============================================================================
void UiSession::loadSong (const juce::File& file, int sampleRate, int64 lengthSamples,
                          std::shared_ptr<const audio::WaveformOverview> wave,
                          std::shared_ptr<const audio::SongAudio> audio)
{
    finishRecording();   // 録音中に別の曲を開いたら、そこまでのテイクは残す
    stopSeparation();    // 前の曲の分離は止める（B16）
    endTakeCompare (false);   // テイク比較の試聴中なら元の採用区間に戻す（B18c。範囲・ループも元へ）
    s.canUndoTake = false;   // 前の曲のテイクの採用を、次の曲のプロジェクトで戻さない
    leadSplitAgreed = false; // ハモリ分けの了承（agreeLeadSplit）は、そのあと読むお手本の分だけ
    flushSave();         // 前の曲のプロジェクトを保存してから
    // 保存した長さ（プロジェクトの SR）と同じ曲か：そろえるときと同じ計算で SR を変えて比べる（前は元の SR に戻して 2 サンプルで比べ、
    // SR を下げてそろえた曲は丸めの差が広がって別の曲と判断し、録ったテイクのない「(2)」のプロジェクトで開いていた。バグチェック 2026-10-06）
    auto sameLength = [&] (int64 savedLength, double savedRate)
    {
        return savedRate > 0.0 && std::llabs (audio::resampledLength (lengthSamples, (double) sampleRate, savedRate) - savedLength) <= 2;
    };
    saveFailed = false;  // 前の曲の保存の失敗を持ち越さない（次の曲で保存できなくても知らせが出なかった。バグチェック 2026-10-05）
    // 開く途中のプロジェクトが、いま開く曲のものでなければ捨てる（読み込みをやめた・失敗した後に別の曲を開いた時、
    // 前のプロジェクトのフォルダに別の曲を開いて上書きしていた。監査 2026-10-04）
    if (pendingProject != nullptr)
    {
        const auto& lp = pendingProject->project;
        if (! juce::File (lp.songPath).getFileName().equalsIgnoreCase (file.getFileName()) || ! sameLength (lp.lengthSamples, (double) lp.sampleRate))
            clearPendingProject();
        // 今のプロジェクトを開き直す時は、いま保存した中身で戻す（開く画面を出した時に読んだ中身では、その後に録ったテイク・
        // 変えた所が古い中身で上書きされていた。監査 2026-10-04）
        else if (pendingProjectFile == s.projectFile && pendingProjectFile.existsAsFile())
            if (auto fresh = project::fromJson (pendingProjectFile.loadFileAsString()); fresh.ok)
                *pendingProject = std::move (fresh);
    }
    awaitingRestore = false;
    timelineLocked = false;
    // 曲の中身のハッシュ（1 ch 目。数十 ms）。同じ名前・同じ長さの別の曲（キー違い・ミックスを直した差し替え）を、前のプロジェクトとして開かない
    songHash = audio != nullptr && audio->buffer.getNumChannels() > 0
                 ? juce::String::toHexString ((juce::int64) analysis::cache::hashSamples (audio->buffer.getReadPointer (0), audio->buffer.getNumSamples()))
                 : juce::String();
    s = dummy::makeSongSession (s, file.getFileNameWithoutExtension(), file.getFullPathName(),
                                sampleRate, lengthSamples, std::move (wave));
    s.songRate = sampleRate;
    lastBackupMs = 0;

    // 大きい曲（高い SR・長い曲）：メモリが足りなくなるおそれを先に知らせる（断りはしない。#21）
    if (audio != nullptr)
    {
        const auto need = audio::estimatedMemoryBytes (lengthSamples, audio->buffer.getNumChannels());
        const auto ramMB = juce::SystemStats::getMemorySizeInMegabytes();
        if (audio::memoryTight (need, ramMB))
            postNotice (tr ("load.memoryTight", juce::String ((double) need / (1024.0 * 1024.0 * 1024.0), 1),
                            juce::String ((double) ramMB / 1024.0, 0)));
    }

    // プロジェクトフォルダ（B14）：.vbooth から開いたらそのフォルダ。曲ファイルから開いたら Projects/{曲名}/。
    // 同じ名前のプロジェクトがあり、同じ曲（ファイル名と長さ）なら続きから開く。違う曲なら「{曲名} (2)」…の別のフォルダ
    if (pendingProject != nullptr)
    {
        s.projectFile = pendingProjectFile;
        s.projectFolder = pendingProjectFile.getParentDirectory();
        awaitingRestore = true;   // 中身を戻すまでは保存しない（空のプロジェクトで上書きしないため）
    }
    else
    {
        auto folder = projectFolderFor (s.songName);
        auto sameSong = [&] (const juce::File& vbooth)
        {
            auto l = project::fromJson (vbooth.loadFileAsString());
            if (! l.ok || l.project.sampleRate <= 0)
                return false;
            if (juce::File (l.project.songPath).getFileName() != file.getFileName() || ! sameLength (l.project.lengthSamples, (double) l.project.sampleRate))
                return false;
            if (l.extras.songHash.isNotEmpty() && songHash.isNotEmpty() && l.extras.songHash != songHash)
                return false;   // 名前と長さは同じでも別の音（監査 2026-10-04）
            pendingProject = std::make_unique<project::LoadedProject> (std::move (l));
            awaitingRestore = true;
            return true;
        };
        // 前の名前の付け方のフォルダに同じ曲のプロジェクトがあれば、それを続きから開く
        if (const auto legacy = legacyProjectFolderFor (s.songName); legacy != folder)
            if (const auto f = legacy.getChildFile (legacy.getFileName() + project::fileExtension); f.existsAsFile() && sameSong (f))
                folder = legacy;
        for (int n = 2; n < 10000; ++n)
        {
            const auto f = folder.getChildFile (folder.getFileName() + project::fileExtension);
            if (! f.exists() || sameSong (f))
                break;
            folder = projectFolderFor (s.songName, n);
        }
        s.projectFolder = folder;
        s.projectFile = folder.getChildFile (folder.getFileName() + project::fileExtension);
    }
    analysis::cache::setFolder (s.projectFolder.getChildFile ("Cache/analysis"));   // 解析の結果の保存先（開き直しで解析し直さない）
    // 前に分離の途中でアプリを閉じた・落ちたときの作業ファイル（入力の写し・書きかけ。1 本で数十 MB）を削除する（監査 2026-10-04）。
    // 分離の結果（vocals・backing・lead）は残す
    if (s.projectFolder != juce::File())
        for (auto* sub : { "Cache/separation", "Cache/lead" })
            for (auto& dir : s.projectFolder.getChildFile (sub).findChildFiles (juce::File::findDirectories, false))
                for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.part;mix.wav;rest.wav"))
                    f.deleteFile();

    // 曲はプロジェクトの中にコピーして持つ（持ち運べるように。元のファイルはそのまま）
    if (file.isAChildOf (s.projectFolder))
        s.project.songPath = file.getRelativePathFrom (s.projectFolder).replaceCharacter ('\\', '/');
    else
        copyIntoProject (file, "Audio/" + file.getFileName(), CopyTarget::song);
    // 前に落ちた時の裏録りの残り（B7）を片付ける。消さない：使っているテイクはそのまま、ほかは Audio/Recovered/ へ
    // （REC を押した後に落ちると、歌った声が .retro- の名前で残る。前は開くたびに全部消していた。監査 2026-10-03）
    {
        juce::StringArray used;
        if (pendingProject != nullptr)
            for (auto& t : pendingProject->project.tracks)
                for (auto& k : t.takes)
                    used.add (k.path);
        if (const auto moved = project::recoverUnusedTakes (s.projectFolder, used); moved > 0)
        {
            if (pendingProject != nullptr)
                recoveredTakes = moved;   // 「続きから開きました」の後に知らせる（上書きされないように。restoreProject）
            else
                postNotice (tr ("project.recovered", moved));
        }
    }
    s.songOriginal = audio;
    s.songCurrent = audio;

    if (engine != nullptr)
    {
        engine->setSong (std::move (audio));   // 止まって頭へ。出力の SR を曲に合わせる（録ったトラックも外れる）
        { const auto g = ++stemGeneration; for (auto& sg : stemSlotGeneration) sg = g; }   // 前の曲で作りかけのトラックの音を入れない（監査 2026-10-04）
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
        // テイクがあるプロジェクトは、保存した時の時間軸の SR のまま開く（テイクの位置と声の SR がその SR。
        // 機器の SR や録音形式の変更で別の SR にそろえると、位置が 8.8% などずれたまま保存されていた。監査 2026-10-04）
        const auto& lp = pendingProject->project;
        const bool withTakes = std::any_of (lp.tracks.begin(), lp.tracks.end(), [] (const project::Track& tr) { return ! tr.takes.empty(); });
        if (withTakes && lp.sampleRate > 0)
        {
            s.recordRate = lp.sampleRate == s.songRate ? 0.0 : (double) lp.sampleRate;
            s.deviceFallbackRate = 0;
            timelineLocked = true;
        }
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
    if (! s.projectFile.existsAsFile())
        markDirty();   // 新しい曲はすぐに .vbooth を作る（続きから開いた時は、何か変えるまで書かない）
}

//==============================================================================
void UiSession::markDirty()
{
    // 出力の機器が閉じていても（抜けた・選んでいない）保存は止めない（監査 2026-10-04：録音中に機器が抜けると、
    // できたテイクが .vbooth に入らず、次に開くと Recovered へ移っていた）。見本の画面（エンジンなし）では保存しない
    if (engine == nullptr || restoring)
        return;
    dirty = true;
    dirtySince = juce::Time::getMillisecondCounter();
}

void UiSession::setPendingProject (const juce::File& vboothFile, const project::LoadedProject& p)
{
    pendingProject = std::make_unique<project::LoadedProject> (p);
    pendingProjectFile = vboothFile;
}

void UiSession::clearPendingProject()
{
    pendingProject.reset();
    pendingProjectFile = juce::File();
}

void UiSession::restoreProject()
{
    if (pendingProject == nullptr)
        return;
    const auto loaded = std::move (pendingProject);
    pendingProjectFile = juce::File();
    awaitingRestore = false;
    const auto& lp = loaded->project;
    restoring = true;

    // テイク・採用区間（ファイルはプロジェクトフォルダ相対）。フォルダの外を指すテイク（../・絶対パス）は使わない（#17）
    int takes = 0, outside = 0, missing = 0;
    for (auto t : lp.tracks)
    {
        juce::StringArray dropped;
        for (auto it = t.takes.begin(); it != t.takes.end();)
            if (project::isInsideProject (it->path))
                ++it;
            else
            {
                dropped.add (it->id);
                it = t.takes.erase (it);
            }
        t.comp.erase (std::remove_if (t.comp.begin(), t.comp.end(), [&] (const project::CompSegment& c) { return dropped.contains (c.takeId); }),
                      t.comp.end());
        outside += dropped.size();
        // 壊れた・手で直した .vbooth：無いテイクを指す区間・長さのない区間は捨て、重なりは前の区間を優先して詰める
        // （前はそのまま使い、トラック全体が鳴らない・書き出せない、重なった所が二重に鳴った。バグチェック 2026-10-05）
        t.comp.erase (std::remove_if (t.comp.begin(), t.comp.end(), [&] (const project::CompSegment& c)
                      {
                          return c.endSample <= c.startSample
                              || std::none_of (t.takes.begin(), t.takes.end(), [&] (const project::Take& k) { return k.id == c.takeId; });
                      }),
                      t.comp.end());
        std::stable_sort (t.comp.begin(), t.comp.end(), [] (const project::CompSegment& a, const project::CompSegment& b) { return a.startSample < b.startSample; });
        for (size_t i = 1; i < t.comp.size();)
        {
            t.comp[i].startSample = std::max (t.comp[i].startSample, t.comp[i - 1].endSample);
            if (t.comp[i].endSample <= t.comp[i].startSample)
                t.comp.erase (t.comp.begin() + (long) i);
            else
                ++i;
        }

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
            else if (std::any_of (t.comp.begin(), t.comp.end(), [&] (const project::CompSegment& c) { return c.takeId == k.id; }))
                ++missing;   // 採用しているテイクのファイルが無い：そのトラックは鳴らず、書き出せない（黙っていた。バグチェック 2026-10-05）
        }
    }
    if (outside > 0)
        postNotice (tr ("project.outsideTakes", outside));
    if (missing > 0)
        postNotice (tr ("project.missingTakeFiles", missing));

    // 曲の情報（推定のままの値も戻す。あとから届く自動推定は確定した値を上書きしない）。
    // 保存した時と時間軸の SR が違えば（テイクのないプロジェクトで、録音形式の SR が変わった等）、位置を今の SR に直してから戻す
    // （前はそのまま戻し、歌詞・区間・拍がずれたまま保存されていた。監査 2026-10-04）
    const double toNow = lp.sampleRate > 0 ? (double) s.sampleRate() / (double) lp.sampleRate : 1.0;
    auto scaled = [toNow] (int64 v) { return v >= 0 ? (int64) std::llround ((double) v * toNow) : v; };
    if (lp.tempo.known())
    {
        s.project.tempo = lp.tempo;
        s.project.tempo.downbeatSample = scaled (s.project.tempo.downbeatSample);
        for (auto& b : s.project.tempo.beats) b = scaled (b);
    }
    if (lp.key.known())   s.project.key = lp.key;
    s.project.sections = lp.sections;
    for (auto& sec : s.project.sections) sec.startSample = scaled (sec.startSample);
    s.project.lyrics = lp.lyrics;
    for (auto& l : s.project.lyrics.lines) { l.startSample = scaled (l.startSample); l.endSample = scaled (l.endSample); }
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

    // モニターの音量と M、作業の続き（2026-10-04。古いファイルには無い：既定のまま）
    if (const auto& m = loaded->extras.monitor; m.has)
    {
        setBackingLevel (m.backing);
        setBackingMuted (m.backingMute);
        setGuideLevel (m.guide);
        setGuideMuted (m.guideMute);
        setHarmGuideLevel (m.harmony);
        setHarmGuideMuted (m.harmonyMute);
        setSelfMonitorLevel (m.self);
        setMonitorReverb (m.reverb);
    }
    if (const auto& w = loaded->extras.work; w.has)
    {
        if (w.rangeIn >= 0 && w.rangeOut > w.rangeIn && scaled (w.rangeOut) <= s.project.lengthSamples)
            setRange (scaled (w.rangeIn), scaled (w.rangeOut));
        setLoop (w.loop);
        for (int i = 0; i < (int) s.trackUi.size(); ++i)
            if (w.track == project::trackKey (s.trackUi[(size_t) i].type))
                selectTrack (i);   // いまのモードで見えないトラックなら選ばない（selectTrack が見る）
        setOctaveUp (w.octaveUp);
        if (w.playhead > 0 && scaled (w.playhead) < s.project.lengthSamples)
            seek (scaled (w.playhead));
    }

    if (lp.sampleRate != s.sampleRate())
    {
        // テイクの位置は保存した時の SR のサンプル。そろえられなかった（伴奏の SR 変換に失敗した）まま保存すると、
        // ずれた位置で上書きしてしまうので、この曲では保存も録音もしない（.vbooth は前のまま残る。監査 2026-10-04）
        if (takes > 0)
        {
            awaitingRestore = true;
            postNotice (tr ("project.rateChangedNotSaved", formatKhz (lp.sampleRate), formatKhz (s.sampleRate())));
        }
        // テイクが無ければ位置を今の SR に直して戻したので、知らせることはない
    }
    else if (takes > 0)
        postNotice (tr ("project.resumed", takes));
    if (recoveredTakes > 0)
    {
        postNotice (tr ("project.recovered", recoveredTakes));
        recoveredTakes = 0;
    }

    restoring = false;
    notify (change::takes | change::tracks | change::songInfo | change::view);

    // お手本（声入りの原曲）はもう一度合わせ直す（結果は知らせで）。時間合わせと RMVPE の線は Cache/analysis/ に保存した結果を使うので数秒
    s.guideNudgeMs = loaded->extras.guideNudgeMs;   // お手本の位置の手直し（合わせ直した後に当てる）
    // お手本の場所は、読めるかどうかに関係なく覚えておく（ファイルが見つからない・機器が閉じている時に、
    // 次の保存でお手本の記録が消えていた。監査 2026-10-04）
    s.guidePath = loaded->extras.guidePath;
    if (loaded->extras.guidePath.isNotEmpty())
    {
        const auto guide = project::findMedia (s.projectFolder, loaded->extras.guidePath, "Audio/Guide");
        if (guide.existsAsFile())
            loadGuide (guide);
        else
            postNotice (tr ("guide.missing", juce::File (loaded->extras.guidePath).getFileName()));
    }
}

void UiSession::copyIntoProject (const juce::File& source, const juce::String& relativePath, CopyTarget target)
{
    auto pathOf = [this] (CopyTarget c) -> juce::String& { return c == CopyTarget::song ? s.project.songPath : s.guidePath; };

    const auto dest = s.projectFolder.getChildFile (relativePath);

    // コピーが終わるまでは元のファイルを指しておく。コピーに失敗した・途中で終えた時も、プロジェクトは開ける
    // （前は先にコピー先を書いていたので、ディスクが一杯だと次から開けなくなっていた。監査 2026-10-03）
    const auto original = source.getFullPathName();
    pathOf (target) = original;

    // 裏で比べてコピー（数十 MB）。同じ名前のものが既にあっても、中身が同じ時だけそれを使う。違えば上書きせず「名前 (2)」でコピーする
    // （前は大きさだけで比べていて、同じ名前で書き出し直したオフボが入らず、開き直すと古い音に戻っていた。監査 2026-10-04）
    const auto serial = s.songSerial;
    const auto folder = s.projectFolder;
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, serial, source, dest, folder, target, original, pathOf]
    {
        auto finalDest = dest;
        bool ok = dest.existsAsFile() && dest.getSize() == source.getSize() && dest.hasIdenticalContentTo (source);
        if (! ok)
        {
            if (dest.existsAsFile())
                finalDest = dest.getNonexistentSibling (true);
            finalDest.getParentDirectory().createDirectory();
            const auto temp = finalDest.getSiblingFile (finalDest.getFileName() + ".part");
            ok = source.copyFileTo (temp) && temp.moveFileTo (finalDest);
            if (! ok)
                temp.deleteFile();
        }
        const auto relativePath = finalDest.getRelativePathFrom (folder).replaceCharacter ('\\', '/');

        juce::MessageManager::callAsync ([this, weak, serial, relativePath, target, original, ok, pathOf]
        {
            if (weak.expired() || serial != s.songSerial || pathOf (target) != original)
                return;   // 別の曲・別のお手本にした
            if (ok)
            {
                pathOf (target) = relativePath;   // ここからはプロジェクトの中のコピーを使う（持ち運べる）
                markDirty();
            }
            else
            {
                postNotice (tr ("project.copyFailed", original));
            }
        });
    });
}

void UiSession::saveProject()
{
    if (engine == nullptr || s.projectFile == juce::File() || s.songOriginal == nullptr || restoring)
        return;
    // 続きから開くプロジェクトの中身（テイク・区間・歌詞・お手本）を戻す前は書かない。伴奏の SR をそろえている間に
    // 曲を替える・終わると、空のプロジェクトで .vbooth を上書きし、次に開くとテイクが全部 Recovered へ移っていた（監査 2026-10-04）
    if (awaitingRestore || s.conforming)
        return;

    project::ProjectExtras ex;
    ex.guidePath = s.guidePath;
    ex.guideNudgeMs = s.guideNudgeMs;
    ex.recordRate = s.recordRate;
    ex.recordFloat = s.recordFloat;
    ex.deviceFallbackRate = s.deviceFallbackRate;
    // テイクがあれば、時間軸の SR をそのまま書く（録音形式の SR を「次の曲から」変えても、このプロジェクトは今の SR で開く。監査 2026-10-04）
    if (hasTakes())
    {
        ex.recordRate = s.sampleRate() == s.songRate ? 0.0 : (double) s.sampleRate();
        ex.deviceFallbackRate = 0;
    }
    for (auto& tu : s.trackUi)   // 録ったトラックの音量・M・S（B12）と練習のテンポ・キー（B11）も覚える
        ex.trackMix.push_back ({ tu.type, tu.monitorGain, tu.mute, tu.solo });
    ex.songHash = songHash;
    ex.practiceTempo = s.tempoPercent;
    ex.practiceKey = s.keyShift;
    // モニターの音量と M、作業の続き（2026-10-04。それまでは開き直すと既定に戻っていた）
    ex.monitor.has = true;
    ex.monitor.backing = s.offVocalGain;
    ex.monitor.backingMute = s.backingMuted;
    ex.monitor.guide = s.mainGain;
    ex.monitor.guideMute = s.guideMuted;
    ex.monitor.harmony = s.harmonyGain;
    ex.monitor.harmonyMute = s.guideHarmMuted;
    ex.monitor.self = s.monitorGain;
    ex.monitor.reverb = s.monitorReverb;
    ex.work.has = true;
    // テイクを比べている間は、比べる前の範囲とループを残す（比べるために一時的に変えた範囲が自動保存に入っていた。監査 2026-10-06）
    const auto keptIn = compareChangedRange ? compareRangeIn : s.rangeIn;
    const auto keptOut = compareChangedRange ? compareRangeOut : s.rangeOut;
    const bool keptRange = keptIn >= 0 && keptOut > keptIn;
    ex.work.rangeIn = keptRange ? keptIn : -1;
    ex.work.rangeOut = keptRange ? keptOut : -1;
    ex.work.loop = compareChangedRange ? compareLoopOn : s.loopOn;
    ex.work.track = project::trackKey (s.currentTrack().type);
    ex.work.octaveUp = s.octaveUp;
    ex.work.playhead = s.playhead;

    // 世代バックアップ：開いてから最初の保存と、その後 10 分ごとに、前の .vbooth を Backups/ へ（新しい 10 個を残す）
    const auto now = juce::Time::getMillisecondCounter();
    if (s.projectFile.existsAsFile() && (lastBackupMs == 0 || now - lastBackupMs > 10 * 60 * 1000))
    {
        const auto dir = s.projectFolder.getChildFile ("Backups");
        dir.createDirectory();
        auto old = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + project::fileExtension);
        std::sort (old.begin(), old.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName() > b.getFileName(); });
        // いちばん新しいバックアップと同じ中身なら作らない（開くだけで 1 世代ずつ増え、意味のある古い世代が押し出されていた。監査 2026-10-04）
        if (old.isEmpty() || ! old.getReference (0).hasIdenticalContentTo (s.projectFile))
        {
            const auto copy = dir.getChildFile (s.projectFile.getFileNameWithoutExtension() + "-"
                                                + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + project::fileExtension);
            s.projectFile.copyFileTo (copy);
            old.insert (0, copy);
        }
        for (int i = 10; i < old.size(); ++i)
            old.getReference (i).deleteFile();
        lastBackupMs = now;
    }

    // テイク比較の試聴中（B18c）は、選んでいない差し替えを書かない（確定している採用区間で保存する）
    auto committed = s.project;
    audition.restoreCommitted (committed);
    committed.modeLast = s.mode;   // 開いた後にモードを変えても、最初の画面の一覧は開いたときのモードのままだった（監査 2026-10-06）
    if (! project::writeAtomically (s.projectFile, project::toJson (committed, ex)))
    {
        // 失敗したら変更ありのまま残し、5 秒ごとに試し直す（終了時も）。知らせは続けて失敗した最初の 1 回だけ。
        // 前は dirty を下ろしていて、次に何か変えるまで・終了時も保存し直さず、録ったテイクが .vbooth に入らなかった（監査 2026-10-04）
        if (! saveFailed)
            postNotice (tr ("project.saveFailed", s.projectFile.getFullPathName()));
        saveFailed = true;
        dirty = true;
        saveRetryAt = juce::Time::getMillisecondCounter() + 5000;
        return;
    }
    saveFailed = false;
    savedPlayhead = s.playhead;
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
    // 曲を替える・終わる時：変更があるか、再生位置が保存した時から動いていれば保存（再生位置は動かすたびには保存しない）
    if (dirty || s.playhead != savedPlayhead)
        saveProject();
}

void UiSession::closeForQuit()
{
    // 録音中に閉じられても、そこまでの声はテイクとして残す（前は保存だけして、テイクがプロジェクトから外れていた）
    if (s.isRecording)
        setRecording (false);
    else
        finishRecording();   // 裏録り（B7）だけなら消す
    // テイクの比較中なら元の採用区間・範囲・ループに戻してから保存する（比べた範囲と「ループ入り」が保存されていた。
    // バグチェック 2026-10-05）
    endTakeCompare (false);
    flushSave();
    // 書き出しの途中なら終わるまで待つ（最長 2 分）。待たずに終わると、書きかけのパック・WAV が残っていた（監査 2026-10-06）
    for (const auto until = juce::Time::getMillisecondCounter() + 120000;
         exportJobs->load() > 0 && (juce::int32) (juce::Time::getMillisecondCounter() - until) < 0;)
        juce::Thread::sleep (50);
    // 分離の途中なら止めて、作業ファイル（入力の写し 約 85 MB など）を消す。止めた知らせはもう届かない
    // （アプリ共通のキャッシュに残っていた。バグチェック 2026-10-05）
    if (s.separating)
        stopSeparation();
    for (auto& f : separationScratch)
        f.deleteFile();
    separationScratch.clear();
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
    if (s.project.tempo.known() && s.project.key.known())
        return;   // プロジェクトから両方戻せた（同じ曲を同じ計算で推定し直すだけになる）
    const auto audio = s.songOriginal;
    const auto serial = s.songSerial;
    std::weak_ptr<bool> weak = alive;
    background::run ([this, weak, audio, serial]
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
    if (! s.output.open || ! s.output.converting || hasTakes() || timelineLocked)
        return;   // 開いていない・そのまま鳴らせる・テイクがある（時間軸を変えない。開く途中でまだ戻していないテイクも）
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
    s.recordFloat = s.prefRecordFloat = floatSamples;
    s.recordRate = s.prefRecordRate = rate;
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
    background::run ([this, weak, original, target, serial]
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
                restoreProject();   // 続きから開くプロジェクトは中身を戻す（SR が違ってテイクがあれば、保存はしない）
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
            // ハモリのお手本の線と「お手本が使える区間」も同じ時間軸にそろえる（前はそのままで、44.1→48 kHz なら 8.8% ずれていた。監査 2026-10-03）
            for (auto& p : s.refPitchHarm) p.sample = scale (p.sample);
            ++s.refPitchSerial;
            for (auto& c : s.guideCovered) c = { scale (c.first), scale (c.second) };
            for (auto& l : s.project.lyrics.lines) { l.startSample = scaleTimed (l.startSample); l.endSample = scaleTimed (l.endSample); }
            s.project.sampleRate = target;
            s.project.lengthSamples = audio->length();
            s.backingWave = wave;
            s.songCurrent = audio;

            if (engine != nullptr)
            {
                engine->setSong (audio);   // デバイスもこの SR に切り替える（対応していれば）
                { const auto g = ++stemGeneration; for (auto& sg : stemSlotGeneration) sg = g; }
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
            // そろえている間に録音形式の SR を選び直していたら、もう一度そろえる（前は 1 回目の SR のまま残り、保存する SR と食い違っていた）
            if (s.targetRate() != s.sampleRate() && ! hasTakes())
                conformSong();
        });
    });
}

static juce::File projectsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VoiceBooth").getChildFile ("Projects");
}

juce::File UiSession::legacyProjectFolderFor (const juce::String& songName)
{
    // 2026-10-05 より前の名前の付け方（長い名前のプロジェクトを続きから開くため）
    return projectsRoot().getChildFile (juce::File::createLegalFileName (songName.isNotEmpty() ? songName : juce::String ("Untitled")));
}

juce::File UiSession::projectFolderFor (const juce::String& songName, int number)
{
    // 使えない文字を外した後で空・点だけなら Untitled（前は Projects フォルダそのもの・その上を使った）。
    // 長い名前は切る（.vbooth の一時ファイル・書き出しの名前を足すとファイル名やパスの上限を超え、保存できなかった）。
    // 番号は切った後に付ける（前は 128 文字で番号ごと切られて、どの番号も同じフォルダになり、開くと固まった。バグチェック 2026-10-05）
    auto name = juce::File::createLegalFileName (songName).trim();
    if (name.isEmpty() || name.containsOnly (". "))   // 「.」「..」と、Windows で末尾の点が消えて空になる「...」
        name = "Untitled";
    constexpr int maxChars = 80, maxBytes = 160;
    if (name.length() > maxChars || (int) name.getNumBytesAsUTF8() > maxBytes)
    {
        name = name.substring (0, maxChars);
        while (name.isNotEmpty() && (int) name.getNumBytesAsUTF8() > maxBytes)
            name = name.dropLastCharacters (1);
        name = name.trimEnd();
    }
    if (number > 1)
        name << " (" << number << ")";
    return projectsRoot().getChildFile (name);
}
} // namespace vb
