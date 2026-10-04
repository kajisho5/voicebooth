#include "AnalysisCache.h"
#include <cstring>
#include <mutex>

namespace vb::analysis::cache
{
namespace
{
    constexpr juce::uint64 fnvPrime = 1099511628211ull;
    constexpr juce::uint32 pitchMagic = 0x31505642;   // "BVP1"
    constexpr juce::uint32 alignMagic = 0x31415642;   // "BVA1"

    std::mutex folderLock;
    juce::File cacheFolder;

    juce::File fileFor (juce::uint64 key, const char* kind)
    {
        const auto dir = folder();
        if (dir == juce::File())
            return {};
        return dir.getChildFile (juce::String (kind) + "-" + juce::String::toHexString ((juce::int64) key) + ".bin");
    }

    bool readFile (const juce::File& f, juce::MemoryBlock& data)
    {
        return f != juce::File() && f.existsAsFile() && f.loadFileAsData (data);
    }

    void writeFile (const juce::File& f, const juce::MemoryBlock& data)
    {
        if (f == juce::File() || ! f.getParentDirectory().createDirectory())
            return;
        // 書いている途中で落ちても壊れたファイルを読まないよう、隣に書いてから置き換える
        const auto tmp = f.getSiblingFile (f.getFileName() + ".tmp");
        if (tmp.replaceWithData (data.getData(), data.getSize()))
            tmp.moveFileTo (f);
        tmp.deleteFile();
    }

    struct Reader
    {
        const juce::MemoryBlock& data;
        size_t at = 0;
        bool ok = true;

        template <typename T> T get()
        {
            T v {};
            if (! ok || at + sizeof (T) > data.getSize()) { ok = false; return v; }
            std::memcpy (&v, static_cast<const char*> (data.getData()) + at, sizeof (T));
            at += sizeof (T);
            return v;
        }
    };

    template <typename T> void put (juce::MemoryOutputStream& out, T v) { out.write (&v, sizeof (T)); }
}

juce::uint64 hashSamples (const float* data, juce::int64 count, juce::uint64 seed)
{
    auto h = seed;
    for (juce::int64 i = 0; i < count; ++i)
    {
        juce::uint32 bits;
        std::memcpy (&bits, data + i, sizeof (bits));
        h = (h ^ bits) * fnvPrime;
    }
    return (h ^ (juce::uint64) count) * fnvPrime;
}

juce::uint64 mix (juce::uint64 h, const juce::String& text)
{
    for (auto c : text.toStdString())
        h = (h ^ (juce::uint8) c) * fnvPrime;
    return (h ^ 0xffu) * fnvPrime;
}

juce::uint64 mix (juce::uint64 h, double value)
{
    juce::uint64 bits;
    std::memcpy (&bits, &value, sizeof (bits));
    return (h ^ bits) * fnvPrime;
}

void setFolder (const juce::File& f)
{
    std::lock_guard<std::mutex> g (folderLock);
    cacheFolder = f;
}

juce::File folder()
{
    std::lock_guard<std::mutex> g (folderLock);
    return cacheFolder;
}

juce::MemoryBlock encodePitch (const std::vector<std::pair<float, float>>& frames)
{
    juce::MemoryOutputStream out;
    put (out, pitchMagic);
    put (out, (juce::uint32) frames.size());
    for (auto& [c, s] : frames)
    {
        put (out, c);
        put (out, s);
    }
    return out.getMemoryBlock();
}

bool decodePitch (const juce::MemoryBlock& data, std::vector<std::pair<float, float>>& frames)
{
    Reader r { data };
    if (r.get<juce::uint32>() != pitchMagic)
        return false;
    const auto n = r.get<juce::uint32>();
    if (! r.ok || (size_t) n * 8 != data.getSize() - r.at)
        return false;
    std::vector<std::pair<float, float>> f ((size_t) n);
    for (auto& p : f)
    {
        p.first = r.get<float>();
        p.second = r.get<float>();
    }
    if (! r.ok)
        return false;
    frames = std::move (f);
    return true;
}

juce::MemoryBlock encodeAlign (const AlignResult& a)
{
    juce::MemoryOutputStream out;
    put (out, alignMagic);
    put (out, (juce::int32) a.quality);
    put (out, (juce::int64) a.offsetSamples);
    put (out, a.tempoRatio);
    put (out, a.confidence);
    put (out, (juce::int32) a.windowsUsed);
    put (out, (juce::int32) a.windowsAgreeing);
    put (out, (juce::uint32) a.covered.size());
    for (auto& c : a.covered)
    {
        put (out, (juce::int64) c.karaokeStart);
        put (out, (juce::int64) c.karaokeEnd);
        put (out, (juce::int64) c.offsetSamples);
        put (out, c.tempoRatio);
    }
    return out.getMemoryBlock();
}

bool decodeAlign (const juce::MemoryBlock& data, AlignResult& result)
{
    Reader r { data };
    if (r.get<juce::uint32>() != alignMagic)
        return false;
    AlignResult a;
    const auto q = r.get<juce::int32>();
    if (q < 0 || q > (int) AlignResult::Quality::good)
        return false;
    a.quality = (AlignResult::Quality) q;
    a.offsetSamples = r.get<juce::int64>();
    a.tempoRatio = r.get<double>();
    a.confidence = r.get<double>();
    a.windowsUsed = r.get<juce::int32>();
    a.windowsAgreeing = r.get<juce::int32>();
    const auto n = r.get<juce::uint32>();
    if (! r.ok || (size_t) n * 32 != data.getSize() - r.at)
        return false;
    a.covered.resize ((size_t) n);
    for (auto& c : a.covered)
    {
        c.karaokeStart = r.get<juce::int64>();
        c.karaokeEnd = r.get<juce::int64>();
        c.offsetSamples = r.get<juce::int64>();
        c.tempoRatio = r.get<double>();
    }
    if (! r.ok)
        return false;
    result = std::move (a);
    return true;
}

bool loadPitch (juce::uint64 key, std::vector<std::pair<float, float>>& frames)
{
    juce::MemoryBlock data;
    return readFile (fileFor (key, "pitch"), data) && decodePitch (data, frames);
}

void savePitch (juce::uint64 key, const std::vector<std::pair<float, float>>& frames)
{
    writeFile (fileFor (key, "pitch"), encodePitch (frames));
}

bool loadAlign (juce::uint64 key, AlignResult& result)
{
    juce::MemoryBlock data;
    return readFile (fileFor (key, "align"), data) && decodeAlign (data, result);
}

void saveAlign (juce::uint64 key, const AlignResult& result)
{
    writeFile (fileFor (key, "align"), encodeAlign (result));
}
} // namespace vb::analysis::cache
