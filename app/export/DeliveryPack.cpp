#include "DeliveryPack.h"
#include <set>

namespace vb::exporter
{
using project::int64;
using project::TrackType;

juce::String DeliveryPack::packFileName (TrackType t)
{
    switch (t)
    {
        case TrackType::main:        return "vocal_dry.wav";
        case TrackType::doubleTrack: return "double_dry.wav";
        case TrackType::harm1:       return "harmony1_dry.wav";
        case TrackType::harm2:       return "harmony2_dry.wav";
        case TrackType::backing:
        case TrackType::guide:       break;   // ボーカルのファイルには混ぜない（6.5）
    }
    return {};
}

juce::File DeliveryPack::nextFolder (const juce::File& projectFolder, juce::Time when)
{
    const auto base = "export_" + when.formatted ("%Y%m%d");
    auto f = projectFolder.getChildFile (base);
    for (int n = 2; f.exists() || f.getSiblingFile (f.getFileName() + ".zip").exists(); ++n)
        f = projectFolder.getChildFile (base + "_" + juce::String (n));
    return f;
}

juce::String DeliveryPack::timeText (int64 sample, int sampleRate)
{
    const auto ms = sampleRate > 0 ? (int64) std::llround ((double) sample * 1000.0 / sampleRate) : 0;
    const auto h = ms / 3600000, m = (ms / 60000) % 60, sec = (ms / 1000) % 60, milli = ms % 1000;
    const auto tail = juce::String (sec).paddedLeft ('0', 2) + "." + juce::String (milli).paddedLeft ('0', 3);
    return h > 0 ? juce::String (h) + ":" + juce::String (m).paddedLeft ('0', 2) + ":" + tail
                 : juce::String (m) + ":" + tail;
}

namespace
{
    juce::String trackLabel (TrackType t)
    {
        switch (t)
        {
            case TrackType::main:        return "Main";
            case TrackType::doubleTrack: return "Double";
            case TrackType::harm1:       return "Harmony 1";
            case TrackType::harm2:       return "Harmony 2";
            case TrackType::backing:     return "Backing";
            case TrackType::guide:       return "Guide";
        }
        return {};
    }

    juce::String dbText (float gain)
    {
        if (gain <= 0.0f)
            return "-inf";
        return juce::String (juce::Decibels::gainToDecibels (gain), 1);
    }

    std::unique_ptr<juce::AudioFormatWriter> makeWriter (const juce::File& file, double rate, int channels, int bitDepth)
    {
        auto fileStream = std::make_unique<juce::FileOutputStream> (file);
        if (! fileStream->openedOk())
            return {};
        std::unique_ptr<juce::OutputStream> stream (fileStream.release());
        juce::WavAudioFormat wav;
        const bool asFloat = bitDepth >= 32;
        using Format = juce::AudioFormatWriterOptions::SampleFormat;
        return wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                         .withNumChannels (channels)
                                                                         .withBitsPerSample (asFloat ? 32 : (bitDepth <= 16 ? 16 : 24))
                                                                         .withSampleFormat (asFloat ? Format::floatingPoint : Format::integral));
    }
}

juce::String DeliveryPack::notesText (const project::Project& p, const PackOptions& o, const PackResult& r)
{
    // DESIGN 9 の項目（title / sr / bit / key / tempo / peak_vocal_dbfs / normalized / latency_compensation_ms）＋ 補足
    juce::StringArray lines;
    lines.add ("title: " + o.songName);
    lines.add ("sr: " + juce::String (p.sampleRate));
    lines.add ("bit: " + (o.bitDepth >= 32 ? juce::String ("32 float") : juce::String (o.bitDepth <= 16 ? 16 : 24)));
    lines.add ("key: 0");          // 納品は原キー（練習のキー変更は入らない。6.1）
    lines.add ("tempo: 100");      // 納品は原速
    lines.add ("length: " + timeText (p.lengthSamples, p.sampleRate) + " (" + juce::String (p.lengthSamples) + " samples, from 0:00.000)");
    if (o.bpm > 0.0)           lines.add ("song_bpm: " + juce::String (o.bpm, 2));
    if (o.songKey.isNotEmpty()) lines.add ("song_key: " + o.songKey);
    for (auto t : o.tracks)
        if (auto it = r.peaks.find (t); it != r.peaks.end())
        {
            const auto key = t == TrackType::main ? juce::String ("peak_vocal_dbfs")
                                                  : "peak_" + packFileName (t).upToFirstOccurrenceOf ("_dry", false, false) + "_dbfs";
            lines.add (key + ": " + dbText (it->second));
        }
    lines.add ("normalized: no");

    // 遅れの補正：書き出したトラックの採用区間で使ったテイクの値（ms）
    std::set<int64> latencies;
    for (auto t : o.tracks)
        if (const auto* tr = p.findTrack (t))
            for (auto& c : tr->comp)
                for (auto& k : tr->takes)
                    if (k.id == c.takeId)
                        latencies.insert (k.latencySamples);
    juce::StringArray ms;
    for (auto l : latencies)
        ms.add (juce::String (p.sampleRate > 0 ? (double) l * 1000.0 / p.sampleRate : 0.0, 1));
    lines.add ("latency_compensation_ms: " + (ms.isEmpty() ? juce::String ("0.0") : ms.joinIntoString (" / ")));
    lines.add ("crossfade_ms: " + juce::String (o.crossfadeMs, 1) + " (only at seams between takes)");
    if (r.files.contains ("refmix.wav"))
        lines.add ("refmix: for checking only (not for mixing). level " + juce::String (r.refmixGainDb, 1) + " dB");
    lines.add ("files: " + r.files.joinIntoString (", "));
    lines.add ("made_with: VoiceBooth");
    return lines.joinIntoString ("\n") + "\n";
}

