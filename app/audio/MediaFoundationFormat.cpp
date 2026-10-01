#include "MediaFoundationFormat.h"
#include "Mp4Gapless.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <mfapi.h>
 #include <mfidl.h>
 #include <mfreadwrite.h>
 #include <mferror.h>

 #if JUCE_MSVC
  #pragma comment (lib, "mfplat.lib")
  #pragma comment (lib, "mfreadwrite.lib")
  #pragma comment (lib, "mfuuid.lib")
  #pragma comment (lib, "ole32.lib")
 #endif
#endif

namespace vb::audio
{
#if JUCE_WINDOWS
namespace
{
    /** COM の参照を持つだけの小さな入れ物 */
    template <typename T>
    struct Com
    {
        T* p = nullptr;
        Com() = default;
        Com (const Com&) = delete;
        Com& operator= (const Com&) = delete;
        ~Com() { reset(); }
        void reset() { if (p != nullptr) { p->Release(); p = nullptr; } }
        T** put() { reset(); return &p; }
        T* operator->() const { return p; }
        explicit operator bool() const { return p != nullptr; }
    };

    constexpr auto audioStream = (DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM;
    constexpr double hundredNs = 10000000.0;   // Media Foundation の時間単位（100 ns）

    class MediaFoundationReader final : public juce::AudioFormatReader
    {
    public:
        MediaFoundationReader (juce::InputStream* in, const juce::File& file)
            : juce::AudioFormatReader (in, "Media Foundation")
        {
            // このスレッドで COM と Media Foundation を使えるようにする（読み込みスレッドで作って捨てる）
            comResult = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
            mfStarted = SUCCEEDED (MFStartup (MF_VERSION, MFSTARTUP_LITE));
            if (mfStarted)
                opened = open (file);
        }

        ~MediaFoundationReader() override
        {
            reader.reset();
            if (mfStarted) MFShutdown();
            if (SUCCEEDED (comResult)) CoUninitialize();
        }

        bool isOpen() const { return opened; }

        bool readSamples (int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer,
                          juce::int64 startSampleInFile, int numSamples) override
        {
            // usesFloatingPointData = true のため、渡されるのは float の配列
            for (int c = 0; c < numDestChannels; ++c)
                if (destChannels[c] != nullptr)
                    juce::zeromem (destChannels[c] + startOffsetInDestBuffer, sizeof (float) * (size_t) numSamples);

            const auto channels = (int) numChannels;
            if (channels <= 0)
                return false;

            // 以下の位置は「頭からデコードした数」。頭の詰め物の分だけ先を読む
            startSampleInFile += skip;

            // 後ろへ戻るときは頭から読み直す（先へ進むときは読み進めて捨てる）
            if (startSampleInFile < pendingStart)
                rewind();

            auto pos = startSampleInFile;
            int done = 0;

            while (done < numSamples)
            {
                const auto end = pendingEnd();

                if (pos >= pendingStart && pos < end)
                {
                    const auto n = (int) juce::jmin ((juce::int64) (numSamples - done), end - pos);
                    const auto* src = pending.data() + (size_t) (pos - pendingStart) * (size_t) channels;
                    for (int c = 0; c < numDestChannels; ++c)
                    {
                        if (destChannels[c] == nullptr || c >= channels)
                            continue;
                        auto* dst = reinterpret_cast<float*> (destChannels[c]) + startOffsetInDestBuffer + done;
                        for (int i = 0; i < n; ++i)
                            dst[i] = src[(size_t) i * (size_t) channels + (size_t) c];
                    }
                    pos += n;
                    done += n;
                    continue;
                }

                // pos >= end：次を読む（使い終わった分は捨てる）
                if (ended)
                    break;   // 終わり以降は無音のまま

                pendingStart = end;
                pending.clear();
                if (! decodeNext())
                    ended = true;
            }

            return true;
        }

    private:
        juce::int64 pendingEnd() const
        {
            return pendingStart + (juce::int64) (pending.size() / juce::jmax ((size_t) 1, (size_t) numChannels));
        }

        bool open (const juce::File& file)
        {
            if (FAILED (MFCreateSourceReaderFromURL (file.getFullPathName().toWideCharPointer(), nullptr, reader.put())))
                return false;

            reader->SetStreamSelection ((DWORD) MF_SOURCE_READER_ALL_STREAMS, FALSE);
            if (FAILED (reader->SetStreamSelection (audioStream, TRUE)))
                return false;   // 音声が無い

            // 32bit float の PCM で受け取る（SR・チャンネルは元のまま）
            Com<IMFMediaType> wanted;
            if (FAILED (MFCreateMediaType (wanted.put()))
                || FAILED (wanted->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio))
                || FAILED (wanted->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_Float))
                || FAILED (reader->SetCurrentMediaType (audioStream, nullptr, wanted.p)))
                return false;

            Com<IMFMediaType> actual;
            UINT32 ch = 0, sr = 0;
            if (FAILED (reader->GetCurrentMediaType (audioStream, actual.put()))
                || FAILED (actual->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &ch))
                || FAILED (actual->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &sr))
                || ch == 0 || sr == 0)
                return false;

