#include "MusicInfo.h"
#include "Align.h"
#include "Fft.h"

namespace vb::analysis
{
namespace
{
    using int64 = juce::int64;

    constexpr double envHopSeconds = 0.01;
    constexpr double minBpm = 50.0, maxBpm = 220.0, preferredBpm = 120.0;

    /** 包絡の値を小数の位置で読む（直線補間） */
    float at (const std::vector<float>& e, double pos)
    {
        if (pos < 0.0)
            return 0.0f;
        const auto i = (size_t) pos;
        if (i + 1 >= e.size())
            return i < e.size() ? e[i] : 0.0f;
        const auto f = (float) (pos - (double) i);
        return e[i] * (1.0f - f) + e[i + 1] * f;
    }

    /** minHz〜maxHz の帯だけの立ち上がりの包絡（キック・ベース / スネアの胴）。拍の上か裏かを決めるのに使う */
    std::vector<float> bandOnsetEnvelope (const float* x, int64 length, int frameSize, int hop, double sampleRate, double minHz, double maxHz)
    {
        std::vector<float> env;
        if (length < frameSize)
            return env;
        const auto frames = (size_t) ((length - frameSize) / hop + 1);
        const auto minBin = juce::jlimit (1, frameSize / 2 - 1, (int) (minHz * frameSize / sampleRate));
        const auto maxBin = juce::jlimit (minBin + 1, frameSize / 2, (int) (maxHz * frameSize / sampleRate));
        env.resize (frames, 0.0f);
        std::vector<double> window ((size_t) frameSize), prev ((size_t) maxBin, 0.0);
        for (int i = 0; i < frameSize; ++i)
            window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / (frameSize - 1));
        std::vector<Complex> buf ((size_t) frameSize);
        for (size_t f = 0; f < frames; ++f)
        {
            const auto* p = x + (int64) f * hop;
            for (int i = 0; i < frameSize; ++i)
                buf[(size_t) i] = p[i] * window[(size_t) i];
            fft (buf, false);
            double flux = 0.0;
            for (int k = minBin; k < maxBin; ++k)
            {
                const auto mag = std::log1p (100.0 * std::abs (buf[(size_t) k]));
                flux += std::max (0.0, mag - prev[(size_t) k]);
                prev[(size_t) k] = mag;
            }
            env[f] = (float) flux;
        }
        return env;
    }

    /** 大きい方から 1% の所の値（とびぬけた 1 発に引っぱられない基準） */
    float loudLevel (std::vector<float> v)
    {
        if (v.empty())
            return 1.0f;
        const auto k = (size_t) (0.99 * (double) (v.size() - 1));
        std::nth_element (v.begin(), v.begin() + (long) k, v.end());
        return std::max (1.0e-9f, v[k]);
    }

    /** 1 小節目を決めるための、拍ごとの和音の変わり目（直前の拍とのクロマの違い 0..2） */
    std::vector<float> chordChanges (const std::vector<std::array<float, 12>>& chroma, int chromaHop,
                                     double firstBeatSample, double samplesPerBeat, int beats)
    {
        std::vector<float> change ((size_t) beats, 0.0f);
        std::array<float, 12> prev {};
        bool havePrev = false;
        for (int k = 0; k < beats; ++k)
        {
            const auto a = (firstBeatSample + k * samplesPerBeat) / chromaHop;
            const auto b = (firstBeatSample + (k + 1) * samplesPerBeat) / chromaHop;
            std::array<float, 12> c {};
            int n = 0;
            for (auto i = (int64) std::ceil (a); i < (int64) b && i < (int64) chroma.size(); ++i, ++n)
                if (i >= 0)
                    for (int p = 0; p < 12; ++p)
                        c[(size_t) p] += chroma[(size_t) i][(size_t) p];
            if (n == 0)
                continue;
            double dot = 0.0, na = 0.0, nb = 0.0;
            for (int p = 0; p < 12; ++p)
            {
                dot += (double) c[(size_t) p] * prev[(size_t) p];
                na += (double) c[(size_t) p] * c[(size_t) p];
                nb += (double) prev[(size_t) p] * prev[(size_t) p];
            }
            if (havePrev && na > 0.0 && nb > 0.0)
                change[(size_t) k] = (float) (1.0 - dot / std::sqrt (na * nb));
            prev = c;
            havePrev = true;
        }
        return change;
    }
}