juce::String DeliveryPack::takeMapText (const project::Project& p, const PackOptions& o)
{
    // DESIGN 9：トラックごとに「0:00.000-0:48.250 take2」。曲の終わりまでなら end。録っていない所は (none)
    juce::StringArray lines;
    for (auto t : o.tracks)
    {
        const auto* tr = p.findTrack (t);
        if (tr == nullptr || tr->comp.empty())
            continue;
        if (! lines.isEmpty()) lines.add ({});
        lines.add (trackLabel (t));

        int64 at = 0;
        auto line = [&] (int64 a, int64 b, const juce::String& what)
        {
            auto text = timeText (a, p.sampleRate) + "-" + (b >= p.lengthSamples ? juce::String ("end") : timeText (b, p.sampleRate)) + " " + what;
            if (o.sectionAt)
                if (const auto name = o.sectionAt (a); name.isNotEmpty())
                    text << "  [" << name << "]";
            lines.add (text);
        };
        for (auto& c : tr->comp)
        {
            const auto a = juce::jmax ((int64) 0, c.startSample), b = juce::jmin (p.lengthSamples, c.endSample);
            if (b <= a)
                continue;
            if (a > at)
                line (at, a, "(none)");
            line (a, b, c.takeId);
            at = b;
        }
        if (at < p.lengthSamples)
            line (at, p.lengthSamples, "(none)");
    }
    return lines.joinIntoString ("\n") + "\n";
}

