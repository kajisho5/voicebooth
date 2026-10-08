#include "UiSession.h"
#include "system/Background.h"
#include "i18n/Reasons.h"
#include "audio/Resample.h"
#include "export/VideoEncoder.h"

/*  共有用の動画（DESIGN 9.1）：画の材料を集め、確認用ミックスの音と一緒に MP4 にする（バックグラウンド） */

namespace vb
{
namespace
{
    using project::TrackType;

    constexpr TrackType vocalTracks[] = { TrackType::main, TrackType::doubleTrack, TrackType::harm1, TrackType::harm2 };
    constexpr int shareFps = 30;

    /** お手本の点 → 音符（ピッチレーンと同じ。0.3 秒より短い切れ目は次の音符の頭まで伸ばす） */
    std::vector<analysis::NoteSpan> notesOf (const std::vector<dummy::PitchPoint>& ref, double sampleRate)
    {
        std::vector<audio::PitchFrame> frames;
        frames.reserve (ref.size());
        for (auto& p : ref)
            frames.push_back ({ p.sample, p.midi, p.confidence, 0.0f });
        auto notes = analysis::segmentNotes (frames, sampleRate);
        const auto legato = (int64) (0.3 * sampleRate);
        for (size_t i = 0; i + 1 < notes.size(); ++i)
            if (notes[i + 1].start - notes[i].end < legato)
                notes[i].end = notes[i + 1].start;
        return notes;
    }

    juce::File uniqueFile (const juce::File& folder, const juce::String& name)
    {
        auto f = folder.getChildFile (name);
        for (int n = 2; f.exists(); ++n)
            f = folder.getChildFile (name.upToLastOccurrenceOf (".", false, false) + "_" + juce::String (n) + ".mp4");
        return f;
    }
} // namespace

bool UiSession::canShareVideo() const
{
    if (s.project.lengthSamples <= 0 || s.projectFolder == juce::File())
        return false;
    for (auto t : vocalTracks)
        if (auto* tr = s.project.findTrack (t); tr != nullptr && ! tr->comp.empty())
            return true;
    return false;
}

std::pair<int64, int64> UiSession::shareSpan (bool useRange) const
{
    if (useRange && s.rangeOut > s.rangeIn)
        return { juce::jlimit<int64> (0, s.project.lengthSamples, s.rangeIn), juce::jlimit<int64> (0, s.project.lengthSamples, s.rangeOut) };
    return { 0, s.project.lengthSamples };
}

share::Scene UiSession::shareScene (const ShareRequest& req) const
{
    share::Scene sc;
    sc.shape = req.shape;
    sc.sampleRate = (double) juce::jmax (1, s.sampleRate());
    std::tie (sc.from, sc.to) = shareSpan (req.useRange);
    sc.title = s.songName;
    sc.showPitch = req.showPitch;
    sc.background = req.background;
    sc.colours = currentSkinColours();

    // 歌詞：時刻のある行。表示の終わりは次の行の頭（なければ 6 秒）
    const auto& lines = s.project.lyrics.lines;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const auto& l = lines[i];
        if (! l.timed() || l.text.trim().isEmpty())
            continue;
        auto end = l.endSample > l.startSample ? l.endSample : (int64) -1;
        if (end < 0)
            for (size_t j = i + 1; j < lines.size() && end < 0; ++j)
                if (lines[j].timed() && lines[j].startSample > l.startSample)
                    end = lines[j].startSample;
        if (end < 0)
            end = l.startSample + (int64) (6.0 * sc.sampleRate);
        if (end <= sc.from || l.startSample >= sc.to)
            continue;
        sc.lyrics.push_back ({ l.startSample, end, l.text.trim() });
    }
    std::sort (sc.lyrics.begin(), sc.lyrics.end(), [] (auto& a, auto& b) { return a.start < b.start; });

    if (req.showPitch)
    {
        // お手本の音符（区間の前後も少し：画面の右から入ってくる分）
        const auto margin = (int64) (8.0 * sc.sampleRate);
        for (auto& n : notesOf (s.refPitch, sc.sampleRate))
            if (n.end > sc.from - margin && n.start < sc.to + margin)
                sc.guide.push_back (n);

        // 採用した声の音程（メインのトラック。原速・原キーで録ったテイクだけ音程がある）
        if (auto* main = s.project.findTrack (TrackType::main))
        {
            auto comp = main->comp;
            std::sort (comp.begin(), comp.end(), [] (auto& a, auto& b) { return a.startSample < b.startSample; });
            for (auto& seg : comp)
            {
                const auto it = s.takePitch.find (dummy::takeWaveKey (TrackType::main, seg.takeId));
                if (it == s.takePitch.end() || it->second == nullptr)
                    continue;
                const auto a = juce::jmax (seg.startSample, sc.from), b = juce::jmin (seg.endSample, sc.to);
                if (a >= b)
                    continue;
                for (auto& f : *it->second)
                    if (f.songSample >= a && f.songSample < b)
                        sc.voice.push_back ({ f.songSample, f.midi > 0.0f && f.confidence >= 0.5f ? f.midi : 0.0f });
                sc.voice.push_back ({ b, 0.0f });   // テイクの継ぎ目で線を切る
            }
        }
        sc.fitPitchRange();
    }

