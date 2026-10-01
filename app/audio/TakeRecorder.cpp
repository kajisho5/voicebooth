#include "TakeRecorder.h"

namespace vb::audio
{
TakeRecorder::TakeRecorder() = default;

TakeRecorder::~TakeRecorder()
{
    finish();
    writerThread.stopThread (2000);
}

juce::String TakeRecorder::begin (const juce::File& f, double sampleRate)
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

    // 24bit PCM・モノラル・デバイス（＝曲）の SR（DESIGN 6.5）
    std::unique_ptr<juce::OutputStream> stream (fileStream.release());
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                                         .withNumChannels (1)
                                                                         .withBitsPerSample (24));
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

    // 曲が鳴り始めたブロックから録る。始まった後に止まった・ループで戻った（録音中はループしない）ら閉じる
    if (! started.load())
    {
        if (songPlayed <= 0 || wrapped)
            return;
        startSample = songStart;
        started = true;
    }
    else if (songPlayed <= 0 || wrapped)
    {
        ended = true;
        return;
    }

    const auto n = juce::jmin (numSamples, songPlayed);
    bool ok = true;
    if (input != nullptr)
    {
        ok = writer->write (&input, n);
        auto p = peak.load();
        for (int i = 0; i < n; ++i)
            p = juce::jmax (p, std::abs (input[i]));
        peak = p;
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

    if (n < numSamples)
        ended = true;     // 曲の終わり
}
} // namespace vb::audio