PackResult DeliveryPack::write (const project::Project& project, const juce::File& projectFolder, const PackOptions& o)
{
    PackResult r;
    if (project.lengthSamples <= 0 || project.sampleRate <= 0 || o.tracks.empty())
    {
        r.message = o.tracks.empty() ? "nothing recorded" : "no song";
        return r;
    }

    const auto folder = nextFolder (projectFolder, juce::Time::getCurrentTime());
    if (! folder.createDirectory())
    {
        r.message = "can't create " + folder.getFullPathName();
        return r;
    }
    r.folder = folder;
    auto fail = [&] (const juce::String& why)
    {
        folder.deleteRecursively();   // 途中までのパックは残さない（新しく作ったフォルダだけ）
        r.ok = false;
        r.message = why;
        r.folder = juce::File();
        return r;
    };

    // 進み具合：ボーカル 1 本ずつ・refmix・zip を同じ重さとみなす
    const auto steps = (float) o.tracks.size() + (o.backing != nullptr ? 1.0f : 0.0f) + (o.zip ? 1.0f : 0.0f);
    float done = 0.0f;
    auto stepProgress = [&] (float within) { return ! o.progress || o.progress ((done + within) / steps); };

    // 1. ボーカル（書き出しと同じ。6.5 を守る）
    auto p = project;
    p.bitDepthExport = o.bitDepth;
    for (auto t : o.tracks)
    {
        Options eo;
        eo.crossfadeMs = o.crossfadeMs;
        eo.progress = stepProgress;
        const auto name = packFileName (t);
        const auto res = ExportService::exportTrackDry (p, t, projectFolder, folder.getChildFile (name), eo);
        if (! res.ok)
            return fail (name + ": " + res.message);
        r.files.add (name);
        r.peaks[t] = res.peak;
        done += 1.0f;
    }

    // 2. 確認用ミックス（伴奏 + ボーカル。ステレオ）。-1 dBFS を超える時だけ全体を下げる
    if (o.backing != nullptr && o.backing->getNumSamples() > 0)
    {
        std::vector<std::pair<juce::AudioBuffer<float>, float>> vocals;
        for (auto t : o.tracks)
        {
            const auto g = o.vocalGains.count (t) ? o.vocalGains.at (t) : 1.0f;
            if (g <= 0.0f)
                continue;
            juce::AudioBuffer<float> b;
            Options eo;
            eo.crossfadeMs = o.crossfadeMs;
            if (! ExportService::renderTrackDry (p, t, projectFolder, b, eo).ok)
                return fail ("refmix: can't render " + trackLabel (t));
            vocals.push_back ({ std::move (b), g });
        }

        const auto length = (int) juce::jmin<int64> (p.lengthSamples, std::numeric_limits<int>::max());
        const auto& back = *o.backing;
        const auto backCh = back.getNumChannels();
        auto mixAt = [&] (int c, int i)
        {
            float v = i < back.getNumSamples() ? back.getSample (juce::jmin (c, backCh - 1), i) * o.backingGain : 0.0f;
            for (auto& [b, g] : vocals)
                if (i < b.getNumSamples())
                    v += b.getSample (0, i) * g;
            return v;
        };

        float peak = 0.0f;
        for (int i = 0; i < length; ++i)
            for (int c = 0; c < 2; ++c)
                peak = juce::jmax (peak, std::abs (mixAt (c, i)));
        const auto ceiling = juce::Decibels::decibelsToGain (-1.0f);
        const auto scale = peak > ceiling ? ceiling / peak : 1.0f;
        r.refmixGainDb = juce::Decibels::gainToDecibels (scale);

        const auto file = folder.getChildFile ("refmix.wav");
        auto writer = makeWriter (file, (double) p.sampleRate, 2, o.bitDepth);
        if (writer == nullptr)
            return fail ("refmix: can't create WAV writer");
        constexpr int block = 65536;
        juce::AudioBuffer<float> out (2, block);
        for (int a = 0; a < length; a += block)
        {
            const auto n = juce::jmin (block, length - a);
            for (int c = 0; c < 2; ++c)
            {
                auto* w = out.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    w[i] = mixAt (c, a + i) * scale;
            }
            if (! writer->writeFromAudioSampleBuffer (out, 0, n))
                return fail ("refmix: write failed");
            if (! stepProgress ((float) (a + n) / (float) length))
                return fail ("cancelled");
        }
        writer.reset();
        r.files.add ("refmix.wav");
        done += 1.0f;
    }

    // 3. notes.txt / take_map.txt
    if (o.takeMap)
    {
        if (! folder.getChildFile ("take_map.txt").replaceWithText (takeMapText (project, o), false, false, "\n"))
            return fail ("can't write take_map.txt");
        r.files.add ("take_map.txt");
    }
    r.files.add ("notes.txt");
    if (! folder.getChildFile ("notes.txt").replaceWithText (notesText (project, o, r), false, false, "\n"))
        return fail ("can't write notes.txt");

    // 4. zip（フォルダと同じ名前。WAV は無圧縮で格納、文字は圧縮）。一時ファイルに書いてから名前を変える
    if (o.zip)
    {
        juce::ZipFile::Builder builder;
        for (auto& name : r.files)
            builder.addFile (folder.getChildFile (name), name.endsWithIgnoreCase (".wav") ? 0 : 9,
                             folder.getFileName() + "/" + name);
        const auto zipFile = folder.getSiblingFile (folder.getFileName() + ".zip");
        const auto temp = zipFile.getSiblingFile (zipFile.getFileName() + ".part");
        temp.deleteFile();
        bool ok = false;
        {
            juce::FileOutputStream out (temp);
            double zp = 0.0;
            ok = out.openedOk() && builder.writeToStream (out, &zp);
            out.flush();
            ok = ok && ! out.getStatus().failed();
        }
        if (! ok || ! temp.moveFileTo (zipFile))
        {
            temp.deleteFile();
            return fail ("can't write zip");
        }
        r.zipFile = zipFile;
    }

    r.ok = true;
    if (o.progress) o.progress (1.0f);
    return r;
}
} // namespace vb::exporter
