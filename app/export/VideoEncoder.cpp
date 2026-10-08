#include "VideoEncoder.h"
#include <juce_audio_formats/juce_audio_formats.h>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <mfapi.h>
 #include <mfidl.h>
 #include <mfreadwrite.h>
 #include <mferror.h>
 #include <codecapi.h>

 #if JUCE_MSVC
  #pragma comment (lib, "mfplat.lib")
  #pragma comment (lib, "mfreadwrite.lib")
  #pragma comment (lib, "mfuuid.lib")
  #pragma comment (lib, "ole32.lib")
 #endif
#elif JUCE_MAC
 #import <AVFoundation/AVFoundation.h>
 #import <CoreMedia/CoreMedia.h>
 #import <CoreVideo/CoreVideo.h>
 #import <VideoToolbox/VideoToolbox.h>
#else
 #include <cstdio>
 #include <csignal>
 #include <unistd.h>
 #include <sys/wait.h>
#endif

namespace vb::video
{
#if JUCE_WINDOWS || JUCE_MAC
namespace
{
    /** 音を 1 回に渡す長さ（0.1 秒） */
    constexpr int audioChunk = audioRate / 10;

    float sampleAt (const juce::AudioBuffer<float>& a, int channel, juce::int64 i)
    {
        return a.getSample (juce::jmin (channel, a.getNumChannels() - 1), (int) i);
    }
} // namespace
#endif

#if JUCE_WINDOWS
//==============================================================================
// Windows：Media Foundation の Sink Writer（H.264 + AAC。OS に入っている符号化器を使う）
namespace
{
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

    juce::String hrText (const char* what, HRESULT h)
    {
        return juce::String (what) + " failed (0x" + juce::String::toHexString ((int) h).paddedLeft ('0', 8) + ")";
    }

    constexpr LONGLONG hundredNs = 10000000;   // Media Foundation の時間単位（100 ns）

    /** このスレッドで COM と Media Foundation を使う間だけ持つ */
    struct MfScope
    {
        HRESULT co = E_FAIL;
        bool started = false;
        MfScope()
        {
            co = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
            started = SUCCEEDED (MFStartup (MF_VERSION));
        }
        ~MfScope()
        {
            if (started) MFShutdown();
            if (SUCCEEDED (co)) CoUninitialize();   // RPC_E_CHANGED_MODE（別の形で初期化済み）の時は呼ばない
        }
    };

    class MfEncoder final : public Encoder
    {
    public:
        ~MfEncoder() override { abandon(); }

        juce::String open (const juce::File& d, const Spec& s, std::shared_ptr<const juce::AudioBuffer<float>> a) override
        {
            dest = d;
            spec = s;
            audio = a != nullptr && a->getNumSamples() > 0 && a->getNumChannels() > 0 ? a : nullptr;
            mf = std::make_unique<MfScope>();
            if (! mf->started)
                return "Media Foundation is not available";
            dest.deleteFile();

            Com<IMFAttributes> attr;
            HRESULT h = MFCreateAttributes (attr.put(), 1);
            if (SUCCEEDED (h)) h = attr->SetGUID (MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
            if (SUCCEEDED (h)) h = MFCreateSinkWriterFromURL (dest.getFullPathName().toWideCharPointer(), nullptr, attr.p, writer.put());
            if (FAILED (h)) return fail ("MFCreateSinkWriterFromURL", h);

            // 映像：H.264（Main）。入力は RGB32（上の行から。DEFAULT_STRIDE を正にする）。色の変換は Sink Writer が入れる
            {
                Com<IMFMediaType> out;
                h = MFCreateMediaType (out.put());
                if (SUCCEEDED (h)) h = out->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
                if (SUCCEEDED (h)) h = out->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_H264);
                if (SUCCEEDED (h)) h = out->SetUINT32 (MF_MT_AVG_BITRATE, (UINT32) spec.videoBitrate);
                if (SUCCEEDED (h)) h = out->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
                if (SUCCEEDED (h)) h = out->SetUINT32 (MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
                if (SUCCEEDED (h)) h = MFSetAttributeSize (out.p, MF_MT_FRAME_SIZE, (UINT32) spec.width, (UINT32) spec.height);
                if (SUCCEEDED (h)) h = MFSetAttributeRatio (out.p, MF_MT_FRAME_RATE, (UINT32) spec.fps, 1);
                if (SUCCEEDED (h)) h = MFSetAttributeRatio (out.p, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
                if (SUCCEEDED (h)) h = writer->AddStream (out.p, &videoStream);
                if (FAILED (h)) return fail ("H.264 output", h);

                Com<IMFMediaType> in;
                h = MFCreateMediaType (in.put());
                if (SUCCEEDED (h)) h = in->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
                if (SUCCEEDED (h)) h = in->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_RGB32);
                if (SUCCEEDED (h)) h = in->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
                if (SUCCEEDED (h)) h = in->SetUINT32 (MF_MT_DEFAULT_STRIDE, (UINT32) (spec.width * 4));
                if (SUCCEEDED (h)) h = MFSetAttributeSize (in.p, MF_MT_FRAME_SIZE, (UINT32) spec.width, (UINT32) spec.height);
                if (SUCCEEDED (h)) h = MFSetAttributeRatio (in.p, MF_MT_FRAME_RATE, (UINT32) spec.fps, 1);
                if (SUCCEEDED (h)) h = MFSetAttributeRatio (in.p, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
                if (SUCCEEDED (h)) h = writer->SetInputMediaType (videoStream, in.p, nullptr);
                if (FAILED (h)) return fail ("H.264 input", h);
            }

            // 音：AAC（48 kHz・ステレオ）。入力は 16 bit の PCM（Windows の AAC の符号化器が受け取る形）
            if (audio != nullptr)
            {
                Com<IMFMediaType> out;
                h = MFCreateMediaType (out.put());
                if (SUCCEEDED (h)) h = out->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                if (SUCCEEDED (h)) h = out->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_AAC);
                if (SUCCEEDED (h)) h = out->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                if (SUCCEEDED (h)) h = out->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) audioRate);
                if (SUCCEEDED (h)) h = out->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
                if (SUCCEEDED (h)) h = out->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32) (aacBitrate (spec.audioBitrate) / 8));
                if (SUCCEEDED (h)) h = writer->AddStream (out.p, &audioStream);
                if (FAILED (h)) return fail ("AAC output", h);