    // 見本の波形：伴奏の概形（書き出すときは確認用ミックスから作り直す）
    const auto bins = share::waveBinCount (req.shape);
    sc.wave.assign ((size_t) bins, 0.0f);
    if (s.backingWave != nullptr && sc.to > sc.from)
    {
        float top = 0.0f;
        for (int b = 0; b < bins; ++b)
        {
            const auto a = sc.from + (sc.to - sc.from) * b / bins, e = sc.from + (sc.to - sc.from) * (b + 1) / bins;
            sc.wave[(size_t) b] = s.backingWave->getRms (a, e);
            top = juce::jmax (top, sc.wave[(size_t) b]);
        }
        if (top > 0.0f)
            for (auto& v : sc.wave)
                v = std::pow (v / top, 0.8f);
    }
    return sc;
}

juce::String UiSession::sharePostText() const
{
    return tr ("share.post.template", s.songName.isNotEmpty() ? s.songName : tr ("share.post.untitled"));
}

void UiSession::exportShareVideo (const ShareRequest& req)
{
    if (s.share.running)  { postNotice (tr ("share.busy")); return; }
    if (! canShareVideo()) { postNotice (tr ("share.nothing")); return; }
    if (! video::Encoder::available())
    {
        s.share.error = tr ("share.unavailable");
        notify (change::share);
        return;
    }

    auto scene = shareScene (req);
    if (scene.to <= scene.from)
        return;

    auto o = mixOptions();
    for (auto t : vocalTracks)
        if (auto* tr = s.project.findTrack (t); tr != nullptr && ! tr->comp.empty())
            o.tracks.push_back (t);

    const auto folder = s.projectFolder.getChildFile ("share");
    if (! folder.createDirectory())
    {
        s.share.error = i18n::reasonText ("can't create " + folder.getFullPathName());
        notify (change::share);
        return;
    }
    const auto dest = uniqueFile (folder, share::fileName (s.songName, req.shape));

    s.share = {};
    s.share.running = true;
    notify (change::share);

    auto cancel = std::make_shared<std::atomic<bool>> (false);
    shareCancel = cancel;
    const auto job = ++shareJob;
    auto project = s.project;
    const auto projectFolder = s.projectFolder;
    std::weak_ptr<bool> weak = alive;
    ++*exportJobs;
    background::run ([this, weak, project, projectFolder, o, scene, dest, cancel, job, jobs = exportJobs]() mutable
    {
        // 進み具合（1 % ごとにメッセージスレッドへ）。音を作るまでを 1 割とみなす
        int lastPercent = -1;
        auto report = [&] (float p)
        {
            const auto percent = juce::roundToInt (juce::jlimit (0.0f, 1.0f, p) * 100.0f);
            if (percent != lastPercent)
            {
                lastPercent = percent;
                juce::MessageManager::callAsync ([this, weak, job, p]
                {
                    if (weak.expired() || job != shareJob || ! s.share.running)
                        return;
                    s.share.progress = p;
                    notify (change::share);
                });
            }
            return ! cancel->load();
        };

        // 1. 音：確認用ミックスと同じ計算で区間だけ。-1 dBFS を超えるときだけ下げる。区間の頭と終わりは短く絞る（途中で切った音がぷつっと鳴らない）
        juce::String error;
        std::shared_ptr<const juce::AudioBuffer<float>> audio;
        {
            exporter::RefMix mix;
            if (! mix.prepare (project, projectFolder, o, error))
                error = "refmix: " + error;
            else if (! report (0.03f))
                error = "cancelled";
            else
            {
                const auto peak = mix.peak (scene.from, scene.to);
                const auto ceiling = juce::Decibels::decibelsToGain (-1.0f);
                audio::SongAudio clip;
                clip.sampleRate = (double) project.sampleRate;
                mix.render (scene.from, scene.to, peak > ceiling ? ceiling / peak : 1.0f, clip.buffer);
                const auto n = clip.buffer.getNumSamples();
                const auto fadeIn = juce::jmin (n / 2, (int) (0.02 * clip.sampleRate));
                const auto fadeOut = juce::jmin (n / 2, (int) ((scene.to < project.lengthSamples ? 0.6 : 0.02) * clip.sampleRate));
                if (fadeIn > 0)  clip.buffer.applyGainRamp (0, fadeIn, 0.0f, 1.0f);
                if (fadeOut > 0) clip.buffer.applyGainRamp (n - fadeOut, fadeOut, 1.0f, 0.0f);
                if (std::abs (clip.sampleRate - video::audioRate) > 0.5)
                {
                    auto converted = audio::resampleSong (clip, (double) video::audioRate,
                                                          [&] (float p) { return report (0.03f + 0.07f * p); });
                    if (converted == nullptr)
                        error = "cancelled";
                    else
                        audio = std::shared_ptr<const juce::AudioBuffer<float>> (converted, &converted->buffer);
                }
                else
                    audio = std::make_shared<juce::AudioBuffer<float>> (std::move (clip.buffer));
            }
        }

        // 2. コマを描いて書く
        if (error.isEmpty())
        {
            scene.wave = share::waveBins (*audio, share::waveBinCount (scene.shape));
            error = share::write (scene, audio, dest, shareFps, [&] (float p) { return report (0.1f + 0.9f * p); });
        }
        --*jobs;

        juce::MessageManager::callAsync ([this, weak, job, dest, error]
        {
            if (weak.expired() || job != shareJob)
                return;
            s.share.running = false;
            shareCancel = nullptr;
            if (error.isEmpty())
            {
                s.share.progress = 1.0f;
                s.share.file = dest;
                postNotice (tr ("share.done", dest.getFileName()));
            }
            else if (error == "cancelled")
                s.share.progress = 0.0f;
            else
            {
                s.share.error = i18n::reasonText (error);
                postNotice (tr ("share.failed", s.share.error));
            }
            notify (change::share);
        });
    });
}

void UiSession::cancelShareVideo()
{
    if (shareCancel != nullptr)
        shareCancel->store (true);
}
} // namespace vb