std::vector<std::array<float, 12>> chromaFrames (const float* x, int64 length, double sampleRate, int& hopOut)
{
    const auto frame = (int) nextPow2 ((size_t) juce::roundToInt (sampleRate * 0.186));
    const auto hop = frame / 2;
    hopOut = hop;
    std::vector<std::array<float, 12>> out;
    if (x == nullptr || length < frame || sampleRate <= 0.0)
        return out;

    // 周波数のビン → 音名（55 Hz〜2 kHz）。窓は Hann
    std::vector<int> pc ((size_t) frame / 2, -1);
    for (int k = 1; k < frame / 2; ++k)
    {
        const auto f = (double) k * sampleRate / frame;
        if (f >= 55.0 && f <= 2000.0)
            pc[(size_t) k] = ((int) std::lround (12.0 * std::log2 (f / 440.0) + 69.0)) % 12;
    }
    std::vector<double> window ((size_t) frame);
    for (int i = 0; i < frame; ++i)
        window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / (frame - 1));

    std::vector<Complex> buf ((size_t) frame);
    for (int64 a = 0; a + frame <= length; a += hop)
    {
        for (int i = 0; i < frame; ++i)
            buf[(size_t) i] = x[a + i] * window[(size_t) i];
        fft (buf, false);
        std::array<float, 12> c {};
        for (int k = 1; k < frame / 2; ++k)
            if (pc[(size_t) k] >= 0)
                c[(size_t) pc[(size_t) k]] += (float) std::abs (buf[(size_t) k]);
        out.push_back (c);
    }
    return out;
}