                Com<IMFMediaType> in;
                h = MFCreateMediaType (in.put());
                if (SUCCEEDED (h)) h = in->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                if (SUCCEEDED (h)) h = in->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
                if (SUCCEEDED (h)) h = in->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                if (SUCCEEDED (h)) h = in->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) audioRate);
                if (SUCCEEDED (h)) h = in->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
                if (SUCCEEDED (h)) h = in->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
                if (SUCCEEDED (h)) h = in->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32) (audioRate * 4));
                if (SUCCEEDED (h)) h = writer->SetInputMediaType (audioStream, in.p, nullptr);
                if (FAILED (h)) return fail ("AAC input", h);
            }

            h = writer->BeginWriting();
            if (FAILED (h)) return fail ("BeginWriting", h);
            return {};
        }

        juce::String addFrame (const juce::uint8* bgra, int lineStride) override
        {
            if (! writer)
                return "not open";
            const auto rowBytes = (DWORD) spec.width * 4;
            const auto size = rowBytes * (DWORD) spec.height;
            Com<IMFMediaBuffer> buffer;
            HRESULT h = MFCreateMemoryBuffer (size, buffer.put());
            BYTE* p = nullptr;
            if (SUCCEEDED (h)) h = buffer->Lock (&p, nullptr, nullptr);
            if (SUCCEEDED (h))
            {
                h = MFCopyImage (p, (LONG) rowBytes, bgra, (LONG) lineStride, rowBytes, (DWORD) spec.height);
                buffer->Unlock();
            }
            if (SUCCEEDED (h)) h = buffer->SetCurrentLength (size);
            Com<IMFSample> sample;
            if (SUCCEEDED (h)) h = MFCreateSample (sample.put());
            if (SUCCEEDED (h)) h = sample->AddBuffer (buffer.p);
            const auto t0 = frames * hundredNs / spec.fps, t1 = (frames + 1) * hundredNs / spec.fps;
            if (SUCCEEDED (h)) h = sample->SetSampleTime (t0);
            if (SUCCEEDED (h)) h = sample->SetSampleDuration (t1 - t0);
            if (SUCCEEDED (h)) h = writer->WriteSample (videoStream, sample.p);
            if (FAILED (h)) return fail ("WriteSample (video)", h);
            ++frames;
            return writeAudioUntil (audioSamplesUntilFrame (frames - 1, spec.fps));
        }

        juce::String finish() override
        {
            if (! writer)
                return "not open";
            if (auto e = writeAudioUntil (std::numeric_limits<juce::int64>::max()); e.isNotEmpty())
                return e;
            const auto h = writer->Finalize();
            writer.reset();
            if (FAILED (h))
                return fail ("Finalize", h);
            mf.reset();
            if (! dest.existsAsFile() || dest.getSize() <= 0)
                return "no file written";
            return {};
        }

    private:
        static int aacBitrate (int bps)
        {
            // Windows の AAC の符号化器が受け取る値（96 / 128 / 160 / 192 kbps）のうち、いちばん近いもの
            const int allowed[] = { 96000, 128000, 160000, 192000 };
            int best = allowed[3];
            for (auto v : allowed)
                if (std::abs (v - bps) < std::abs (best - bps))
                    best = v;
            return best;
        }

        juce::String writeAudioUntil (juce::int64 end)
        {
            if (audio == nullptr)
                return {};
            end = juce::jmin (end, (juce::int64) audio->getNumSamples());
            while (audioPos < end)
            {
                const auto n = (int) juce::jmin<juce::int64> (audioChunk, end - audioPos);
                const auto bytes = (DWORD) n * 4;
                Com<IMFMediaBuffer> buffer;
                HRESULT h = MFCreateMemoryBuffer (bytes, buffer.put());
                BYTE* p = nullptr;
                if (SUCCEEDED (h)) h = buffer->Lock (&p, nullptr, nullptr);
                if (SUCCEEDED (h))
                {
                    auto* w = reinterpret_cast<juce::int16*> (p);
                    for (int i = 0; i < n; ++i)
                        for (int c = 0; c < 2; ++c)
                        {
                            const auto v = juce::jlimit (-1.0f, 1.0f, sampleAt (*audio, c, audioPos + i));
                            w[i * 2 + c] = (juce::int16) juce::roundToInt (v * 32767.0f);
                        }
                    buffer->Unlock();
                }
                if (SUCCEEDED (h)) h = buffer->SetCurrentLength (bytes);
                Com<IMFSample> sample;
                if (SUCCEEDED (h)) h = MFCreateSample (sample.put());
                if (SUCCEEDED (h)) h = sample->AddBuffer (buffer.p);
                const auto t0 = audioPos * hundredNs / audioRate, t1 = (audioPos + n) * hundredNs / audioRate;
                if (SUCCEEDED (h)) h = sample->SetSampleTime (t0);
                if (SUCCEEDED (h)) h = sample->SetSampleDuration (t1 - t0);
                if (SUCCEEDED (h)) h = writer->WriteSample (audioStream, sample.p);
                if (FAILED (h)) return fail ("WriteSample (audio)", h);
                audioPos += n;
            }
            return {};
        }

        juce::String fail (const char* what, HRESULT h)
        {
            abandon();
            return hrText (what, h);
        }

        void abandon()
        {
            const bool hadWriter = (bool) writer;
            writer.reset();   // Finalize していない書きかけは閉じて消す
            mf.reset();
            if (hadWriter)
                dest.deleteFile();
        }

        juce::File dest;
        Spec spec;
        std::shared_ptr<const juce::AudioBuffer<float>> audio;
        std::unique_ptr<MfScope> mf;
        Com<IMFSinkWriter> writer;
        DWORD videoStream = 0, audioStream = 0;
        LONGLONG frames = 0;
        juce::int64 audioPos = 0;
    };
} // namespace

