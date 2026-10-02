#include "PitchTracker.h"

namespace vb::audio
{
namespace pitch
{
Estimate yin (const float* x, int window, int minLag, int maxLag, std::vector<float>& d)
{
    // 差分関数 d(τ) = Σ (x[j] - x[j+τ])²、累積平均で正規化した d'(τ)
    d.assign ((size_t) maxLag + 1, 1.0f);
    double running = 0.0;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double sum = 0.0;
        for (int j = 0; j < window; ++j)
        {
            const double diff = (double) x[j] - (double) x[j + tau];
            sum += diff * diff;
        }
        running += sum;
        d[(size_t) tau] = running > 0.0 ? (float) (sum * tau / running) : 1.0f;
    }

    // しきい値を下回った最初の谷。無ければ一番低い所（信頼度は低い）
    int best = -1;
    for (int tau = minLag; tau <= maxLag; ++tau)
        if (d[(size_t) tau] < threshold)
        {
            while (tau + 1 <= maxLag && d[(size_t) tau + 1] < d[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        best = minLag;
        for (int tau = minLag + 1; tau <= maxLag; ++tau)
            if (d[(size_t) tau] < d[(size_t) best])
                best = tau;
    }

    // 谷を放物線で補間（サンプルの間の周期）
    double lag = best;
    if (best > minLag && best < maxLag)
    {
        const double a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
        const double denom = a - 2.0 * b + c;
        if (std::abs (denom) > 1.0e-12)
            lag += juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denom);
    }
    return { lag, juce::jlimit (0.0f, 1.0f, 1.0f - d[(size_t) best]) };
}
} // namespace pitch

//==============================================================================
PitchTracker::PitchTracker() : juce::Thread ("VoiceBooth pitch") {}

PitchTracker::~PitchTracker()
{
    release();
}

void PitchAnalyzer::prepare (double sampleRate)
{
    rate = sampleRate;
    if (rate <= 0.0)
        return;

    decim = pitch::decimation (rate);
    const auto fs = rate / decim;
    hop = juce::jmax (1, juce::roundToInt (fs * pitch::hopSeconds));
    window = juce::roundToInt (fs * pitch::windowSeconds);
    minLag = juce::jmax (2, (int) std::floor (fs / pitch::maxHz));
    maxLag = (int) std::ceil (fs / pitch::minHz);

    // 間引きの前の低域通過（窓付き sinc、遮断 0.45 × 間引いた後の SR）
    if (decim == 1)
        taps = { 1.0f };
    else
    {
        const int half = 8 * decim;
        taps.assign ((size_t) (2 * half + 1), 0.0f);
        const auto cutoff = 0.45 / decim;   // 入力の SR に対する比
        double sum = 0.0;
        for (int i = -half; i <= half; ++i)
        {
            const double sinc = i == 0 ? 2.0 * cutoff : std::sin (juce::MathConstants<double>::twoPi * cutoff * i) / (juce::MathConstants<double>::pi * i);
            const double w = 0.42 + 0.5 * std::cos (juce::MathConstants<double>::pi * i / half) + 0.08 * std::cos (juce::MathConstants<double>::twoPi * i / half);
            taps[(size_t) (i + half)] = (float) (sinc * w);
            sum += sinc * w;
        }
        for (auto& t : taps)
            t = (float) (t / sum);
    }
    history.assign (taps.size(), 0.0f);
    historyPos = phase = 0;
    buf.assign ((size_t) (window + maxLag + 1), 0.0f);
    bufPos.assign (buf.size(), -1);
    frameIn.assign (buf.size(), 0.0f);
    frameInPos.assign (buf.size(), -1);
    sinceHop = filled = 0;
    recentCount = 0;
    for (auto& r : recent)
        r = {};
}

void PitchTracker::prepare (double rate)
{
    release();
    if (rate <= 0.0)
        return;

    analyzer.prepare (rate);
    const auto size = (int) (rate * 2.0);   // 2 秒分（検出が少し遅れても落とさない）
    {
        const juce::SpinLock::ScopedLockType sl (pushLock);
        ring.assign ((size_t) size, 0.0f);
        ringPos.assign ((size_t) size, -1);
        fifo.setTotalSize (size);
        fifo.reset();
    }
    {
        const juce::ScopedLock sl (outLock);
        output.clear();
    }
    sampleRate = rate;
    ready = true;
    startThread();
}

void PitchTracker::release()
{
    {
        const juce::SpinLock::ScopedLockType sl (pushLock);
        ready = false;
    }
    stopThread (1000);
}

void PitchTracker::push (const float* input, int numSamples, int64 songStart, int songPlayed, double songStep) noexcept
{
    if (! ready.load() || input == nullptr || numSamples <= 0)
        return;
    const juce::SpinLock::ScopedTryLockType sl (pushLock);
    if (! sl.isLocked() || ! ready.load())
        return;

    if (songPlayed <= 0 && freeRun.load (std::memory_order_relaxed))
    {
        // 曲が止まっている：声域を測る間だけ、曲の外の位置を振って音程を取る
        songStart = freeCounter;
        songPlayed = numSamples;
        songStep = 1.0;
        freeCounter += numSamples;
    }

    int s1, n1, s2, n2;
    fifo.prepareToWrite (numSamples, s1, n1, s2, n2);
    if (n1 + n2 < numSamples)
        overflow.fetch_add (1);   // 検出が追いつかない：入らない分は捨てる（線が途切れるだけ）
    auto put = [&] (int start, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto k = offset + i;
            ring[(size_t) (start + i)] = input[k];
            ringPos[(size_t) (start + i)] = k >= songPlayed ? -1
                                          : juce::exactlyEqual (songStep, 1.0) ? songStart + k
                                                            : songStart + (int64) std::llround (k * songStep);
        }
    };
    put (s1, n1, 0);
    put (s2, n2, n1);
    fifo.finishedWrite (n1 + n2);
}

