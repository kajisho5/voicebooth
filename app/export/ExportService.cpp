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
        // 窓は左の区間の頭から右の区間の終わりまでに収める（短い区間で窓がはみ出し、その先の区間と 3 つ重なって
        // 大きく鳴った。バグチェック 2026-10-05）
        const auto lo = juce::jmax (left.takeStart, right.takeStart, left.start);
        const auto hi = juce::jmin (left.takeEnd, right.takeEnd, right.end);
        if (h <= 0 || hi - lo <= 1 || seam < lo || seam > hi)
            return { seam, seam };
        const auto width = juce::jmin (2 * h, hi - lo, left.end - left.start, right.end - right.start);   // 短い区間では窓も短く
        const auto a = juce::jlimit (lo, hi - width, seam - width / 2);
        return { a, a + width };
    }

    /** 1 トラックの採用区間をつないだ音（書き出しと試聴で同じ計算にする。B5 / B12） */
    struct CompSource
    {
        juce::AudioFormatManager formats;
        std::map<juce::String, std::unique_ptr<juce::AudioFormatReader>> readers;
        std::vector<Piece> pieces;
        juce::AudioBuffer<float> take { 1, 65536 };

        /** 開けなければ理由（英語の短い文） */
        juce::String open (const project::Project& project, project::TrackType type, const juce::File& projectFolder, double crossfadeMs)
        {
            const auto length = project.lengthSamples;
            const auto rate = project.sampleRate;
            const auto* track = project.findTrack (type);
            if (track == nullptr || track->comp.empty())
                return "nothing recorded";

            // テイクを開く（SR が曲と違えば書き出さない：勝手に変換しない。DESIGN 6.5 / 13）
            formats.registerBasicFormats();
            for (size_t i = 0; i < track->comp.size(); ++i)
            {
                const auto& c = track->comp[i];
                const project::Take* tk = nullptr;
                for (auto& k : track->takes)
                    if (k.id == c.takeId)
                        tk = &k;
                if (tk == nullptr)
                    return "missing take " + c.takeId;

                auto& reader = readers[tk->id];
                if (reader == nullptr)
                {
                    const auto file = projectFolder.getChildFile (tk->path);
                    reader.reset (formats.createReaderFor (file));
                    if (reader == nullptr)
                        return "can't read " + tk->path;
                    if (std::abs (reader->sampleRate - (double) rate) > 0.5)
                        return "sample rate of " + tk->id + " differs from the song";
                }

                Piece p;
                p.start = juce::jmax ((int64) 0, c.startSample);
                p.end = juce::jmin (length, c.endSample);
                p.inA = p.inB = p.start;
                p.outA = p.outB = p.end;
                p.reader = reader.get();
                p.takeStart = tk->startSample;
                p.takeEnd = tk->startSample + reader->lengthInSamples;
                if (p.end > p.start)
                    pieces.push_back (p);
            }

            // 別のテイク同士が接している所（継ぎ目）だけクロスフェードの窓を決める
            const auto h = (int64) std::llround (crossfadeMs * 0.001 * rate * 0.5);
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
            return {};
        }

        /** [a, a + n) を o に書く（n は 65536 まで）。無音で始め、重なる区間を重みを付けて足す */
        void render (int64 a, int n, float* o)
        {
            juce::FloatVectorOperations::clear (o, n);
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
        }
    };
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

    CompSource source;
    if (const auto error = source.open (project, type, projectFolder, options.crossfadeMs); error.isNotEmpty())
    {
        result.message = error;
        return result;
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
        // 16bit / 24bit PCM か 32bit float（project.bitDepthExport）
        const bool asFloat = project.bitDepthExport >= 32;
        const int bits = asFloat ? 32 : (project.bitDepthExport <= 16 ? 16 : 24);
        using Format = juce::AudioFormatWriterOptions::SampleFormat;
        writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate ((double) rate)
                                                                              .withNumChannels (1)
                                                                              .withBitsPerSample (bits)
                                                                              .withSampleFormat (asFloat ? Format::floatingPoint
                                                                                                         : Format::integral));
        if (writer == nullptr)
        {
            result.message = "can't create WAV writer";
            return result;
        }
    }

    // 2 ^ 16 サンプルずつ
    constexpr int block = 65536;
    juce::AudioBuffer<float> out (1, block);
    float peak = 0.0f;
    bool ok = true;

    // 16bit は TPDF ディザー（±1 LSB の三角分布）を掛けてから丸める。録っていない所（ちょうど 0）には掛けない
    // （無音はデジタルの無音のまま）。乱数の種は固定（同じ素材なら毎回同じファイル）
    const bool dither16 = project.bitDepthExport <= 16;
    juce::Random dither (0x5eed);
    constexpr float lsb16 = 1.0f / 32768.0f;

    // 整数の形式は自分で「いちばん近い値」に丸めて渡す。JUCE の float → int32 → int16 / int24 は最後が切り捨て
    // （右シフト）で、0.5 LSB の偏り（ごく小さい直流）が出るため。値は 32bit の上詰め（16bit なら × 65536）
    const bool asInt = project.bitDepthExport < 32;
    const double intScale = dither16 ? 32768.0 : 8388608.0;
    const int intMax = dither16 ? 32767 : 8388607, intShift = dither16 ? 65536 : 256;
    std::vector<int> ints (asInt ? (size_t) block : 0);

    for (int64 a = 0; a < length && ok; a += block)
    {
        const auto n = (int) juce::jmin<int64> (block, length - a);
        auto* o = out.getWritePointer (0);
        source.render (a, n, o);

        peak = juce::jmax (peak, out.getMagnitude (0, 0, n));
        if (dither16)
            for (int i = 0; i < n; ++i)
                if (! juce::exactlyEqual (o[i], 0.0f))
                    o[i] += (dither.nextFloat() - dither.nextFloat()) * lsb16;
        if (asInt)
        {
            for (int i = 0; i < n; ++i)
                ints[(size_t) i] = juce::jlimit (-intMax - 1, intMax, juce::roundToInt ((double) o[i] * intScale)) * intShift;
            const int* channels[] = { ints.data(), nullptr };
            ok = writer->write (channels, n);
        }
        else
        {
            const float* channels[] = { o };
            ok = writer->writeFromFloatArrays (channels, 1, n);
        }
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
    // 最後のブロックとヘッダーは閉じる時に書かれ、失敗しても分からない。読み直して長さを確かめる
    // （ディスクが一杯・書き出し中にドライブが外れた時に、壊れた WAV を「書き出しました」としていた。監査 2026-10-04）
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> check (wav.createReaderFor (temp.createInputStream().release(), true));
        if (check == nullptr || (juce::int64) check->lengthInSamples != (juce::int64) length)
        {
            temp.deleteFile();
            result.message = "write failed (the file is incomplete)";
            return result;
        }
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

ExportResult ExportService::renderTrackDry (const project::Project& project, project::TrackType type, const juce::File& projectFolder,
                                            juce::AudioBuffer<float>& out, const Options& options)
{
    ExportResult result;
    const auto length = project.lengthSamples;
    if (length <= 0 || project.sampleRate <= 0 || length > std::numeric_limits<int>::max())
    {
        result.message = "no song";
        return result;
    }

    CompSource source;
    if (const auto error = source.open (project, type, projectFolder, options.crossfadeMs); error.isNotEmpty())
    {
        result.message = error;
        return result;
    }

    out.setSize (1, (int) length, false, false, true);
    constexpr int block = 65536;
    float peak = 0.0f;
    for (int64 a = 0; a < length; a += block)
    {
        const auto n = (int) juce::jmin<int64> (block, length - a);
        source.render (a, n, out.getWritePointer (0, (int) a));
        peak = juce::jmax (peak, out.getMagnitude (0, (int) a, n));
        if (options.progress && ! options.progress ((float) (a + n) / (float) length))
        {
            result.message = "cancelled";
            return result;
        }
    }
    result.ok = true;
    result.length = length;
    result.peak = peak;
    result.clipped = peak >= clipLevel;
    return result;
}
} // namespace vb::exporter