juce::String Encoder::problem() { return {}; }   // Media Foundation が無い Windows（N エディション）は open で理由を返す
std::unique_ptr<Encoder> Encoder::create() { return std::make_unique<MfEncoder>(); }

bool readFrame (const juce::File& file, double seconds, int& width, int& height, std::vector<juce::uint32>& argb)
{
    MfScope mf;
    if (! mf.started)
        return false;
    Com<IMFAttributes> attr;
    if (FAILED (MFCreateAttributes (attr.put(), 1)) || FAILED (attr->SetUINT32 (MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE)))
        return false;
    Com<IMFSourceReader> reader;
    if (FAILED (MFCreateSourceReaderFromURL (file.getFullPathName().toWideCharPointer(), attr.p, reader.put())))
        return false;
    constexpr auto stream = (DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM;
    reader->SetStreamSelection ((DWORD) MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection (stream, TRUE);
    Com<IMFMediaType> want;
    if (FAILED (MFCreateMediaType (want.put())) || FAILED (want->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video))
        || FAILED (want->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_RGB32)) || FAILED (reader->SetCurrentMediaType (stream, nullptr, want.p)))
        return false;

    const auto target = (LONGLONG) (seconds * (double) hundredNs);
    if (target > 0)
    {
        PROPVARIANT pos;
        PropVariantInit (&pos);
        pos.vt = VT_I8;
        pos.hVal.QuadPart = target;
        reader->SetCurrentPosition (GUID_NULL, pos);
        PropVariantClear (&pos);
    }

    Com<IMFSample> sample;
    for (int tries = 0; tries < 600; ++tries)
    {
        DWORD flags = 0;
        LONGLONG time = 0;
        Com<IMFSample> s;
        if (FAILED (reader->ReadSample (stream, 0, nullptr, &flags, &time, s.put())))
            return false;
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
            break;
        if (! s)
            continue;
        LONGLONG duration = 0;
        s->GetSampleDuration (&duration);
        if (time + duration <= target)   // 動かした先のキーフレームから届くので、目当ての時刻まで読み進める
            continue;
        sample.p = s.p;
        s.p = nullptr;
        break;
    }
    if (! sample)
        return false;

    Com<IMFMediaType> current;
    UINT32 w = 0, h = 0;
    if (FAILED (reader->GetCurrentMediaType (stream, current.put())) || FAILED (MFGetAttributeSize (current.p, MF_MT_FRAME_SIZE, &w, &h)))
        return false;
    LONG stride = 0;
    UINT32 rawStride = 0;
    if (SUCCEEDED (current->GetUINT32 (MF_MT_DEFAULT_STRIDE, &rawStride)))
        stride = (LONG) rawStride;
    else if (FAILED (MFGetStrideForBitmapInfoHeader (MFVideoFormat_RGB32.Data1, w, &stride)))
        return false;

    // H.264 は 16 の倍数に詰めて符号化するので、復号した画は大きいことがある（幅 1080 → 1088）。見せる範囲だけ切り出す
    UINT32 cropX = 0, cropY = 0, outW = w, outH = h;
    for (auto& key : { MF_MT_MINIMUM_DISPLAY_APERTURE, MF_MT_GEOMETRIC_APERTURE })
    {
        MFVideoArea area {};
        if (SUCCEEDED (current->GetBlob (key, reinterpret_cast<UINT8*> (&area), sizeof (area), nullptr))
            && area.Area.cx > 0 && area.Area.cy > 0)
        {
            cropX = (UINT32) juce::jmax (0, (int) area.OffsetX.value);
            cropY = (UINT32) juce::jmax (0, (int) area.OffsetY.value);
            outW = juce::jmin ((UINT32) area.Area.cx, w - juce::jmin (w, cropX));
            outH = juce::jmin ((UINT32) area.Area.cy, h - juce::jmin (h, cropY));
            break;
        }
    }

    Com<IMFMediaBuffer> buffer;
    if (FAILED (sample->ConvertToContiguousBuffer (buffer.put())))
        return false;
    width = (int) outW;
    height = (int) outH;
    argb.assign ((size_t) outW * outH, 0);
    auto copyRows = [&] (const BYTE* firstRow, LONG pitch)
    {
        for (UINT32 y = 0; y < outH; ++y)
        {
            auto* row = reinterpret_cast<const juce::uint32*> (firstRow + (LONGLONG) pitch * (y + cropY));
            for (UINT32 x = 0; x < outW; ++x)
                argb[(size_t) y * outW + x] = row[x + cropX] | 0xff000000u;
        }
    };
    Com<IMF2DBuffer> buffer2d;
    if (SUCCEEDED (buffer->QueryInterface (__uuidof (IMF2DBuffer), (void**) buffer2d.put())))
    {
        BYTE* scan0 = nullptr;
        LONG pitch = 0;
        if (FAILED (buffer2d->Lock2D (&scan0, &pitch)))
            return false;
        copyRows (scan0, pitch);   // 下の行からの画像でも scan0 は上の行（pitch が負）
        buffer2d->Unlock2D();
        return true;
    }
    BYTE* p = nullptr;
    DWORD length = 0;
    if (FAILED (buffer->Lock (&p, nullptr, &length)))
        return false;
    const auto absStride = std::abs (stride);
    const bool ok = (LONGLONG) absStride * h <= (LONGLONG) length;
    if (ok)
        copyRows (stride >= 0 ? p : p + (LONGLONG) absStride * (h - 1), stride);
    buffer->Unlock();
    return ok;
}