            numChannels = (unsigned int) ch;
            sampleRate = (double) sr;
            bitsPerSample = 32;
            usesFloatingPointData = true;

            // 長さ：コンテナの再生時間から（100 ns 単位）
            PROPVARIANT duration;
            PropVariantInit (&duration);
            if (SUCCEEDED (reader->GetPresentationAttribute ((DWORD) MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration))
                && duration.vt == VT_UI8)
                lengthInSamples = (juce::int64) std::llround ((double) duration.uhVal.QuadPart * sampleRate / hundredNs);
            PropVariantClear (&duration);

            // AAC の頭の詰め物：Media Foundation は MP4 の指示（edit list / iTunSMPB）を無視して出すので、ここで飛ばす
            if (! file.hasFileExtension ("mp3"))
                if (auto stream = file.createInputStream())
                {
                    const auto g = readMp4Gapless (*stream, sampleRate);
                    if (g.found)
                    {
                        skip = juce::jmax ((juce::int64) 0, g.priming);
                        if (g.validSamples > 0)
                            lengthInSamples = g.validSamples;
                    }
                }

            return lengthInSamples > 0;
        }

        /** 1 つ分デコードして pending に足す。終わり・失敗なら false */
        bool decodeNext()
        {
            for (int attempt = 0; attempt < 64; ++attempt)   // 空の通知（ギャップ等）を読み飛ばす
            {
                DWORD flags = 0;
                LONGLONG timestamp = 0;   // 使わない（位置はデコードした数で数える）
                Com<IMFSample> sample;
                if (FAILED (reader->ReadSample (audioStream, 0, nullptr, &flags, &timestamp, sample.put())))
                    return false;
                if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0 && ! sample)
                    return false;
                if (! sample)
                    continue;

                Com<IMFMediaBuffer> buffer;
                if (FAILED (sample->ConvertToContiguousBuffer (buffer.put())))
                    return false;

                BYTE* data = nullptr;
                DWORD bytes = 0;
                if (FAILED (buffer->Lock (&data, nullptr, &bytes)))
                    return false;
                const auto* f = reinterpret_cast<const float*> (data);
                pending.insert (pending.end(), f, f + bytes / sizeof (float));
                buffer->Unlock();

                return true;
            }
            return false;
        }

        /** 頭に戻る。位置は「頭からデコードしたサンプル数」だけで決める（時刻は使わない）
            AAC / mp3 は先頭の詰め物の扱いで時刻とサンプル位置がずれ得るため、
            どの順で読んでも同じサンプルが同じ位置に来ることを優先する（DESIGN 7.4 タイムコード精度）。
            長い曲で後ろへ戻ると遅いが、再生（B2）では内部 WAV を使う前提（DESIGN 7.1） */
        void rewind()
        {
            PROPVARIANT position;
            PropVariantInit (&position);
            position.vt = VT_I8;
            position.hVal.QuadPart = 0;
            reader->SetCurrentPosition (GUID_NULL, position);
            PropVariantClear (&position);

            pending.clear();
            pendingStart = 0;
            ended = false;
        }

        HRESULT comResult = E_FAIL;
        bool mfStarted = false, opened = false;
        Com<IMFSourceReader> reader;

        juce::int64 skip = 0;             // 頭で飛ばすサンプル数（AAC の詰め物）
        std::vector<float> pending;       // インターリーブ
        juce::int64 pendingStart = 0;     // pending の先頭のサンプル位置
        bool ended = false;
    };
}

MediaFoundationAudioFormat::MediaFoundationAudioFormat()
    : juce::AudioFormat ("Media Foundation", juce::StringArray { ".m4a", ".mp4", ".aac", ".mp3" })   // mp3 は minimp3 で開けないときの予備
{
}

juce::AudioFormatReader* MediaFoundationAudioFormat::createReaderFor (juce::InputStream* in, bool deleteStreamIfOpeningFails)
{
    // Media Foundation はパスから開く。ファイル以外のストリームは扱わない
    auto* fileStream = dynamic_cast<juce::FileInputStream*> (in);
    if (fileStream == nullptr)
    {
        if (deleteStreamIfOpeningFails)
            delete in;
        return nullptr;
    }

    auto reader = std::make_unique<MediaFoundationReader> (in, fileStream->getFile());
    if (reader->isOpen())
        return reader.release();

    if (! deleteStreamIfOpeningFails)
        reader->input = nullptr;   // 呼び出し側が持ち続ける
    return nullptr;
}
#endif

} // namespace vb::audio
