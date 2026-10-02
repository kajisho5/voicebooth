#include "LatencyProbe.h"

namespace vb::audio::latency
{
namespace
{
    constexpr double chirpSeconds = 0.04, leadSeconds = 0.2, periodSeconds = 0.65, maxLatencySeconds = 0.6;
    constexpr double startHz = 500.0, endHz = 8000.0;
    constexpr float level = 0.25f;               // -12 dBFS（ヘッドホンをマイクに近づけて測るので大きくしない）
    constexpr float minSnrDb = 18.0f;            // 雑音だけの相関の山は 13 dB 前後。これより十分高い山だけ数える
    constexpr float silentPeak = 1.0e-4f;        // -80 dBFS 未満なら入力に何も来ていない
    constexpr int minAgreeing = 3;               // 5 回のうち 3 回以上がそろえば採る

    int nextPow2 (int64 n)
    {
        int p = 1;
        while (p < n) p <<= 1;
        return p;
    }
}

Plan planFor (double sampleRate)
{
    Plan p;
    p.sampleRate = sampleRate;
    p.chirpLength = juce::roundToInt (chirpSeconds * sampleRate);
    p.lead = juce::roundToInt (leadSeconds * sampleRate);
    p.period = juce::roundToInt (periodSeconds * sampleRate);
    p.maxLatency = juce::roundToInt (maxLatencySeconds * sampleRate);
    p.totalLength = p.burstStart (p.bursts - 1) + p.maxLatency + p.chirpLength;
    return p;
}

std::vector<float> makeChirp (double sampleRate)
{
    const auto n = juce::roundToInt (chirpSeconds * sampleRate);
    const auto f1 = juce::jmin (endHz, sampleRate * 0.45);
    const auto k = std::log (f1 / startHz);
    const auto fade = juce::jmax (1, juce::roundToInt (0.002 * sampleRate));
    std::vector<float> out ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        // 指数スイープ：f(t) = f0 (f1/f0)^(t/T)
        const auto t = (double) i / sampleRate;
        const auto phase = juce::MathConstants<double>::twoPi * startHz * chirpSeconds / k * (std::exp (k * t / chirpSeconds) - 1.0);
        double w = 1.0;
        if (i < fade)          w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * i / fade);
        else if (i >= n - fade) w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * (n - 1 - i) / fade);
        out[(size_t) i] = (float) (std::sin (phase) * w) * level;
    }
    return out;
}

std::vector<float> makeSignal (const Plan& p)
{
    std::vector<float> out ((size_t) p.totalLength, 0.0f);
    const auto chirp = makeChirp (p.sampleRate);
    for (int b = 0; b < p.bursts; ++b)
        std::copy (chirp.begin(), chirp.end(), out.begin() + (std::ptrdiff_t) p.burstStart (b));
    return out;
}