#elif JUCE_MAC
//==============================================================================
// Mac：AVFoundation の AVAssetWriter（H.264 + AAC）。このファイルは参照カウントを手で持つ（ARC なし）
namespace
{
    NSString* toNS (const juce::String& s) { return [NSString stringWithUTF8String: s.toRawUTF8()]; }

    /** H.264 で 1 コマ符号化できるか（できなければ理由）。仮想マシンの Mac（CI）には符号化器がなく、AVAssetWriter が
        エラーにならないまま映像を受け取らなくなった（2026-10-08）。止まっても待ち続けないよう、5 秒で打ち切る
        （打ち切ったときは、止まった符号化器の後始末をしない：解放すると、まだ動いている処理が使うため） */
    juce::String probeH264()
    {
        constexpr int w = 1280, h = 720;
        VTCompressionSessionRef session = nullptr;
        auto st = VTCompressionSessionCreate (kCFAllocatorDefault, w, h, kCMVideoCodecType_H264, nullptr, nullptr, nullptr,
                                              nullptr, nullptr, &session);
        if (st != noErr || session == nullptr)
            return "H.264 encoder not available (" + juce::String ((int) st) + ")";
        VTSessionSetProperty (session, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
        VTSessionSetProperty (session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
        CVPixelBufferRef pb = nullptr;
        if (CVPixelBufferCreate (kCFAllocatorDefault, w, h, kCVPixelFormatType_32BGRA, nullptr, &pb) != kCVReturnSuccess || pb == nullptr)
        {
            VTCompressionSessionInvalidate (session);
            CFRelease (session);
            return "can't make a pixel buffer";
        }
        CVPixelBufferLockBaseAddress (pb, 0);
        std::memset (CVPixelBufferGetBaseAddress (pb), 0, CVPixelBufferGetBytesPerRow (pb) * (size_t) h);
        CVPixelBufferUnlockBaseAddress (pb, 0);

        dispatch_semaphore_t encoded = dispatch_semaphore_create (0), completed = dispatch_semaphore_create (0);
        __block OSStatus result = -1;
        __block bool gotSample = false;
        st = VTCompressionSessionEncodeFrameWithOutputHandler (session, pb, CMTimeMake (0, 30), kCMTimeInvalid, nullptr, nullptr,
                                                               ^(OSStatus s, VTEncodeInfoFlags, CMSampleBufferRef sb)
                                                               {
                                                                   result = s;
                                                                   gotSample = sb != nullptr;
                                                                   dispatch_semaphore_signal (encoded);
                                                               });
        if (st != noErr)
        {
            VTCompressionSessionInvalidate (session);
            CFRelease (session);
            CVPixelBufferRelease (pb);
            return "H.264 encoder not available (" + juce::String ((int) st) + ")";
        }
        dispatch_async (dispatch_get_global_queue (QOS_CLASS_DEFAULT, 0), ^{
            VTCompressionSessionCompleteFrames (session, kCMTimeInvalid);
            dispatch_semaphore_signal (completed);
        });
        const auto wait5s = [] (dispatch_semaphore_t s) { return dispatch_semaphore_wait (s, dispatch_time (DISPATCH_TIME_NOW, 5 * (int64_t) NSEC_PER_SEC)) == 0; };
        if (! wait5s (encoded) || ! wait5s (completed))
            return "H.264 encoder not responding";   // 後始末はしない（上の説明）
        VTCompressionSessionInvalidate (session);
        CFRelease (session);
        CVPixelBufferRelease (pb);
       #if ! __has_feature(objc_arc)
        dispatch_release (encoded);
        dispatch_release (completed);
       #endif
        if (result != noErr || ! gotSample)
            return "H.264 encoder failed (" + juce::String ((int) result) + ")";
        return {};
    }
    juce::String fromNS (NSString* s) { return s != nil ? juce::String::fromUTF8 ([s UTF8String]) : juce::String(); }

    class AvEncoder final : public Encoder
    {
    public:
        ~AvEncoder() override { abandon(); }

        juce::String open (const juce::File& d, const Spec& s, std::shared_ptr<const juce::AudioBuffer<float>> a) override
        {
            dest = d;
            spec = s;
            audio = a != nullptr && a->getNumSamples() > 0 && a->getNumChannels() > 0 ? a : nullptr;
            if (auto why = Encoder::problem(); why.isNotEmpty())
                return why;
            dest.deleteFile();

            @autoreleasepool
            {
                NSError* error = nil;
                writer = [[AVAssetWriter alloc] initWithURL: [NSURL fileURLWithPath: toNS (dest.getFullPathName())]
                                                   fileType: AVFileTypeMPEG4
                                                      error: &error];
                if (writer == nil)
                    return "AVAssetWriter: " + fromNS (error.localizedDescription);

                NSDictionary* compression = @{ AVVideoAverageBitRateKey: @(spec.videoBitrate),
                                               AVVideoMaxKeyFrameIntervalKey: @(spec.fps * 2),
                                               AVVideoProfileLevelKey: AVVideoProfileLevelH264MainAutoLevel };
                NSDictionary* videoSettings = @{ AVVideoCodecKey: AVVideoCodecTypeH264,
                                                 AVVideoWidthKey: @(spec.width),
                                                 AVVideoHeightKey: @(spec.height),
                                                 AVVideoCompressionPropertiesKey: compression };
                videoIn = [[AVAssetWriterInput alloc] initWithMediaType: AVMediaTypeVideo outputSettings: videoSettings];
                videoIn.expectsMediaDataInRealTime = NO;
                NSDictionary* pixels = @{ (id) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
                                          (id) kCVPixelBufferWidthKey: @(spec.width),
                                          (id) kCVPixelBufferHeightKey: @(spec.height) };
                adaptor = [[AVAssetWriterInputPixelBufferAdaptor alloc] initWithAssetWriterInput: videoIn
                                                                     sourcePixelBufferAttributes: pixels];
                if (! [writer canAddInput: videoIn])
                    return fail ("can't add the H.264 track");
                [writer addInput: videoIn];

                if (audio != nullptr)
                {
                    AudioChannelLayout layout {};
                    layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
                    NSDictionary* audioSettings = @{ AVFormatIDKey: @(kAudioFormatMPEG4AAC),
                                                     AVSampleRateKey: @(audioRate),
                                                     AVNumberOfChannelsKey: @2,
                                                     AVEncoderBitRateKey: @(spec.audioBitrate),
                                                     AVChannelLayoutKey: [NSData dataWithBytes: &layout length: sizeof (layout)] };
                    audioIn = [[AVAssetWriterInput alloc] initWithMediaType: AVMediaTypeAudio outputSettings: audioSettings];
                    audioIn.expectsMediaDataInRealTime = NO;
                    if (! [writer canAddInput: audioIn])
                        return fail ("can't add the AAC track");
                    [writer addInput: audioIn];

                    AudioStreamBasicDescription asbd {};
                    asbd.mSampleRate = audioRate;
                    asbd.mFormatID = kAudioFormatLinearPCM;
                    asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
                    asbd.mBytesPerPacket = 8;
                    asbd.mFramesPerPacket = 1;
                    asbd.mBytesPerFrame = 8;
                    asbd.mChannelsPerFrame = 2;
                    asbd.mBitsPerChannel = 32;
                    if (CMAudioFormatDescriptionCreate (kCFAllocatorDefault, &asbd, sizeof (layout), &layout, 0, nullptr, nullptr, &audioFormat) != noErr)
                        return fail ("can't describe the audio");
                }

                if (! [writer startWriting])
                    return fail ("startWriting: " + fromNS (writer.error.localizedDescription));
                [writer startSessionAtSourceTime: kCMTimeZero];
            }
            return {};
        }

        juce::String addFrame (const juce::uint8* bgra, int lineStride) override
        {
            if (writer == nil)
                return "not open";
            // 映像を受け取れるまで待つ。書く側は映像と音を交互に並べるので、音が足りないと映像を受け取らない。
            // 待つ間は音を先に渡す（1 コマごとに同じ時刻までの音しか渡さないと、CI の Mac で止まったまま進まなかった）
            if (auto e = waitWhileFeedingAudio (videoIn); e.isNotEmpty())
                return fail ("video: " + e);
            @autoreleasepool
            {
                CVPixelBufferRef pb = nullptr;
                auto pool = adaptor.pixelBufferPool;
                const auto made = pool != nullptr ? CVPixelBufferPoolCreatePixelBuffer (kCFAllocatorDefault, pool, &pb)
                                                  : CVPixelBufferCreate (kCFAllocatorDefault, (size_t) spec.width, (size_t) spec.height,
                                                                         kCVPixelFormatType_32BGRA, nullptr, &pb);
                if (made != kCVReturnSuccess || pb == nullptr)
                    return fail ("can't make a pixel buffer");
                CVPixelBufferLockBaseAddress (pb, 0);
                auto* base = static_cast<juce::uint8*> (CVPixelBufferGetBaseAddress (pb));
                const auto rowBytes = CVPixelBufferGetBytesPerRow (pb);
                for (int y = 0; y < spec.height; ++y)
                    std::memcpy (base + rowBytes * (size_t) y, bgra + (size_t) lineStride * (size_t) y, (size_t) spec.width * 4);
                CVPixelBufferUnlockBaseAddress (pb, 0);
                const BOOL ok = [adaptor appendPixelBuffer: pb withPresentationTime: CMTimeMake (frames, spec.fps)];
                CVPixelBufferRelease (pb);
                if (! ok)
                    return fail ("video: " + writerError());
            }
            ++frames;
            // このコマの終わりまでの音を、受け取れる分だけ（残りは次のコマを待つ間か、最後に書く）
            const auto until = audioSamplesUntilFrame (frames - 1, spec.fps);
            for (;;)
            {
                juce::String e;
                if (audioPos >= until || ! appendAudioChunk (e))
                {
                    if (e.isNotEmpty())
                        return fail ("audio: " + e);
                    break;
                }
            }
            return {};
        }

        juce::String finish() override
        {
            if (writer == nil)
                return "not open";
            @autoreleasepool
            {
                // 映像を閉じてから残りの音を書く（映像を待たずに音を受け取るように）
                [videoIn markAsFinished];
                if (audioIn != nil)
                {
                    while (audioPos < audio->getNumSamples())
                        if (auto e = waitWhileFeedingAudio (audioIn); e.isNotEmpty())
                            return fail ("audio: " + e);
                        else if (juce::String err; ! appendAudioChunk (err) && err.isNotEmpty())
                            return fail ("audio: " + err);
                    [audioIn markAsFinished];
                }
                dispatch_semaphore_t done = dispatch_semaphore_create (0);
                [writer finishWritingWithCompletionHandler: ^{ dispatch_semaphore_signal (done); }];
                dispatch_semaphore_wait (done, DISPATCH_TIME_FOREVER);
               #if ! __has_feature(objc_arc)
                dispatch_release (done);
               #endif
                if (writer.status != AVAssetWriterStatusCompleted)
                    return fail ("finish: " + writerError());
            }
            release();
            if (! dest.existsAsFile() || dest.getSize() <= 0)
                return "no file written";
            return {};
        }

    private:
        /** input が受け取れるまで待つ。待つ間、音が残っていて受け取れるなら先に渡す。
            何も進まないまま 20 秒たったら、または書く側が失敗したら理由を返す */
        juce::String waitWhileFeedingAudio (AVAssetWriterInput* input)
        {
            auto lastProgress = juce::Time::getMillisecondCounter();
            for (;;)
            {
                if (input.readyForMoreMediaData)
                    return {};
                if (writer.status == AVAssetWriterStatusFailed)
                    return writerError();
                juce::String e;
                if (input != audioIn && appendAudioChunk (e))
                    lastProgress = juce::Time::getMillisecondCounter();
                else if (e.isNotEmpty())
                    return e;
                else if (juce::Time::getMillisecondCounter() - lastProgress > 20000)
                    return "not ready";
                else
                    juce::Thread::sleep (1);
            }
        }

        juce::String writerError() const
        {
            return writer != nil && writer.error != nil ? fromNS (writer.error.localizedDescription) : juce::String ("not ready");
        }

        /** 音を 0.1 秒書く（書いたら true）。音が残っていない・書く側が受け取れないときは書かずに false（error は空）。
            書けなかったときは error に理由 */
        bool appendAudioChunk (juce::String& error)
        {
            if (audio == nullptr || audioIn == nil || audioPos >= audio->getNumSamples() || ! audioIn.readyForMoreMediaData)
                return false;
            @autoreleasepool
            {
                const auto n = (int) juce::jmin<juce::int64> (audioChunk, audio->getNumSamples() - audioPos);
                interleaved.resize ((size_t) n * 2);
                for (int i = 0; i < n; ++i)
                    for (int c = 0; c < 2; ++c)
                        interleaved[(size_t) i * 2 + (size_t) c] = sampleAt (*audio, c, audioPos + i);
                const auto bytes = interleaved.size() * sizeof (float);
                CMBlockBufferRef block = nullptr;
                auto st = CMBlockBufferCreateWithMemoryBlock (kCFAllocatorDefault, nullptr, bytes, kCFAllocatorDefault, nullptr,
                                                              0, bytes, kCMBlockBufferAssureMemoryNowFlag, &block);
                if (st == noErr)
                    st = CMBlockBufferReplaceDataBytes (interleaved.data(), block, 0, bytes);
                CMSampleBufferRef sample = nullptr;
                if (st == noErr)
                    st = CMAudioSampleBufferCreateReadyWithPacketDescriptions (kCFAllocatorDefault, block, audioFormat, (CMItemCount) n,
                                                                               CMTimeMake (audioPos, audioRate), nullptr, &sample);
                if (block != nullptr)
                    CFRelease (block);
                if (st != noErr || sample == nullptr)
                {
                    error = "can't make an audio buffer";
                    return false;
                }
                const BOOL ok = [audioIn appendSampleBuffer: sample];
                CFRelease (sample);
                if (! ok)
                {
                    error = writerError();
                    return false;
                }
                audioPos += n;
            }
            return true;
        }

        juce::String fail (const juce::String& why)
        {
            abandon();
            return why;
        }

        void release()
        {
            if (audioFormat != nullptr) { CFRelease (audioFormat); audioFormat = nullptr; }
            [adaptor release];  adaptor = nil;
            [audioIn release];  audioIn = nil;
            [videoIn release];  videoIn = nil;
            [writer release];   writer = nil;
        }

        void abandon()
        {
            const bool hadWriter = writer != nil;
            if (hadWriter && writer.status == AVAssetWriterStatusWriting)
                [writer cancelWriting];
            release();
            if (hadWriter)
                dest.deleteFile();
        }

        juce::File dest;
        Spec spec;
        std::shared_ptr<const juce::AudioBuffer<float>> audio;
        AVAssetWriter* writer = nil;
        AVAssetWriterInput* videoIn = nil;
        AVAssetWriterInput* audioIn = nil;
        AVAssetWriterInputPixelBufferAdaptor* adaptor = nil;
        CMAudioFormatDescriptionRef audioFormat = nullptr;
        juce::int64 frames = 0, audioPos = 0;
        std::vector<float> interleaved;
    };
} // namespace

juce::String Encoder::problem()
{
    @autoreleasepool
    {
        static const juce::String reason = probeH264();
        return reason;
    }
}
std::unique_ptr<Encoder> Encoder::create() { return std::make_unique<AvEncoder>(); }

bool readFrame (const juce::File& file, double seconds, int& width, int& height, std::vector<juce::uint32>& argb)
{
    @autoreleasepool
    {
        AVURLAsset* asset = [AVURLAsset URLAssetWithURL: [NSURL fileURLWithPath: toNS (file.getFullPathName())] options: nil];
       #pragma clang diagnostic push
       #pragma clang diagnostic ignored "-Wdeprecated-declarations"
        NSArray* tracks = [asset tracksWithMediaType: AVMediaTypeVideo];
       #pragma clang diagnostic pop
        if (tracks.count == 0)
            return false;
        NSError* error = nil;
        AVAssetReader* reader = [[[AVAssetReader alloc] initWithAsset: asset error: &error] autorelease];
        if (reader == nil)
            return false;
        NSDictionary* settings = @{ (id) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        AVAssetReaderTrackOutput* output = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack: tracks[0] outputSettings: settings];
        if (! [reader canAddOutput: output])
            return false;
        [reader addOutput: output];
        reader.timeRange = CMTimeRangeMake (CMTimeMakeWithSeconds (juce::jmax (0.0, seconds), 600), kCMTimePositiveInfinity);
        if (! [reader startReading])
            return false;
        CMSampleBufferRef sample = [output copyNextSampleBuffer];
        if (sample == nullptr)
            return false;
        bool ok = false;
        if (CVImageBufferRef image = CMSampleBufferGetImageBuffer (sample))
        {
            CVPixelBufferLockBaseAddress (image, kCVPixelBufferLock_ReadOnly);
            width = (int) CVPixelBufferGetWidth (image);
            height = (int) CVPixelBufferGetHeight (image);
            const auto rowBytes = CVPixelBufferGetBytesPerRow (image);
            auto* base = static_cast<const juce::uint8*> (CVPixelBufferGetBaseAddress (image));
            argb.assign ((size_t) width * (size_t) height, 0);
            for (int y = 0; y < height; ++y)
            {
                auto* row = reinterpret_cast<const juce::uint32*> (base + rowBytes * (size_t) y);
                for (int x = 0; x < width; ++x)
                    argb[(size_t) y * (size_t) width + (size_t) x] = row[x] | 0xff000000u;   // B G R A の並び = 0xAARRGGBB
            }
            CVPixelBufferUnlockBaseAddress (image, kCVPixelBufferLock_ReadOnly);
            ok = width > 0 && height > 0;
        }
        CFRelease (sample);
        [reader cancelReading];
        return ok;
    }
}

#else
//==============================================================================
// Linux（開発用）：PATH の ffmpeg に生のコマを渡す（音は一時的な WAV）。libx264 の入った ffmpeg が要る
namespace
{
    juce::File findOnPath (const juce::String& name)
    {
        for (auto& dir : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("PATH", {}), ":", {}))
        {
            if (dir.isEmpty())
                continue;
            auto f = juce::File (dir).getChildFile (name);
            if (f.existsAsFile() && ::access (f.getFullPathName().toRawUTF8(), X_OK) == 0)
                return f;
        }
        return {};
    }

    juce::String quote (const juce::String& s) { return "'" + s.replace ("'", "'\\''") + "'"; }

    int exitCode (int status) { return status != -1 && WIFEXITED (status) ? WEXITSTATUS (status) : -1; }

    class FfmpegEncoder final : public Encoder
    {
    public:
        ~FfmpegEncoder() override { abandon(); }

        juce::String open (const juce::File& d, const Spec& s, std::shared_ptr<const juce::AudioBuffer<float>> a) override
        {
            dest = d;
            spec = s;
            const auto ffmpeg = findOnPath ("ffmpeg");
            if (ffmpeg == juce::File())
                return "ffmpeg not found";
            dest.deleteFile();
            log = juce::File::createTempFile (".log");

            juce::String audioArgs;
            if (a != nullptr && a->getNumSamples() > 0 && a->getNumChannels() > 0)
            {
                wav = juce::File::createTempFile (".wav");
                std::unique_ptr<juce::OutputStream> stream (wav.createOutputStream().release());
                if (stream == nullptr)
                    return "can't write the audio";
                juce::WavAudioFormat format;
                auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (audioRate)
                                                                                              .withNumChannels (2)
                                                                                              .withBitsPerSample (24));
                if (writer == nullptr)
                    return "can't write the audio";
                juce::AudioBuffer<float> stereo (2, a->getNumSamples());
                for (int c = 0; c < 2; ++c)
                    stereo.copyFrom (c, 0, *a, juce::jmin (c, a->getNumChannels() - 1), 0, a->getNumSamples());
                if (! writer->writeFromAudioSampleBuffer (stereo, 0, stereo.getNumSamples()))
                    return "can't write the audio";
                audioArgs = " -i " + quote (wav.getFullPathName()) + " -map 0:v -map 1:a -c:a aac -b:a " + juce::String (spec.audioBitrate);
            }

            const auto cmd = quote (ffmpeg.getFullPathName()) + " -hide_banner -loglevel error -y"
                           + " -f rawvideo -pix_fmt bgra -s " + juce::String (spec.width) + "x" + juce::String (spec.height)
                           + " -r " + juce::String (spec.fps) + " -i -" + audioArgs
                           + " -c:v libx264 -preset veryfast -profile:v main -pix_fmt yuv420p -b:v " + juce::String (spec.videoBitrate)
                           + " -movflags +faststart " + quote (dest.getFullPathName())
                           + " 2> " + quote (log.getFullPathName());
            std::signal (SIGPIPE, SIG_IGN);   // ffmpeg が先に終わっても書き込みで落ちない（失敗として返す）
            pipe = ::popen (cmd.toRawUTF8(), "w");
            if (pipe == nullptr)
                return "can't start ffmpeg";
            return {};
        }

        juce::String addFrame (const juce::uint8* bgra, int lineStride) override
        {
            if (pipe == nullptr)
                return "not open";
            const auto rowBytes = (size_t) spec.width * 4;
            for (int y = 0; y < spec.height; ++y)
                if (std::fwrite (bgra + (size_t) lineStride * (size_t) y, 1, rowBytes, pipe) != rowBytes)
                    return fail ("ffmpeg stopped");
            return {};
        }

        juce::String finish() override
        {
            if (pipe == nullptr)
                return "not open";
            const auto code = exitCode (::pclose (pipe));
            pipe = nullptr;
            if (code != 0)
                return fail ("ffmpeg failed (" + juce::String (code) + "): " + log.loadFileAsString().trim().substring (0, 300));
            cleanup();
            if (! dest.existsAsFile() || dest.getSize() <= 0)
                return "no file written";
            return {};
        }

    private:
        juce::String fail (const juce::String& why)
        {
            auto detail = why;
            abandon();
            return detail;
        }

        void cleanup()
        {
            if (wav != juce::File()) wav.deleteFile();
            if (log != juce::File()) log.deleteFile();
            wav = log = juce::File();
        }

        void abandon()
        {
            if (pipe != nullptr)
            {
                ::pclose (pipe);
                pipe = nullptr;
                dest.deleteFile();
            }
            cleanup();
        }

        juce::File dest, wav, log;
        Spec spec;
        FILE* pipe = nullptr;
    };