TempoEstimate estimateTempo (const float* x, int64 length, double sampleRate)
{
    TempoEstimate r;
    if (x == nullptr || sampleRate <= 0.0)
        return r;

    const auto hop = juce::jmax (1, juce::roundToInt (sampleRate * envHopSeconds));
    const auto frame = (int) nextPow2 ((size_t) juce::roundToInt (sampleRate * 0.046));
    auto env = onsetEnvelope (x, length, frame, hop);
    const auto fps = sampleRate / hop;
    if ((double) env.size() < fps * 8.0)
        return r;   // 8 秒未満は推定しない

    // 前後 0.5 秒の平均を引いて正の所だけ（ゆっくりした音量の変化を除く）
    {
        const auto w = (int) std::lround (fps * 0.5);
        std::vector<double> sum (env.size() + 1, 0.0);
        for (size_t i = 0; i < env.size(); ++i)
            sum[i + 1] = sum[i] + env[i];
        std::vector<float> d (env.size());
        for (size_t i = 0; i < env.size(); ++i)
        {
            const auto a = i >= (size_t) w ? i - (size_t) w : 0, b = std::min (env.size(), i + (size_t) w + 1);
            d[i] = std::max (0.0f, env[i] - (float) ((sum[b] - sum[a]) / (double) (b - a)));
        }
        env = std::move (d);
    }

    // 1) 自己相関で大まかな周期。120 BPM 付近を少し優先（倍・半分の取り違えを抑える）
    const auto minLag = (int) std::floor (fps * 60.0 / maxBpm), maxLag = (int) std::ceil (fps * 60.0 / minBpm);
    std::vector<double> score ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double s = 0.0;
        for (size_t i = 0; i + (size_t) lag < env.size(); ++i)
            s += (double) env[i] * env[i + (size_t) lag];
        s /= (double) (env.size() - (size_t) lag);
        const auto bpm = fps * 60.0 / lag;
        const auto octaves = std::log2 (bpm / preferredBpm);
        score[(size_t) lag] = s * std::exp (-0.5 * octaves * octaves / (0.9 * 0.9));
    }
    int best = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag)
        if (score[(size_t) lag] > score[(size_t) best])
            best = lag;
    if (score[(size_t) best] <= 0.0)
        return r;
    double lag = best;
    if (best > minLag && best < maxLag)
    {
        const auto a = score[(size_t) best - 1], b = score[(size_t) best], c = score[(size_t) best + 1];
        const auto denom = a - 2.0 * b + c;
        if (std::abs (denom) > 1.0e-20)
            lag += juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denom);
    }
    const auto bpm0 = fps * 60.0 / lag;

    // 2) 曲全体で拍がいちばんそろう BPM と位相（±2%、0.005 BPM 刻み・位相は 1/4 フレーム刻み）
    double bestScore = -1.0, bestBpm = bpm0, bestPhase = 0.0, sumScores = 0.0;
    int tried = 0;
    for (double bpm = bpm0 * 0.98; bpm <= bpm0 * 1.02; bpm += 0.005)
    {
        const auto period = fps * 60.0 / bpm;
        const auto beats = (int) ((double) env.size() / period);
        for (double phase = 0.0; phase < period; phase += 0.25)
        {
            double s = 0.0;
            for (int k = 0; k < beats; ++k)
                s += at (env, phase + k * period);
            s /= juce::jmax (1, beats);
            sumScores += s;
            ++tried;
            if (s > bestScore)
            {
                bestScore = s;
                bestBpm = bpm;
                bestPhase = phase;
            }
        }
    }
    const auto mean = sumScores / juce::jmax (1, tried);
    r.bpm = std::round (bestBpm * 100.0) / 100.0;
    r.confidence = (float) juce::jlimit (0.0, 1.0, mean > 0.0 ? (bestScore / mean - 1.0) / 3.0 : 0.0);

    // 包絡のフレーム f は、窓の頭から 3/4 ほどの所の音の立ち上がりで大きくなる（Hann 窓の上り坂が急な所）
    const auto envOffset = 0.72 * frame;
    const auto period = fps * 60.0 / r.bpm;
    const auto samplesPerBeat = sampleRate * 60.0 / r.bpm;
    int chromaHop = 1;
    const auto chroma = chromaFrames (x, length, sampleRate, chromaHop);

    // 3) 拍の上か裏か（16 分のずれも）：全帯域ではハイハット（裏拍・16 分）が強く出ることがある。
    //    キック・ベース（150 Hz 未満）とスネアの胴（150〜500 Hz）の立ち上がり、和音の変わり目が多い所を拍の上にする。
    //    強さは曲全体の基準（大きい方から 1%）で割る。候補ごとの最大値で割ると、拍の頭に特大の 1 発（キメ・シンバル）がある曲で
    //    拍の上の点が下がり、裏に倒れていた（2026-10-02、実際の曲で拍の線が約 0.3 秒ずれた）
    {
        const auto kick = bandOnsetEnvelope (x, length, frame, hop, sampleRate, 30.0, 150.0);
        const auto snare = bandOnsetEnvelope (x, length, frame, hop, sampleRate, 150.0, 500.0);
        const auto kickLevel = loudLevel (kick), snareLevel = loudLevel (snare);
        auto onBeat = [&] (double phase)
        {
            const auto first = phase * hop + envOffset;
            const auto n = (int) (((double) length - first) / samplesPerBeat);
            const auto ch = chordChanges (chroma, chromaHop, first, samplesPerBeat, n);
            double sum = 0.0;
            for (int k = 0; k < n; ++k)
                sum += std::min (1.5, (double) at (kick, phase + k * period) / kickLevel)
                     + std::min (1.5, (double) at (snare, phase + k * period) / snareLevel)
                     + 2.0 * ch[(size_t) k];
            return sum / juce::jmax (1, n);
        };
        // くし（全帯域）で選んだ位相を少しだけ優先し、半拍・16 分ずらしの方がはっきり拍らしい時だけ動かす
        auto chosen = bestPhase;
        auto chosenScore = onBeat (bestPhase) * 1.05;
        for (int j = 1; j < 4; ++j)
        {
            const auto phase = std::fmod (bestPhase + 0.25 * j * period, period);
            const auto sc = onBeat (phase);
            if (sc > chosenScore)
            {
                chosen = phase;
                chosenScore = sc;
            }
        }
        bestPhase = chosen;
    }

    // 4) 1 小節目：立ち上がりの強さ＋和音の変わり目を 4 拍の周期で足して、小節の頭の拍を選ぶ
    const auto firstBeat = bestPhase * hop + envOffset;   // サンプル
    const auto beats = (int) (((double) length - firstBeat) / samplesPerBeat);
    const auto change = chordChanges (chroma, chromaHop, firstBeat, samplesPerBeat, beats);
    double onsetMax = 1.0e-12;
    for (int k = 0; k < beats; ++k)
        onsetMax = std::max (onsetMax, (double) at (env, bestPhase + k * period));
    double barScore[4] = {};
    for (int k = 0; k < beats; ++k)
        barScore[k % 4] += (double) at (env, bestPhase + k * period) / onsetMax + 2.0 * change[(size_t) k];
    int bar = 0;
    for (int m = 1; m < 4; ++m)
        if (barScore[m] > barScore[bar])
            bar = m;

    // 音が鳴り始めた所（包絡が最大の 10% を最初に超えた所）より前で一番近い小節の頭
    float envMax = 0.0f;
    for (auto v : env) envMax = std::max (envMax, v);
    size_t startFrame = 0;
    while (startFrame < env.size() && env[startFrame] < 0.1f * envMax)
        ++startFrame;
    const auto start = (double) startFrame * hop + envOffset;
    auto downbeat = firstBeat + bar * samplesPerBeat;
    const auto barLength = 4.0 * samplesPerBeat;
    while (downbeat - barLength > start - 0.5 * samplesPerBeat)
        downbeat -= barLength;
    while (downbeat < start - 0.5 * samplesPerBeat)
        downbeat += barLength;
    r.downbeatSample = (int64) std::llround (downbeat);
    r.beatsPerBar = 4;
    return r;
}