void PitchTracker::run()
{
    while (! threadShouldExit())
    {
        if (fifo.getNumReady() == 0)
        {
            wait (5);
            continue;
        }
        int s1, n1, s2, n2;
        fifo.prepareToRead (juce::jmin (fifo.getNumReady(), 8192), s1, n1, s2, n2);
        found.clear();
        analyzer.process (ring.data() + s1, ringPos.data() + s1, n1, found);
        analyzer.process (ring.data() + s2, ringPos.data() + s2, n2, found);
        fifo.finishedRead (n1 + n2);
        if (! found.empty())
        {
            const juce::ScopedLock sl (outLock);
            output.insert (output.end(), found.begin(), found.end());
        }
    }
}

void PitchTracker::pop (std::vector<PitchFrame>& out)
{
    const juce::ScopedLock sl (outLock);
    out.insert (out.end(), output.begin(), output.end());
    output.clear();
}

void PitchAnalyzer::process (const float* in, const int64* pos, int n, std::vector<PitchFrame>& found)
{
    if (rate <= 0.0)
        return;
    const auto length = (int) taps.size();
    const auto filterDelay = (length - 1) / 2;
    const auto size = (int) buf.size();

    for (int i = 0; i < n; ++i)
    {
        history[(size_t) historyPos] = in[i];
        historyPos = (historyPos + 1) % length;
        if (++phase < decim)
            continue;
        phase = 0;

        // 間引いた 1 サンプル（係数は左右対称なので履歴の向きは問わない）
        float y = 0.0f;
        for (int k = 0; k < length; ++k)
            y += taps[(size_t) k] * history[(size_t) ((historyPos + k) % length)];

        buf[(size_t) (filled % size)] = y;
        bufPos[(size_t) (filled % size)] = pos[i] >= 0 ? pos[i] - filterDelay : -1;
        ++filled;
        if (++sinceHop < hop || filled < size)
            continue;
        sinceHop = 0;

        // 古い順に並べて 1 窓
        for (int k = 0; k < size; ++k)
        {
            const auto idx = (size_t) ((filled + k) % size);
            frameIn[(size_t) k] = buf[idx];
            frameInPos[(size_t) k] = bufPos[idx];
        }

        // 位置は YIN が見ている範囲（窓 + 周期）の真ん中。声が無ければ窓 + 最長の周期の真ん中
        PitchFrame f;
        auto centre = (window + maxLag) / 2;
        float peak = 0.0f;
        for (int k = 0; k < window + maxLag; ++k)
            peak = juce::jmax (peak, std::abs (frameIn[(size_t) k]));
        f.levelDb = peak > 0.0f ? 20.0f * std::log10 (peak) : -100.0f;
        if (f.levelDb >= pitch::gateDb)
        {
            const auto e = pitch::yin (frameIn.data(), window, minLag, maxLag, scratch);
            const auto fs = rate / decim;
            if (e.lag > 0.0)
            {
                f.midi = pitch::hzToMidi (fs / e.lag);
                f.confidence = e.confidence;
                centre = (window + juce::roundToInt (e.lag)) / 2;
            }
        }
        f.songSample = frameInPos[(size_t) centre];

        if (f.songSample < 0)
        {
            recentCount = 0;   // 曲が止まっている：線にしない
            for (auto& r : recent)
                r = {};
            continue;
        }

        // 前後 2 点ずつ（5 点）を見て出す（20 ms 遅れる）：
        //  - 3 半音以内でつながる声のかたまりが 2 点以下（20 ms 以下）で、両側を別の音（声あり）にはさまれていたら信用しない。
        //    音の切り替わりで窓に 2 つの音が混ざると、その公約数の低い周期が見えるため（1 点だけのオクターブの飛びも同じ）。
        //    歌い出し・歌い終わり（片側が無声）はそのまま
        //  - 前後とも声があれば 3 点の中央値でならす
        for (int k = 0; k < 4; ++k)
            recent[k] = recent[k + 1];
        recent[4] = f;
        recentCount = juce::jmin (5, recentCount + 1);
        if (recentCount < 3)
            continue;
        auto voicedAt = [this] (int k) { return recent[k].confidence >= 0.5f; };
        auto joined = [this] (int a, int b) { return std::abs (recent[a].midi - recent[b].midi) <= 3.0f; };
        auto out = recent[2];   // 出す点（始まりは前の点が無い：その所は声なしの扱い）
        if (voicedAt (2))
        {
            int lo = 2, hi = 2;
            while (lo > 0 && voicedAt (lo - 1) && joined (lo - 1, lo)) --lo;
            while (hi < 4 && voicedAt (hi + 1) && joined (hi, hi + 1)) ++hi;
            // 両側（間に無声の点をはさんでもよい）に声がある＝歌っている途中の飛び
            bool leftVoice = false, rightVoice = false;
            for (int k = 0; k < lo; ++k) leftVoice = leftVoice || voicedAt (k);
            for (int k = hi + 1; k < 5; ++k) rightVoice = rightVoice || voicedAt (k);
            const bool boxedIn = leftVoice && rightVoice;
            if (boxedIn && hi - lo + 1 <= 2)
                out.confidence = 0.3f;
            else if (voicedAt (1) && voicedAt (3))
                out.midi = juce::jmax (juce::jmin (recent[1].midi, out.midi), juce::jmin (juce::jmax (recent[1].midi, out.midi), recent[3].midi));   // 3 つの中央値
        }
        found.push_back (out);
    }
}
} // namespace vb::audio
