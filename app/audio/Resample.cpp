#include "Resample.h"

namespace vb::audio
{
juce::int64 resampledLength (juce::int64 length, double fromRate, double toRate) noexcept
{
    if (fromRate <= 0.0 || toRate <= 0.0)
        return length;
    return (juce::int64) std::llround ((double) length * toRate / fromRate);
}

const juce::Array<double>& recordingRates()
{
    static const juce::Array<double> rates { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 352800.0, 384000.0 };
    return rates;
}

namespace
{
    /** JUCE の窓付き sinc は全体の大きさがわずかに小さい（実測 0.99 倍、周波数によらない）。
        一度だけ直流を通して測り、その逆数を掛けて 1 倍に戻す（JUCE のバージョンが変わっても追従する） */
    float sincGainCorrection()
    {
        static const float correction = []
        {
            std::vector<float> ones (4096, 1.0f), out (1024);
            juce::WindowedSincInterpolator interp;
            interp.process (0.5, ones.data(), out.data(), (int) out.size(), (int) ones.size(), 0);
            const auto g = out.back();
            return g > 0.5f && g < 1.5f ? 1.0f / g : 1.0f;
        }();
        return correction;
    }

    /** 8 次 Butterworth の低域通過を前後に掛ける（位相 0。下げる時の折り返し防止） */
    void lowPassZeroPhase (float* x, int n, double rate, double cutoff)
    {
        // 8 次 Butterworth の 2 次の段ごとの Q
        static constexpr double qs[] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        for (int pass = 0; pass < 2; ++pass)
        {
            for (auto q : qs)
            {
                juce::IIRFilter f;
                f.setCoefficients (juce::IIRCoefficients::makeLowPass (rate, cutoff, q));
                f.processSamples (x, n);
            }
            std::reverse (x, x + n);   // 2 回目は逆向き（2 回で元の向き）
        }
    }
}

std::shared_ptr<SongAudio> resampleSong (const SongAudio& song, double toRate, const std::function<bool (float)>& progress)
{
    auto out = std::make_shared<SongAudio>();
    const auto fromRate = song.sampleRate;
    const auto channels = song.buffer.getNumChannels();
    const auto inLen = song.buffer.getNumSamples();

    if (fromRate <= 0.0 || toRate <= 0.0 || std::abs (fromRate - toRate) < 0.5)
    {
        out->buffer.makeCopyOf (song.buffer);
        out->sampleRate = song.sampleRate;
        return out;
    }

    const auto outLen = (int) resampledLength (inLen, fromRate, toRate);
    const auto ratio = fromRate / toRate;                                   // 出力 1 サンプルあたりに進む入力サンプル
    const auto latency = (int) juce::WindowedSincInterpolator::getBaseLatency();   // 補間の遅れ（入力サンプル）
    try
    {
        out->buffer.setSize (channels, outLen);
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
    out->sampleRate = toRate;

    const auto gain = sincGainCorrection();

    // 入力は「頭に遅れの分」をずらして与え、後ろは補間の窓が読みきれるだけ無音を足す
    std::vector<float> in ((size_t) inLen + (size_t) latency * 2 + 64, 0.0f);
    constexpr int block = 1 << 15;

    for (int c = 0; c < channels; ++c)
    {
        std::copy (song.buffer.getReadPointer (c), song.buffer.getReadPointer (c) + inLen, in.begin());
        std::fill (in.begin() + inLen, in.end(), 0.0f);
        if (toRate < fromRate)
            lowPassZeroPhase (in.data(), inLen, fromRate, 0.45 * toRate);

        juce::WindowedSincInterpolator interp;
        interp.reset();
        const float* src = in.data() + latency;
        auto available = (int) in.size() - latency;
        auto* dst = out->buffer.getWritePointer (c);

        for (int done = 0; done < outLen;)
        {
            const auto n = juce::jmin (block, outLen - done);
            const auto used = interp.process (ratio, src, dst + done, n, available, 0);
            juce::FloatVectorOperations::multiply (dst + done, gain, n);
            src += used;
            available -= used;
            done += n;

            if (progress && ! progress (((float) c + (float) done / (float) outLen) / (float) channels))
                return nullptr;
        }
    }
    return out;
}
} // namespace vb::audio
