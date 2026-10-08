#include "export/VideoEncoder.h"

/*  共有用の動画の MP4（DESIGN 9.1）。書いたファイルを読み戻して、長さ・トラック（映像と音）・色・上下の向きを確かめる。
    Windows（Media Foundation）・Mac（AVFoundation）は CI で必ず書く。Linux は ffmpeg があるときだけ */

namespace vb::video
{
namespace
{
    /** MP4 の箱（size + 4 文字の種類）を読む小さな読み手 */
    struct Mp4
    {
        juce::MemoryBlock data;
        juce::StringArray handlers;        // trak/mdia/hdlr の種類（vide / soun）
        juce::StringArray sampleEntries;   // stsd の中身（avc1 / mp4a）
        double movieSeconds = 0.0;
        int width = 0, height = 0;         // 映像のトラックの tkhd の幅・高さ（音のトラックは 0）

        explicit Mp4 (const juce::File& f)
        {
            f.loadFileAsData (data);
            walk (0, data.getSize(), 0);
        }

        juce::uint32 u32 (size_t at) const
        {
            auto* p = static_cast<const juce::uint8*> (data.getData()) + at;
            return ((juce::uint32) p[0] << 24) | ((juce::uint32) p[1] << 16) | ((juce::uint32) p[2] << 8) | p[3];
        }
        juce::uint64 u64 (size_t at) const { return ((juce::uint64) u32 (at) << 32) | u32 (at + 4); }
        juce::String type (size_t at) const { return juce::String (static_cast<const char*> (data.getData()) + at, 4); }

        void walk (size_t from, size_t to, int depth)
        {
            if (depth > 12)
                return;
            for (size_t at = from; at + 8 <= to;)
            {
                juce::uint64 size = u32 (at);
                size_t header = 8;
                if (size == 1 && at + 16 <= to) { size = u64 (at + 8); header = 16; }
                else if (size == 0)              size = to - at;
                if (size < header || at + size > to)
                    return;
                const auto t = type (at + 4);
                const auto body = at + header, end = (size_t) (at + size);
                if (t == "moov" || t == "trak" || t == "mdia" || t == "minf" || t == "stbl")
                    walk (body, end, depth + 1);
                else if (t == "hdlr" && body + 12 <= end)
                    handlers.add (type (body + 8));   // version/flags(4) + pre_defined(4) + handler_type
                else if (t == "stsd" && body + 8 <= end)
                {
                    // version/flags(4) + 数(4) のあとに見本の記述が並ぶ（それぞれ箱の形）
                    for (size_t e = body + 8; e + 8 <= end;)
                    {
                        const auto es = u32 (e);
                        if (es < 8 || e + es > end)
                            break;
                        sampleEntries.add (type (e + 4));
                        e += es;
                    }
                }
                else if (t == "tkhd" && body + 1 <= end)
                {
                    // version/flags(4) のあと、v0 は 20 バイト・v1 は 32 バイトの時刻などがあり、続く 8+8+36 バイトの後に幅・高さ（16.16）
                    const auto version = static_cast<const juce::uint8*> (data.getData())[body];
                    const auto at16 = body + (version == 1 ? 88 : 76);
                    if (at16 + 8 <= end)
                    {
                        width = juce::jmax (width, (int) (u32 (at16) >> 16));
                        height = juce::jmax (height, (int) (u32 (at16 + 4) >> 16));
                    }
                }
                else if (t == "mvhd" && body + 32 <= end)
                {
                    const auto version = static_cast<const juce::uint8*> (data.getData())[body];
                    const double scale = version == 1 ? u32 (body + 20) : u32 (body + 12);
                    const double duration = version == 1 ? (double) u64 (body + 24) : u32 (body + 16);
                    movieSeconds = scale > 0 ? duration / scale : 0.0;
                }
                at = end;
            }
        }
    };