    juce::String run (const juce::String& cmd, std::vector<juce::uint8>* bytes)
    {
        FILE* p = ::popen (cmd.toRawUTF8(), "r");
        if (p == nullptr)
            return {};
        juce::MemoryOutputStream out;
        char buf[65536];
        for (size_t n; (n = std::fread (buf, 1, sizeof (buf), p)) > 0;)
            out.write (buf, n);
        ::pclose (p);
        if (bytes != nullptr)
        {
            bytes->assign (static_cast<const juce::uint8*> (out.getData()), static_cast<const juce::uint8*> (out.getData()) + out.getDataSize());
            return {};
        }
        return out.toString();
    }
} // namespace

juce::String Encoder::problem() { return findOnPath ("ffmpeg") != juce::File() ? juce::String() : juce::String ("ffmpeg not found"); }
std::unique_ptr<Encoder> Encoder::create() { return std::make_unique<FfmpegEncoder>(); }

bool readFrame (const juce::File& file, double seconds, int& width, int& height, std::vector<juce::uint32>& argb)
{
    const auto ffmpeg = findOnPath ("ffmpeg"), ffprobe = findOnPath ("ffprobe");
    if (ffmpeg == juce::File() || ffprobe == juce::File())
        return false;
    const auto size = juce::StringArray::fromTokens (run (quote (ffprobe.getFullPathName()) + " -v error -select_streams v:0"
                                                          " -show_entries stream=width,height -of csv=p=0 " + quote (file.getFullPathName()), nullptr)
                                                         .trim(), ",", {});
    if (size.size() != 2)
        return false;
    width = size[0].getIntValue();
    height = size[1].getIntValue();
    if (width <= 0 || height <= 0)
        return false;
    std::vector<juce::uint8> bytes;
    run (quote (ffmpeg.getFullPathName()) + " -v error -ss " + juce::String (juce::jmax (0.0, seconds), 3) + " -i " + quote (file.getFullPathName())
         + " -frames:v 1 -f rawvideo -pix_fmt bgra -", &bytes);
    if (bytes.size() != (size_t) width * (size_t) height * 4)
        return false;
    argb.resize ((size_t) width * (size_t) height);
    for (size_t i = 0; i < argb.size(); ++i)
        argb[i] = (juce::uint32) bytes[i * 4] | ((juce::uint32) bytes[i * 4 + 1] << 8) | ((juce::uint32) bytes[i * 4 + 2] << 16) | 0xff000000u;
    return true;
}
#endif
} // namespace vb::video
