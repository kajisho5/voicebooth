#include "ProjectFile.h"

namespace vb::project
{
namespace
{
    using juce::var;
    using Obj = juce::DynamicObject;

    var obj() { return var (new Obj()); }
    void set (var& o, const char* key, const var& v) { o.getDynamicObject()->setProperty (key, v); }

    // 読む側：無ければ既定値
    juce::int64 getInt64 (const var& o, const char* key, juce::int64 def = 0)
    {
        const auto v = o.getProperty (key, var());
        return v.isVoid() ? def : (juce::int64) v;
    }
    int getInt (const var& o, const char* key, int def = 0)               { const auto v = o.getProperty (key, var()); return v.isVoid() ? def : (int) v; }
    double getDouble (const var& o, const char* key, double def = 0.0)    { const auto v = o.getProperty (key, var()); return v.isVoid() ? def : (double) v; }
    bool getBool (const var& o, const char* key, bool def = false)        { const auto v = o.getProperty (key, var()); return v.isVoid() ? def : (bool) v; }
    juce::String getString (const var& o, const char* key)                { return o.getProperty (key, var()).toString(); }
    bool has (const var& o, const char* key)                              { return ! o.getProperty (key, var()).isVoid(); }

    song::Source sourceFrom (const var& o)
    {
        return getString (o, "source") == "manual" ? song::Source::confirmed : song::Source::estimated;
    }

    const char* modeKey (Mode m)
    {
        switch (m)
        {
            case Mode::easy:     return "easy";
            case Mode::standard: return "standard";
            case Mode::pro:      return "pro";
        }
        return "standard";
    }

    Mode modeFrom (const juce::String& s)
    {
        if (s == "easy") return Mode::easy;
        if (s == "pro")  return Mode::pro;
        return Mode::standard;
    }