    struct Rgb { int r, g, b; };
    Rgb rgbAt (const std::vector<juce::uint32>& px, int w, int x, int y)
    {
        const auto v = px[(size_t) y * (size_t) w + (size_t) x];
        return { (int) ((v >> 16) & 0xff), (int) ((v >> 8) & 0xff), (int) (v & 0xff) };
    }
} // namespace

class VideoEncoderTests final : public juce::UnitTest
{
public:
    VideoEncoderTests() : juce::UnitTest ("Share video encoder (MP4)", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("frame and audio timing");
        {
            expectEquals ((int) frameCount (2.0, 30), 60);
            expectEquals ((int) frameCount (2.01, 30), 61);   // 端数は 1 コマに
            expectEquals ((int) frameCount (0.0, 30), 0);
            expectEquals ((int) audioSamplesUntilFrame (0, 30), 1600);
            expectEquals ((int) audioSamplesUntilFrame (59, 30), 96000);   // 2 秒の終わり
        }

        beginTest ("writes an MP4 that reads back (H.264 + AAC, colours, top row first)");
        {
            // 書けない環境（Linux で ffmpeg がない・仮想マシンの Mac で H.264 の符号化器がない）：理由を返して、何も書かない
            if (! Encoder::available())
            {
                logMessage ("*** this machine can't write MP4: " + Encoder::problem() + " (DESIGN 9.1) ***");
               #if JUCE_WINDOWS
                expect (false, "Windows always has Media Foundation");
               #endif
                const auto f = juce::File::createTempFile (".mp4");
                auto enc = Encoder::create();
                expect (enc->open (f, {}, nullptr).isNotEmpty());
                expect (! f.exists());
                return;
            }

            Spec spec;
            spec.width = 648;    // 16 の倍数ではない（H.264 は 656 に詰めて符号化する。読み戻すと 648 に戻るか。1080 も同じ）。
            spec.height = 360;   // 小さすぎると Mac の符号化器が受け付けないことがある
            spec.fps = 30;
            spec.videoBitrate = 2000000;
            constexpr double seconds = 2.0;

            auto audio = std::make_shared<juce::AudioBuffer<float>> (2, (int) (seconds * audioRate));
            for (int i = 0; i < audio->getNumSamples(); ++i)
            {
                const auto v = 0.25f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / (float) audioRate);
                audio->setSample (0, i, v);
                audio->setSample (1, i, v);
            }

            // 上半分は赤、下半分は青（上下が逆さまに書かれたら分かる）
            const auto rowBytes = spec.width * 4;
            std::vector<juce::uint8> frame ((size_t) rowBytes * (size_t) spec.height);
            for (int y = 0; y < spec.height; ++y)
                for (int x = 0; x < spec.width; ++x)
                {
                    auto* p = frame.data() + (size_t) y * (size_t) rowBytes + (size_t) x * 4;
                    const bool top = y < spec.height / 2;
                    p[0] = top ? 0 : 255;   // B
                    p[1] = 0;               // G
                    p[2] = top ? 255 : 0;   // R
                    p[3] = 255;             // A
                }

            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("VoiceBoothTests-video-" + juce::String (juce::Random::getSystemRandom().nextInt64()) + ".mp4");
            auto enc = Encoder::create();
            auto error = enc->open (file, spec, audio);
            expect (error.isEmpty(), error);
            const auto frames = frameCount (seconds, spec.fps);
            for (juce::int64 i = 0; i < frames && error.isEmpty(); ++i)
                error = enc->addFrame (frame.data(), rowBytes);
            expect (error.isEmpty(), error);
            if (error.isEmpty())
                error = enc->finish();
            expect (error.isEmpty(), error);
            enc.reset();

            expect (file.existsAsFile());
            const Mp4 mp4 (file);
            logMessage ("MP4: " + juce::String (file.getSize()) + " bytes, " + juce::String (mp4.movieSeconds, 3) + " s, handlers "
                        + mp4.handlers.joinIntoString (",") + ", entries " + mp4.sampleEntries.joinIntoString (","));
            expect (mp4.handlers.contains ("vide"), mp4.handlers.joinIntoString (","));
            expect (mp4.handlers.contains ("soun"), mp4.handlers.joinIntoString (","));
            expect (mp4.sampleEntries.contains ("avc1"), mp4.sampleEntries.joinIntoString (","));
            expect (mp4.sampleEntries.contains ("mp4a"), mp4.sampleEntries.joinIntoString (","));
            expect (std::abs (mp4.movieSeconds - seconds) < 0.15, juce::String (mp4.movieSeconds, 3));
            expectEquals (mp4.width, spec.width, "tkhd width");
            expectEquals (mp4.height, spec.height, "tkhd height");

            int w = 0, h = 0;
            std::vector<juce::uint32> px;
            const bool read = readFrame (file, 1.0, w, h, px);
            expect (read, "readFrame");
            if (read)
            {
                expectEquals (w, spec.width);
                expectEquals (h, spec.height);
                if (w == spec.width && h == spec.height)
                {
                    const auto top = rgbAt (px, w, w / 2, h / 8), bottom = rgbAt (px, w, w / 2, h * 7 / 8);
                    logMessage ("top " + juce::String (top.r) + "," + juce::String (top.g) + "," + juce::String (top.b)
                                + " bottom " + juce::String (bottom.r) + "," + juce::String (bottom.g) + "," + juce::String (bottom.b));
                    expect (top.r > 180 && top.b < 80 && top.g < 80, "top should be red");
                    expect (bottom.b > 180 && bottom.r < 80 && bottom.g < 80, "bottom should be blue");
                    // 右端（詰め物の列が混ざっていない）
                    const auto edge = rgbAt (px, w, w - 2, h / 8);
                    expect (edge.r > 180 && edge.b < 80 && edge.g < 80,
                            "right edge should be red: " + juce::String (edge.r) + "," + juce::String (edge.g) + "," + juce::String (edge.b));
                }
            }
            file.deleteFile();
        }

        beginTest ("a failed write leaves no file");
        {
            if (! Encoder::available())
                return;
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("VoiceBoothTests-video-abandon-" + juce::String (juce::Random::getSystemRandom().nextInt64()) + ".mp4");
            {
                auto enc = Encoder::create();
                Spec spec;
                spec.width = 64;
                spec.height = 64;
                std::vector<juce::uint8> frame (64 * 64 * 4, 128);
                expect (enc->open (file, spec, nullptr).isEmpty());
                expect (enc->addFrame (frame.data(), 64 * 4).isEmpty());
                // finish を呼ばずに捨てる（中止）
            }
            expect (! file.exists());
        }
    }
};

static VideoEncoderTests videoEncoderTests;
} // namespace vb::video
