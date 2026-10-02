#include "TakeRecorder.h"

namespace vb::audio
{
TakeRecorder::TakeRecorder() = default;

TakeRecorder::~TakeRecorder()
{
    finish();
    writerThread.stopThread (2000);
}

juce::String TakeRecorder::begin (const juce::File& f, double sampleRate, bool floatSamples, juce::int64 tailSamples)
{
    finish();

    if (sampleRate <= 0.0)
        return "no sample rate";
    if (f.exists())
        return "file exists";   // 録った声は上書きしない（消さない）
    if (auto r = f.getParentDirectory().createDirectory(); r.failed())
        return r.getErrorMessage();

    auto fileStream = std::make_unique<juce::FileOutputStream> (f);
    if (! fileStream->openedOk())
        return fileStream->getStatus().getErrorMessage();

    // 24bit PCM か 32bit float・モノラル・デバイス（＝時間軸）の SR（DESIGN 6.5）
    std::unique_ptr<juce::OutputStream> stream (fileStream.release());
    juce::WavAudioFormat wav;
    using Format = juce::AudioFormatWriterOptions::SampleFormat;
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                                         .withNumChannels (1)
                                                                         .withBitsPerSample (floatSamples ? 32 : 24)
                                                                         .withSampleFormat (floatSamples ? Format::floatingPoint
                                                                                                         : Format::integral));
    if (w == nullptr)
        return "can't create WAV writer";

    if (! writerThread.isThreadRunning())
        writerThread.startThread();

    // 裏で書く。FIFO は 10 秒分（ディスクが一瞬詰まっても落ちない）
    auto threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w.release(), writerThread,
                                                                               (int) (sampleRate * 10.0));
    started = false;
    ended = false;
    dropped = false;
    startSample = 0;
    recorded = 0;
    peak = 0.0f;
    tail = juce::jmax ((juce::int64) 0, tailSamples);
    tailLeft = -1;
    file = f;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        writer = std::move (threaded);
    }
    active = true;
    return {};
}

TakeRecorder::Result TakeRecorder::finish()
{
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> w;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        w = std::move (writer);
        active = false;
    }
    if (w == nullptr)
        return {};

    w.reset();   // 残りを書ききって閉じる

    Result r;
    r.file = file;
    r.startSample = startSample.load();
    r.length = started.load() ? recorded.load() : 0;
    r.peak = peak.load();
    r.clipped = r.peak >= clipLevel;
    r.dropped = dropped.load();
    started = false;
    ended = false;
    return r;
}

void TakeRecorder::process (const float* input, int numSamples, juce::int64 songStart, int songPlayed, bool wrapped) noexcept
{
    if (! active.load() || ended.load())
        return;

    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked() || writer == nullptr)
        return;

    // 曲が鳴り始めたブロックから録る。録音中はループしない（戻ったら曲の終わりと同じに扱う）
    if (! started.load())
    {
        if (songPlayed <= 0 || wrapped)
            return;
        startSample = songStart;
        started = true;
    }

    // 曲の中の分。曲が止まった・終わったら、残りは後ろの分（遅れて届く歌の終わり）
    int offset = 0;
    if (tailLeft < 0)
    {
        const auto inSong = (songPlayed <= 0 || wrapped) ? 0 : juce::jmin (numSamples, songPlayed);
        write (input, 0, inSong);
        offset = inSong;
        if (inSong == numSamples)
            return;
        tailLeft = tail;
    }

    const auto n = (int) juce::jmin<juce::int64> (numSamples - offset, tailLeft);
    write (input, offset, n);
    tailLeft -= n;
    if (tailLeft <= 0)
        ended = true;
}

void TakeRecorder::write (const float* input, int offset, int n) noexcept
{
    if (n <= 0)
        return;
    bool ok = true;
    if (input != nullptr)
    {
        const float* p = input + offset;
        ok = writer->write (&p, n);
        auto pk = peak.load();
        for (int i = 0; i < n; ++i)
            pk = juce::jmax (pk, std::abs (p[i]));
        peak = pk;
    }
    else
    {
        // 入力が消えた（機器が外れた等）：位置がずれないよう無音で埋める
        static const float zeros[512] = {};
        const float* z = zeros;
        for (int done = 0; done < n && ok; done += 512)
            ok = writer->write (&z, juce::jmin (512, n - done));
    }

    if (! ok)
        dropped = true;   // 裏の書き込みが追いつかない（このテイクは壊れている）
    recorded += n;
}
} // namespace vb::audio