    bool trackTypeFrom (const juce::String& s, TrackType& t)
    {
        for (auto type : { TrackType::backing, TrackType::guide, TrackType::main, TrackType::doubleTrack, TrackType::harm1, TrackType::harm2 })
            if (s == trackKey (type))
            {
                t = type;
                return true;
            }
        return false;
    }
}

juce::String toJson (const Project& p, const ProjectExtras& extras)
{
    auto root = obj();
    set (root, "format", "voicebooth.project");
    set (root, "format_version", formatVersion);
    set (root, "song_path", p.songPath);
    set (root, "sr", p.sampleRate);
    set (root, "bit_depth_export", p.bitDepthExport);
    set (root, "length_samples", p.lengthSamples);

    if (extras.guidePath.isNotEmpty())
    {
        auto ref = obj();
        set (ref, "path", extras.guidePath);
        if (extras.guideNudgeMs != 0.0)
            set (ref, "nudge_ms", extras.guideNudgeMs);
        set (root, "reference", ref);
    }
    auto rec = obj();
    set (rec, "sample_rate", extras.recordRate);
    set (rec, "float", extras.recordFloat);
    if (extras.deviceFallbackRate > 0)
        set (rec, "device_fallback_rate", extras.deviceFallbackRate);
    set (root, "record_format", rec);

    if (! extras.trackMix.empty())
    {
        juce::Array<var> mix;
        for (auto& m : extras.trackMix)
        {
            auto o = obj();
            set (o, "type", trackKey (m.type));
            set (o, "gain", std::round ((double) m.gain * 1000.0) / 1000.0);   // 0.800000011920929 のような端数を残さない
            set (o, "mute", m.mute);
            set (o, "solo", m.solo);
            mix.add (o);
        }
        set (root, "track_mix", mix);
    }
    if (extras.practiceTempo != 100 || extras.practiceKey != 0)
    {
        auto pr = obj();
        set (pr, "tempo_percent", extras.practiceTempo);
        set (pr, "key_shift", extras.practiceKey);
        set (root, "practice", pr);
    }

    if (p.key.known())
    {
        auto k = obj();
        set (k, "tonic", p.key.tonic);
        set (k, "mode", p.key.minor ? "minor" : "major");
        set (k, "source", song::sourceKey (p.key.source));
        set (k, "confidence", p.key.confidence);
        set (root, "key_original", k);
    }
    if (p.tempo.known())
    {
        auto t = obj();
        set (t, "bpm", p.tempo.bpm);
        set (t, "time_signature", p.tempo.signature.toString());
        set (t, "downbeat_sample", p.tempo.downbeatSample);
        juce::Array<var> beats;
        for (auto b : p.tempo.beats)
            beats.add (b);
        set (t, "beats", beats);
        set (t, "source", song::sourceKey (p.tempo.source));
        set (t, "confidence", p.tempo.confidence);
        set (root, "tempo", t);
    }
    set (root, "cache_dir", p.cacheDir);
    set (root, "mode_last", modeKey (p.modeLast));
    set (root, "input_profile_id", p.inputProfileId);

    juce::Array<var> tracks;
    for (auto& tr : p.tracks)
    {
        auto t = obj();
        set (t, "type", trackKey (tr.type));
        juce::Array<var> takes;
        for (auto& k : tr.takes)
        {
            auto o = obj();
            set (o, "id", k.id);
            set (o, "path", k.path);
            set (o, "start_sample", k.startSample);
            set (o, "end_sample", k.endSample);
            set (o, "created", k.created.toISO8601 (true));
            set (o, "clip", k.clip);
            set (o, "peak", k.peak);
            set (o, "rec_mode", k.recMode == RecMode::practice ? "practice" : "delivery");
            set (o, "latency_samples", k.latencySamples);
            if (k.useFrom >= 0 && k.useTo > k.useFrom)       // リハーサルのテイクを本番に入れる時の範囲
            {
                set (o, "use_from", k.useFrom);
                set (o, "use_to", k.useTo);
            }
            if (k.tempoPercent != 100 || k.keyShift != 0)   // 練習録音で変えていた時だけ（B11）
            {
                set (o, "tempo_percent", k.tempoPercent);
                set (o, "key_shift", k.keyShift);
            }
            takes.add (o);
        }
        set (t, "takes", takes);
        juce::Array<var> comp;
        for (auto& c : tr.comp)
        {
            auto o = obj();
            set (o, "start_sample", c.startSample);
            set (o, "end_sample", c.endSample);
            set (o, "take_id", c.takeId);
            comp.add (o);
        }
        set (t, "comp", comp);
        tracks.add (t);
    }
    set (root, "tracks", tracks);

    juce::Array<var> sections;
    for (auto& sec : p.sections)
    {
        auto o = obj();
        set (o, "start_sample", sec.startSample);
        set (o, "kind", sec.kind);
        if (sec.isCustom())
            set (o, "name", sec.name);
        set (o, "source", song::sourceKey (sec.source));
        if (sec.heading >= 0)
            set (o, "heading", sec.heading);
        sections.add (o);
    }
    set (root, "sections", sections);

    if (! p.lyrics.empty())
    {
        auto ly = obj();
        set (ly, "source_file_name", p.lyrics.sourceFileName);
        set (ly, "encoding", p.lyrics.encoding);
        set (ly, "sections_from_headings", p.lyrics.sectionsFromHeadings);
        juce::Array<var> headings, lines, chorus;
        for (auto& h : p.lyrics.headings)
        {
            auto o = obj();
            set (o, "name", h.name);
            set (o, "first_line", h.firstLine);
            headings.add (o);
        }
        for (auto& l : p.lyrics.lines)
        {
            auto o = obj();
            set (o, "text", l.text);
            if (l.timed())
            {
                set (o, "start_sample", l.startSample);
                set (o, "source", song::sourceKey (l.source));
            }
            set (o, "block", l.block);
            if (l.heading >= 0)
                set (o, "heading", l.heading);
            lines.add (o);
        }
        for (auto b : p.lyrics.chorusBlocks)
            chorus.add (b);
        set (ly, "headings", headings);
        set (ly, "lines", lines);
        set (ly, "chorus_blocks", chorus);
        set (root, "lyrics", ly);
    }

    return juce::JSON::toString (root, false) + "\n";
}

LoadedProject fromJson (const juce::String& text)
{
    LoadedProject out;
    var root;
    if (juce::JSON::parse (text, root).failed() || ! root.isObject())
    {
        out.error = "project.error.notProject";
        return out;
    }
    if (getString (root, "format") != "voicebooth.project")
    {
        out.error = "project.error.notProject";
        return out;
    }
    if (getInt (root, "format_version", 1) > formatVersion)
    {
        out.error = "project.error.newer";
        return out;
    }

    auto& p = out.project;
    p.songPath = getString (root, "song_path");
    p.sampleRate = getInt (root, "sr", 48000);
    p.bitDepthExport = getInt (root, "bit_depth_export", 24);
    p.lengthSamples = getInt64 (root, "length_samples");
    p.cacheDir = getString (root, "cache_dir");
    p.modeLast = modeFrom (getString (root, "mode_last"));
    p.inputProfileId = getString (root, "input_profile_id");

    if (const auto ref = root.getProperty ("reference", var()); ref.isObject())
    {
        out.extras.guidePath = getString (ref, "path");
        out.extras.guideNudgeMs = juce::jlimit (-500.0, 500.0, getDouble (ref, "nudge_ms"));
    }
    if (const auto rec = root.getProperty ("record_format", var()); rec.isObject())
    {
        out.extras.recordRate = getDouble (rec, "sample_rate");
        out.extras.recordFloat = getBool (rec, "float");
        out.extras.deviceFallbackRate = getInt (rec, "device_fallback_rate");
    }
    if (const auto* mix = root.getProperty ("track_mix", var()).getArray())
        for (auto& o : *mix)
        {
            ProjectExtras::TrackMix m;
            if (! trackTypeFrom (getString (o, "type"), m.type))
                continue;
            m.gain = juce::jlimit (0.0f, 1.0f, (float) getDouble (o, "gain", 0.75));
            m.mute = getBool (o, "mute");
            m.solo = getBool (o, "solo");
            out.extras.trackMix.push_back (m);
        }
    if (const auto pr = root.getProperty ("practice", var()); pr.isObject())
    {
        out.extras.practiceTempo = juce::jlimit (50, 150, getInt (pr, "tempo_percent", 100));
        out.extras.practiceKey = juce::jlimit (-6, 6, getInt (pr, "key_shift", 0));
    }

    if (const auto k = root.getProperty ("key_original", var()); k.isObject())
    {
        p.key.tonic = juce::jlimit (-1, 11, getInt (k, "tonic", -1));
        p.key.minor = getString (k, "mode") == "minor";
        p.key.source = sourceFrom (k);
        p.key.confidence = (float) getDouble (k, "confidence");
    }
    if (const auto t = root.getProperty ("tempo", var()); t.isObject())
    {
        p.tempo.bpm = getDouble (t, "bpm");
        const auto sig = juce::StringArray::fromTokens (getString (t, "time_signature"), "/", {});
        if (sig.size() == 2 && sig[0].getIntValue() > 0 && sig[1].getIntValue() > 0)
            p.tempo.signature = { sig[0].getIntValue(), sig[1].getIntValue() };
        p.tempo.downbeatSample = getInt64 (t, "downbeat_sample");
        if (const auto* beats = t.getProperty ("beats", var()).getArray())
            for (auto& b : *beats)
                p.tempo.beats.push_back ((juce::int64) b);
        p.tempo.source = sourceFrom (t);
        p.tempo.confidence = (float) getDouble (t, "confidence");
    }

    if (const auto* tracks = root.getProperty ("tracks", var()).getArray())
        for (auto& t : *tracks)
        {
            Track tr;
            if (! trackTypeFrom (getString (t, "type"), tr.type))
                continue;   // 知らない種類は読まない
            if (const auto* takes = t.getProperty ("takes", var()).getArray())
                for (auto& o : *takes)
                {
                    Take k;
                    k.id = getString (o, "id");
                    k.path = getString (o, "path");
                    k.startSample = getInt64 (o, "start_sample");
                    k.endSample = getInt64 (o, "end_sample");
                    k.created = juce::Time::fromISO8601 (getString (o, "created"));
                    k.clip = getBool (o, "clip");
                    k.peak = (float) getDouble (o, "peak");
                    k.recMode = getString (o, "rec_mode") == "practice" ? RecMode::practice : RecMode::delivery;
                    k.latencySamples = getInt64 (o, "latency_samples");
                    k.useFrom = getInt64 (o, "use_from", -1);
                    k.useTo = getInt64 (o, "use_to", -1);
                    k.tempoPercent = juce::jlimit (50, 150, getInt (o, "tempo_percent", 100));
                    k.keyShift = juce::jlimit (-6, 6, getInt (o, "key_shift", 0));
                    if (k.id.isNotEmpty())
                        tr.takes.push_back (k);
                }
            if (const auto* comp = t.getProperty ("comp", var()).getArray())
                for (auto& o : *comp)
                    tr.comp.push_back ({ getInt64 (o, "start_sample"), getInt64 (o, "end_sample"), getString (o, "take_id") });
            p.tracks.push_back (std::move (tr));
        }

    if (const auto* sections = root.getProperty ("sections", var()).getArray())
        for (auto& o : *sections)
        {
            song::Section sec;
            sec.startSample = getInt64 (o, "start_sample");
            sec.kind = getString (o, "kind");
            if (sec.kind.isEmpty())
                sec.kind = song::kind::generic;
            sec.name = getString (o, "name");
            sec.source = sourceFrom (o);
            sec.heading = getInt (o, "heading", -1);
            p.sections.push_back (sec);
        }

    if (const auto ly = root.getProperty ("lyrics", var()); ly.isObject())
    {
        p.lyrics.sourceFileName = getString (ly, "source_file_name");
        p.lyrics.encoding = getString (ly, "encoding");
        p.lyrics.sectionsFromHeadings = getBool (ly, "sections_from_headings", true);
        if (const auto* headings = ly.getProperty ("headings", var()).getArray())
            for (auto& o : *headings)
                p.lyrics.headings.push_back ({ getString (o, "name"), getInt (o, "first_line") });
        if (const auto* lines = ly.getProperty ("lines", var()).getArray())
            for (auto& o : *lines)
            {
                song::Line l;
                l.text = getString (o, "text");
                l.startSample = has (o, "start_sample") ? getInt64 (o, "start_sample") : -1;
                l.block = getInt (o, "block");
                l.heading = getInt (o, "heading", -1);
                l.source = has (o, "source") ? sourceFrom (o) : song::Source::confirmed;
                p.lyrics.lines.push_back (l);
            }
        if (const auto* chorus = ly.getProperty ("chorus_blocks", var()).getArray())
            for (auto& b : *chorus)
                p.lyrics.chorusBlocks.push_back ((int) b);
    }

    out.ok = true;
    return out;
}

bool writeAtomically (const juce::File& file, const juce::String& text)
{
    if (! file.getParentDirectory().createDirectory().wasOk())
        return false;
    juce::TemporaryFile temp (file);
    if (! temp.getFile().replaceWithText (text, false, false, "\n"))
        return false;
    return temp.overwriteTargetFileWithTemporary();
}

int recoverRetroLeftovers (const juce::File& projectFolder, const juce::StringArray& usedPaths)
{
    const auto takes = projectFolder.getChildFile ("Audio/Takes");
    int moved = 0;
    for (auto& f : takes.findChildFiles (juce::File::findFiles, false, ".retro-*.wav"))
    {
        const auto rel = f.getRelativePathFrom (projectFolder).replaceCharacter ('\\', '/');
        if (usedPaths.contains (rel))
            continue;   // 採用しているテイク（名前を付け直せなかった遡及録音）。消さない
        const auto dir = projectFolder.getChildFile ("Audio/Recovered");
        if (! dir.createDirectory().wasOk())
            continue;   // 移せなければ、そのまま置いておく（消さない）
        const auto dest = dir.getChildFile (f.getFileName().substring (1));   // 先頭の . を取る（隠しファイルにしない）
        if (f.moveFileTo (dest.exists() ? dest.getNonexistentSibling (false) : dest))
            ++moved;
    }
    return moved;
}
} // namespace vb::project
