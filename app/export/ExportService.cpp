#include "ExportService.h"

namespace vb::exporter
{
namespace
{
    using project::int64;

    /** 採用区間 1 つ。読む範囲は [inA, outB)。頭の [inA, inB) で上がり、終わりの [outA, outB) で下がる。
        無音との境目は inA = inB = 区間の頭、outA = outB = 区間の終わり（フェードなし） */
    struct Piece
    {
        int64 start = 0, end = 0;          // 採用区間 [start, end)
        int64 inA = 0, inB = 0, outA = 0, outB = 0;
        juce::AudioFormatReader* reader = nullptr;
        int64 takeStart = 0, takeEnd = 0;  // テイクに音がある範囲（曲の位置）
    };

    /** 等パワーの重み */
    float gainAt (const Piece& p, int64 t) noexcept
    {
        if (t < p.inA || t >= p.outB)
            return 0.0f;
        float g = 1.0f;
        if (t < p.inB)
            g *= std::sin (juce::MathConstants<float>::halfPi * (float) (t - p.inA) / (float) (p.inB - p.inA));
        if (t >= p.outA)
            g *= std::cos (juce::MathConstants<float>::halfPi * (float) (t - p.outA) / (float) (p.outB - p.outA));
        return g;
    }

    /** 継ぎ目 seam（left → right）のクロスフェードの窓。継ぎ目を中心に幅 2h が基本。
        片方のテイクに音が無い所へはみ出す時は、両方に音がある所へずらす（録り始めの前は無音なので）。
        両方に音がある所が無ければ、継ぎ目でそのまま切り替える */
    std::pair<int64, int64> seamWindow (const Piece& left, const Piece& right, int64 seam, int64 h)
    {
        const auto lo = juce::jmax (left.takeStart, right.takeStart);
        const auto hi = juce::jmin (left.takeEnd, right.takeEnd);
        if (h <= 0 || hi - lo <= 1 || seam < lo || seam > hi)
            return { seam, seam };
        const auto width = juce::jmin (2 * h, hi - lo);
        const auto a = juce::jlimit (lo, hi - width, seam - width / 2);
        return { a, a + width };
    }
}

ExportResult ExportService::exportTrackDry (const project::Project& project, project::TrackType type,
                                            const juce::File& projectFolder, const juce::File& destination,
                                            const Options& options)
{
    ExportResult result;
    result.file = destination;

    const auto length = project.lengthSamples;
    const auto rate = project.sampleRate;
    if (length <= 0 || rate <= 0)
    {
        result.message = "no song";
        return result;
    }

    const auto* track = project.findTrack (type);
    if (track == nullptr || track->comp.empty())
    {
        result.message = "nothing recorded";
        return result;
    }

    // テイクを開く（SR が曲と違えば書き出さない：勝手に変換しない。DESIGN 6.5 / 13）
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::map<juce::String, std::unique_ptr<juce::AudioFormatReader>> readers;
    std::vector<Piece> pieces;
    for (size_t i = 0; i < track->comp.size(); ++i)
    {
        const auto& c = track->comp[i];
        const project::Take* take = nullptr;
        for (auto& k : track->takes)
            if (k.id == c.takeId)
                take = &k;
        if (take == nullptr)
        {
            result.message = "missing take " + c.takeId;
            return result;
        }

        auto& reader = readers[take->id];
        if (reader == nullptr)
        {
            const auto file = projectFolder.getChildFile (take->path);
            reader.reset (formats.createReaderFor (file));
            if (reader == nullptr)
            {
                result.message = "can't read " + take->path;
                return result;
            }
            if (std::abs (reader->sampleRate - (double) rate) > 0.5)
            {
                result.message = "sample rate of " + take->id + " differs from the song";
                return result;
            }
        }

        Piece p;
        p.start = juce::jmax ((int64) 0, c.startSample);
        p.end = juce::jmin (length, c.endSample);
        p.inA = p.inB = p.start;
        p.outA = p.outB = p.end;
        p.reader = reader.get();
        p.takeStart = take->startSample;
        p.takeEnd = take->startSample + reader->lengthInSamples;
        if (p.end > p.start)
            pieces.push_back (p);
    }

    // 別のテイク同士が接している所（継ぎ目）だけクロスフェードの窓を決める
    const auto h = (int64) std::llround (options.crossfadeMs * 0.001 * rate * 0.5);
    for (size_t i = 0; i + 1 < pieces.size(); ++i)
    {
        auto& l = pieces[i];
        auto& r = pieces[i + 1];
        if (l.end != r.start || l.reader == r.reader)
            continue;
        const auto [a, b] = seamWindow (l, r, l.end, h);
        l.outA = r.inA = a;
        l.outB = r.inB = b;
    }

    destination.getParentDirectory().createDirectory();
    const auto temp = destination.getSiblingFile (destination.getFileName() + ".part");
    temp.deleteFile();

    std::unique_ptr<juce::AudioFormatWriter> writer;
    {
        auto fileStream = std::make_unique<juce::FileOutputStream> (temp);
        if (! fileStream->openedOk())
        {
            result.message = fileStream->getStatus().getErrorMessage();
            return result;
        }
        std::unique_ptr<juce::OutputStream> stream (fileStream.release());
        juce::WavAudioFormat wav;
        // 24bit PCM か 32bit float（プロジェクトの録音形式）
        const bool asFloat = project.bitDepthExport >= 32;
        using Format = juce::AudioFormatWriterOptions::SampleFormat;
        writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate ((double) rate)
                                                                              .withNumChannels (1)
                                                                              .withBitsPerSample (asFloat ? 32 : 24)
                                                                              .withSampleFormat (asFloat ? Format::floatingPoint
                                                                                                         : Format::integral));
        if (writer == nullptr)
        {
            result.message = "can't create WAV writer";
            return result;
        }
    }

    // 2 ^ 16 サンプルずつ：無音で始め、重なる区間を重みを付けて足す
    constexpr int block = 65536;
    juce::AudioBuffer<float> out (1, block), take (1, block);   // 読む範囲はこのブロックの中だけ
    float peak = 0.0f;
    bool ok = true;

    for (int64 a = 0; a < length && ok; a += block)
    {
        const auto n = (int) juce::jmin<int64> (block, length - a);
        out.clear();
        auto* o = out.getWritePointer (0);

        for (auto& p : pieces)
        {
            const auto from = juce::jmax (a, p.inA);
            const auto to = juce::jmin (a + n, p.outB);
            if (to <= from)
                continue;

            // テイクの外（録っていない所）は 0 で埋まる
            const auto count = (int) (to - from);
            take.clear();
            p.reader->read (&take, 0, count, from - p.takeStart, true, false);
            const auto* t = take.getReadPointer (0);
            for (int i = 0; i < count; ++i)
                o[from - a + i] += t[i] * gainAt (p, from + i);
        }

        peak = juce::jmax (peak, out.getMagnitude (0, 0, n));
        const float* channels[] = { o };
        ok = writer->writeFromFloatArrays (channels, 1, n);
        if (ok && options.progress)
            ok = options.progress ((float) (a + n) / (float) length);
    }

    writer.reset();   // 閉じる（ヘッダーの長さが入る）
    if (! ok)
    {
        temp.deleteFile();
        result.message = "write failed or cancelled";
        return result;
    }

    destination.deleteFile();
    if (! temp.moveFileTo (destination))
    {
        temp.deleteFile();
        result.message = "can't move to " + destination.getFullPathName();
        return result;
    }

    result.ok = true;
    result.length = length;
    result.peak = peak;
    result.clipped = peak >= clipLevel;
    return result;
}
} // namespace vb::exporter