void fft (std::vector<double>& re, std::vector<double>& im, bool inverse)
{
    const auto n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        auto bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap (re[i], re[j]);
            std::swap (im[i], im[j]);
        }
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const auto ang = juce::MathConstants<double>::twoPi / (double) len * (inverse ? 1.0 : -1.0);
        const auto wr = std::cos (ang), wi = std::sin (ang);
        for (size_t i = 0; i < n; i += len)
        {
            double cr = 1.0, ci = 0.0;
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto a = i + j, b = i + j + len / 2;
                const auto xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr;        im[a] += xi;
                const auto nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

Result analyse (const float* captured, int64 length, const Plan& p)
{
    Result r;
    r.bursts = p.bursts;
    if (captured == nullptr || length <= 0 || p.sampleRate <= 0.0)
        return r;

    for (int64 i = 0; i < length; ++i)
        r.inputPeak = juce::jmax (r.inputPeak, std::abs (captured[i]));
    r.clipped = r.inputPeak >= 0.99f;
    if (r.inputPeak < silentPeak)
    {
        r.status = Result::Status::silent;
        return r;
    }

    // チャープのスペクトル（共役を掛けて相関にする）
    const auto chirp = makeChirp (p.sampleRate);
    const auto segLength = (int64) p.maxLatency + p.chirpLength;
    const auto n = nextPow2 (segLength + p.chirpLength);
    std::vector<double> cr ((size_t) n, 0.0), ci ((size_t) n, 0.0);
    for (size_t i = 0; i < chirp.size(); ++i)
        cr[i] = chirp[i];
    fft (cr, ci, false);

    std::vector<int64> lags;
    std::vector<float> snrs;
    std::vector<double> xr ((size_t) n), xi ((size_t) n);
    for (int b = 0; b < p.bursts; ++b)
    {
        // この回の音が返ってくる範囲 [出した位置, 出した位置 + 最大の遅れ]
        const auto from = p.burstStart (b);
        std::fill (xr.begin(), xr.end(), 0.0);
        std::fill (xi.begin(), xi.end(), 0.0);
        for (int64 i = 0; i < segLength && from + i < length; ++i)
            xr[(size_t) i] = captured[from + i];
        fft (xr, xi, false);
        for (int i = 0; i < n; ++i)
        {
            const auto a = xr[(size_t) i], c = xi[(size_t) i];
            xr[(size_t) i] = a * cr[(size_t) i] + c * ci[(size_t) i];   // X · conj(C)
            xi[(size_t) i] = c * cr[(size_t) i] - a * ci[(size_t) i];
        }
        fft (xr, xi, true);   // xr[lag] = 相関（n 倍。比しか見ないのでそのまま）

        double best = 0.0;
        int bestLag = 0;
        for (int lag = 0; lag <= p.maxLatency; ++lag)
            if (std::abs (xr[(size_t) lag]) > best)
            {
                best = std::abs (xr[(size_t) lag]);
                bestLag = lag;
            }
        if (best <= 0.0)
            continue;

        // 山以外（山の前後チャープ 1 個分を除く）の RMS と比べる
        auto rmsAround = [&] (int peak)
        {
            double sum = 0.0;
            int count = 0;
            for (int lag = 0; lag <= p.maxLatency; ++lag)
                if (std::abs (lag - peak) > p.chirpLength)
                {
                    sum += xr[(size_t) lag] * xr[(size_t) lag];
                    ++count;
                }
            return count > 0 ? std::sqrt (sum / count) : 0.0;
        };
        const auto rms = rmsAround (bestLag);
        const auto snr = rms > 0.0 ? (float) (20.0 * std::log10 (best / rms)) : 100.0f;

        // 部屋の反響の方が強くても、直接届いた音（それより前の、十分に強い山）を採る。
        // 山のすぐ脇（0.5 ms 以内）は相関の裾なので見ない。雑音の山（RMS の 8 倍未満）は採らない
        const auto guard = juce::jmax (2, juce::roundToInt (0.0005 * p.sampleRate));
        const auto threshold = juce::jmax (0.35 * best, 8.0 * rms);
        for (int lag = juce::jmax (1, bestLag - p.chirpLength); lag < bestLag - guard; ++lag)
        {
            const auto v = std::abs (xr[(size_t) lag]);
            if (v >= threshold && v >= std::abs (xr[(size_t) lag - 1]) && v >= std::abs (xr[(size_t) lag + 1]))
            {
                bestLag = lag;
                break;
            }
        }

        snrs.push_back (snr);
        if (snr >= minSnrDb)
            lags.push_back (bestLag);
    }

    if (! snrs.empty())
    {
        auto sorted = snrs;
        std::sort (sorted.begin(), sorted.end());
        r.snrDb = sorted[sorted.size() / 2];
    }
    if ((int) lags.size() < minAgreeing)
    {
        r.status = Result::Status::weak;
        return r;
    }

    // 中央値の近く（0.25 ms 以内）にそろった回だけ使う
    std::sort (lags.begin(), lags.end());
    const auto median = lags[(lags.size() - 1) / 2];
    const auto tolerance = juce::jmax ((int64) 2, (int64) std::llround (0.00025 * p.sampleRate));
    std::vector<int64> agree;
    for (auto l : lags)
        if (std::abs (l - median) <= tolerance)
            agree.push_back (l);
    r.agreeing = (int) agree.size();
    if (r.agreeing < minAgreeing)
    {
        r.status = Result::Status::unstable;
        return r;
    }
    r.samples = agree[(agree.size() - 1) / 2];
    r.spreadMs = (double) (agree.back() - agree.front()) * 1000.0 / p.sampleRate;
    r.status = Result::Status::ok;
    return r;
}

//==============================================================================
void Probe::start (double sampleRate)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    running.store (false);
    currentPlan = planFor (sampleRate);
    signal = makeSignal (currentPlan);
    capture.assign ((size_t) currentPlan.totalLength, 0.0f);
    position = 0;
    finished.store (false);
    running.store (true);
}

bool Probe::process (const float* input, float* const* outputs, int numOutputs, int numSamples) noexcept
{
    if (! running.load())
        return false;
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked() || ! running.load())
        return false;

    const auto total = currentPlan.totalLength;
    const auto n = (int) juce::jmin<int64> (numSamples, total - position);
    for (int c = 0; c < numOutputs; ++c)
        if (auto* o = outputs[c])
        {
            std::copy (signal.begin() + (std::ptrdiff_t) position, signal.begin() + (std::ptrdiff_t) (position + n), o);
            std::fill (o + n, o + numSamples, 0.0f);
        }
    if (input != nullptr)
        std::copy (input, input + n, capture.begin() + (std::ptrdiff_t) position);   // 入力が無いブロックは 0 のまま
    position += n;

    if (position >= total)
    {
        running.store (false);
        finished.store (true);
    }
    return true;
}
} // namespace vb::audio::latency