KeyEstimate estimateKey (const float* x, int64 length, double sampleRate)
{
    KeyEstimate r;
    int hop = 1;
    const auto frames = chromaFrames (x, length, sampleRate, hop);
    if (frames.size() < 8)
        return r;

    // 曲全体の 12 音のかたより（各フレームを正規化してから足す：大きい所だけに引っぱられない）
    std::array<double, 12> total {};
    for (auto& c : frames)
    {
        double s = 0.0;
        for (auto v : c) s += v;
        if (s <= 1.0e-9)
            continue;
        for (int p = 0; p < 12; ++p)
            total[(size_t) p] += c[(size_t) p] / s;
    }

    // Krumhansl-Kessler の型（長調・短調）と相関。24 通り
    static const double major[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    auto correlate = [&total] (const double* profile, int tonic)
    {
        double mx = 0.0, my = 0.0;
        for (int p = 0; p < 12; ++p) { mx += total[(size_t) p]; my += profile[p]; }
        mx /= 12.0; my /= 12.0;
        double sxy = 0.0, sxx = 0.0, syy = 0.0;
        for (int p = 0; p < 12; ++p)
        {
            const auto a = total[(size_t) ((p + tonic) % 12)] - mx, b = profile[p] - my;
            sxy += a * b; sxx += a * a; syy += b * b;
        }
        return sxx > 0.0 && syy > 0.0 ? sxy / std::sqrt (sxx * syy) : 0.0;
    };

    double best = -2.0, second = -2.0;
    for (int t = 0; t < 12; ++t)
        for (int m = 0; m < 2; ++m)
        {
            const auto c = correlate (m == 0 ? major : minorProfile, t);
            if (c > best)
            {
                second = best;
                best = c;
                r.tonic = t;
                r.minor = m == 1;
            }
            else if (c > second)
                second = c;
        }
    r.confidence = (float) juce::jlimit (0.0, 1.0, (best - second) * 5.0);
    return r;
}
} // namespace vb::analysis
