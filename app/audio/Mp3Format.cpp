#include "Mp3Format.h"

// minimp3 の実装はこの翻訳単位だけに置く。外部コードなので警告は抑える（中身は変更しない）
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wconversion", "-Wsign-conversion", "-Wshadow", "-Wshadow-field-in-constructor",
                                     "-Wcast-align", "-Wimplicit-fallthrough", "-Wunused-function", "-Wunused-parameter",
                                     "-Wmissing-prototypes", "-Wold-style-cast", "-Wzero-as-null-pointer-constant",
                                     "-Wuseless-cast", "-Wdeclaration-after-statement", "-Wfloat-conversion",
                                     "-Wimplicit-int-conversion", "-Wshorten-64-to-32", "-Wcomma", "-Wextra-semi-stmt",
                                     "-Wmissing-field-initializers", "-Wsign-compare", "-Wredundant-decls",
                                     "-Wundef", "-Wcast-qual", "-Wdouble-promotion", "-Wunused-but-set-variable",
                                     "-Wmisleading-indentation", "-Wpedantic", "-Wfloat-equal")
JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4100 4127 4189 4244 4245 4267 4389 4456 4457 4701 4703 4706 4996)
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO          // ファイルは JUCE の InputStream から読む（日本語パスを含め OS 差を作らない）
#include "../../third_party/minimp3/minimp3_ex.h"
JUCE_END_IGNORE_WARNINGS_MSVC
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

namespace vb::audio
{
namespace
{
    size_t readCallback (void* buffer, size_t size, void* user)
    {
        auto* stream = static_cast<juce::InputStream*> (user);
        const auto n = stream->read (buffer, (int) juce::jmin (size, (size_t) std::numeric_limits<int>::max()));
        return n > 0 ? (size_t) n : 0;
    }

    int seekCallback (uint64_t position, void* user)
    {
        return static_cast<juce::InputStream*> (user)->setPosition ((juce::int64) position) ? 0 : -1;
    }

    class Mp3Reader final : public juce::AudioFormatReader
    {
    public:
        explicit Mp3Reader (juce::InputStream* in)
            : juce::AudioFormatReader (in, "MP3"),
              decoder (std::make_unique<mp3dec_ex_t>())
        {
            io.read = readCallback;
            io.read_data = in;
            io.seek = seekCallback;
            io.seek_data = in;

            // MP3D_SEEK_TO_SAMPLE：サンプル単位でシーク。長さ・頭の詰め物は LAME / Xing タグ（無ければ全フレームを数える）
            if (mp3dec_ex_open_cb (decoder.get(), &io, MP3D_SEEK_TO_SAMPLE) != 0)
                return;

            const auto& info = decoder->info;
            if (info.channels <= 0 || info.hz <= 0 || decoder->samples == 0)
                return;

            numChannels = (unsigned int) info.channels;
            sampleRate = (double) info.hz;
            bitsPerSample = 32;
            usesFloatingPointData = true;
            lengthInSamples = (juce::int64) (decoder->samples / (uint64_t) info.channels);
            opened = true;
        }

        ~Mp3Reader() override
        {
            mp3dec_ex_close (decoder.get());
        }

        bool isOpen() const { return opened; }

        bool readSamples (int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer,
                          juce::int64 startSampleInFile, int numSamples) override
        {
            for (int c = 0; c < numDestChannels; ++c)
                if (destChannels[c] != nullptr)
                    juce::zeromem (destChannels[c] + startOffsetInDestBuffer, sizeof (float) * (size_t) numSamples);

            const auto channels = (int) numChannels;
            if (! opened || startSampleInFile >= lengthInSamples || startSampleInFile < 0)
                return opened;

            if (startSampleInFile != position)
            {
                if (mp3dec_ex_seek (decoder.get(), (uint64_t) startSampleInFile * (uint64_t) channels) != 0)
                    return false;
                position = startSampleInFile;
            }

            const auto wanted = (int) juce::jmin ((juce::int64) numSamples, lengthInSamples - startSampleInFile);
            interleaved.resize ((size_t) wanted * (size_t) channels);
            const auto got = (int) (mp3dec_ex_read (decoder.get(), interleaved.data(), interleaved.size()) / (size_t) channels);

            for (int c = 0; c < numDestChannels; ++c)
            {
                if (destChannels[c] == nullptr || c >= channels)
                    continue;
                auto* dst = reinterpret_cast<float*> (destChannels[c]) + startOffsetInDestBuffer;
                for (int i = 0; i < got; ++i)
                    dst[i] = interleaved[(size_t) i * (size_t) channels + (size_t) c];
            }

            position = startSampleInFile + got;
            return true;
        }

    private:
        std::unique_ptr<mp3dec_ex_t> decoder;   // 大きい（1 フレーム分のバッファを含む）ので確保して持つ
        mp3dec_io_t io {};
        bool opened = false;
        juce::int64 position = 0;               // 次に読むサンプル（ここと違う位置ならシーク）
        std::vector<float> interleaved;
    };
}

Mp3AudioFormat::Mp3AudioFormat()
    : juce::AudioFormat ("MP3", juce::StringArray { ".mp3" })
{
}

juce::AudioFormatReader* Mp3AudioFormat::createReaderFor (juce::InputStream* in, bool deleteStreamIfOpeningFails)
{
    auto reader = std::make_unique<Mp3Reader> (in);
    if (reader->isOpen())
        return reader.release();

    if (! deleteStreamIfOpeningFails)
        reader->input = nullptr;   // 呼び出し側が持ち続ける
    return nullptr;
}
} // namespace vb::audio
