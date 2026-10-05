#include "Align.h"
#include "Decimate.h"
#include "Fft.h"
#include <algorithm>
#include <numeric>

namespace vb::analysis
{
namespace
{
    constexpr int envFrame = 1024, envHop = 512;   // 44.1 kHz で約 11.6 ms 刻み
    constexpr int fineWindow = 16384;              // GCC-PHAT の窓（44.1 kHz で約 0.37 秒）
    constexpr int maxWindows = 12;
    constexpr double segmentSeconds = 10.0;        // 合う・合わないを見る区間の長さ

    void normalise (std::vector<float>& v)
    {
        if (v.empty()) return;
        const double mean = std::accumulate (v.begin(), v.end(), 0.0) / (double) v.size();
        double sq = 0.0;
        for (auto& x : v) { x = (float) (x - mean); sq += (double) x * x; }
        const auto sd = std::sqrt (sq / (double) v.size());
        if (sd > 1e-12)
            for (auto& x : v) x = (float) (x / sd);
    }

    /** c[lag] = Σ a[k + lag] · b[k]。lag は -(nb-1)..(na-1)。戻り値の添字は lag + (nb - 1) */
    std::vector<double> crossCorrelate (const std::vector<float>& a, const std::vector<float>& b)
    {
        const size_t na = a.size(), nb = b.size();
        if (na == 0 || nb == 0) return {};
        const size_t n = nextPow2 (na + nb);
        std::vector<Complex> A (n), B (n);
        for (size_t i = 0; i < na; ++i) A[i] = a[i];
        for (size_t i = 0; i < nb; ++i) B[i] = b[i];
        fft (A, false);
        fft (B, false);
        for (size_t i = 0; i < n; ++i) A[i] *= std::conj (B[i]);
        fft (A, true);

        std::vector<double> c (na + nb - 1);
        for (size_t idx = 0; idx < c.size(); ++idx)
        {
            const auto lag = (long long) idx - (long long) (nb - 1);
            c[idx] = A[(size_t) ((lag + (long long) n) % (long long) n)].real();
        }
        return c;
    }

    struct Peak { long long lag = 0; double value = 0.0, second = 0.0; };

    /** 一番高い山と、その近く（±guard）を除いた 2 番目 */
    Peak findPeak (const std::vector<double>& c, long long lagOfIndex0, long long guard)
    {
        Peak p;
        size_t best = 0;
        for (size_t i = 1; i < c.size(); ++i)
            if (c[i] > c[best]) best = i;
        p.lag = (long long) best + lagOfIndex0;
        p.value = c.empty() ? 0.0 : c[best];
        for (size_t i = 0; i < c.size(); ++i)
            if (std::llabs ((long long) i - (long long) best) > guard)
                p.second = std::max (p.second, c[i]);
        return p;
    }

    /** GCC-PHAT：ref[start .. start + n + 2m) の中で kar[pos .. pos + n) に一番合う所。戻りは ref 側の開始位置 */
    bool gccPhat (const float* ref, juce::int64 refLen, const float* kar, juce::int64 karLen,
                  juce::int64 karPos, juce::int64 refCentre, int searchRadius, juce::int64& refPosOut, double& peakOut)
    {
        const auto n = (juce::int64) fineWindow;
        const auto refStart = refCentre - searchRadius;
        if (karPos < 0 || karPos + n > karLen || refStart < 0 || refStart + n + 2 * searchRadius > refLen)
            return false;

        double energy = 0.0;
        for (juce::int64 i = 0; i < n; ++i) energy += (double) kar[karPos + i] * kar[karPos + i];
        if (energy / (double) n < 1e-8)
            return false;   // 無音の窓は使わない

        const size_t len = nextPow2 ((size_t) (n + 2 * searchRadius + n));
        std::vector<Complex> X (len), Y (len);
        for (juce::int64 i = 0; i < n; ++i) X[(size_t) i] = kar[karPos + i];
        for (juce::int64 i = 0; i < n + 2 * searchRadius; ++i) Y[(size_t) i] = ref[refStart + i];
        fft (X, false);
        fft (Y, false);
        for (size_t i = 0; i < len; ++i)
        {
            auto r = Y[i] * std::conj (X[i]);
            const auto mag = std::abs (r);
            Y[i] = mag > 1e-12 ? r / mag : Complex();
        }
        fft (Y, true);

        int best = 0;
        for (int d = 1; d <= 2 * searchRadius; ++d)
            if (Y[(size_t) d].real() > Y[(size_t) best].real()) best = d;
        refPosOut = refStart + best;
        peakOut = Y[(size_t) best].real();
        return true;
    }

    struct Fit { double a = 0.0, b = 0.0; };

    /** lag = a + b · pos の最小二乗 */
    Fit fitLine (const std::vector<std::pair<double, double>>& pts)
    {
        Fit f;
        if (pts.empty()) return f;
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (auto& [x, y] : pts) { sx += x; sy += y; sxx += x * x; sxy += x * y; }
        const double n = (double) pts.size(), den = n * sxx - sx * sx;
        if (pts.size() < 2 || std::abs (den) < 1e-9) { f.a = sy / n; return f; }
        f.b = (n * sxy - sx * sy) / den;
        f.a = (sy - f.b * sx) / n;
        return f;
    }

    double median (std::vector<double> v)
    {
        if (v.empty()) return 0.0;
        std::sort (v.begin(), v.end());
        return v[v.size() / 2];
    }

    /** 包絡どうしの相関係数（同じ長さ） */
    double correlation (const float* a, const float* b, size_t n)
    {
        double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
        for (size_t i = 0; i < n; ++i)
        {
            sa += a[i]; sb += b[i]; saa += (double) a[i] * a[i]; sbb += (double) b[i] * b[i]; sab += (double) a[i] * b[i];
        }
        const double nn = (double) n;
        const double cov = sab / nn - (sa / nn) * (sb / nn);
        const double va = saa / nn - (sa / nn) * (sa / nn), vb = sbb / nn - (sb / nn) * (sb / nn);
        return (va > 1e-12 && vb > 1e-12) ? cov / std::sqrt (va * vb) : 0.0;
    }
}

//==============================================================================
std::vector<float> onsetEnvelope (const float* x, juce::int64 length, int frameSize, int hop)
{
    std::vector<float> env;
    if (length < frameSize) return env;

    const auto frames = (size_t) ((length - frameSize) / hop + 1);
    env.resize (frames, 0.0f);
    std::vector<double> window ((size_t) frameSize), prev ((size_t) frameSize / 2, 0.0);
    for (int i = 0; i < frameSize; ++i)
        window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / (frameSize - 1));

    std::vector<Complex> buf ((size_t) frameSize);
    for (size_t f = 0; f < frames; ++f)
    {
        const auto* p = x + (juce::int64) f * hop;
        for (int i = 0; i < frameSize; ++i) buf[(size_t) i] = p[i] * window[(size_t) i];
        fft (buf, false);
        double flux = 0.0;
        for (int k = 1; k < frameSize / 2; ++k)
        {
            const auto mag = std::log1p (100.0 * std::abs (buf[(size_t) k]));
            flux += std::max (0.0, mag - prev[(size_t) k]);
            prev[(size_t) k] = mag;
        }
        env[f] = (float) flux;
    }
    return env;
}

static AlignResult alignReferenceAtRate (const float* ref, juce::int64 refLen, const float* kar, juce::int64 karLen, double sampleRate)
{
    AlignResult r;
    auto envR = onsetEnvelope (ref, refLen, envFrame, envHop);
    auto envK = onsetEnvelope (kar, karLen, envFrame, envHop);
    if (envR.size() < 16 || envK.size() < 16)
        return r;
    normalise (envR);
    normalise (envK);

    // 1) 粗く：包絡の相互相関（数秒単位のずれ）
    const auto c = crossCorrelate (envR, envK);
    const auto peak = findPeak (c, -(long long) (envK.size() - 1), 8);
    if (peak.value <= 0.0)
        return r;
    r.confidence = juce::jlimit (0.0, 1.0, 1.0 - peak.second / peak.value);
    const auto coarse = (juce::int64) peak.lag * envHop;   // 原曲の位置 ≈ オフボの位置 + coarse

    // 2) 細かく：オフボの数か所で GCC-PHAT。1 回目は広く、線を引いてから 2 回目は狭く
    auto measure = [&] (std::function<juce::int64 (juce::int64)> centreFor, int radius)
    {
        std::vector<std::pair<double, double>> pts;   // (オフボの位置, ずれ)
        const auto usable = karLen - fineWindow;
        for (int w = 0; w < maxWindows && usable > 0; ++w)
        {
            const auto pos = (juce::int64) ((double) usable * (w + 0.5) / maxWindows);
            juce::int64 refPos = 0;
            double pk = 0.0;
            if (gccPhat (ref, refLen, kar, karLen, pos, centreFor (pos), radius, refPos, pk) && pk > 0.08)
                pts.push_back ({ (double) pos + fineWindow / 2, (double) (refPos - pos) });   // 窓の真ん中のずれとして扱う
        }
        return pts;
    };

    auto pts = measure ([coarse] (juce::int64 pos) { return pos + coarse; }, 4096);
    r.windowsUsed = (int) pts.size();
    if (pts.size() < 3)
    {
        // 窓で確かめられない：粗い値だけ（推定）
        r.quality = r.confidence > 0.15 ? AlignResult::Quality::rough : AlignResult::Quality::none;
        r.offsetSamples = coarse;
        if (r.found())
            r.covered.push_back ({ 0, karLen, coarse });
        return r;
    }

    // 外れた窓（別の所に合った等）を除いて線を引く
    std::vector<double> lags;
    for (auto& p : pts) lags.push_back (p.second);
    const auto med = median (lags);
    std::vector<std::pair<double, double>> inliers;
    for (auto& p : pts)
        if (std::abs (p.second - med) < 0.02 * sampleRate + 1e-3 * p.first)   // 20 ms ＋ 速さの違い（0.1%）まで
            inliers.push_back (p);
    auto fit = fitLine (inliers);

    // 2 回目：引いた線の近く ±256 サンプルだけを探してサンプル単位に詰める
    pts = measure ([fit] (juce::int64 pos) { return pos + (juce::int64) std::llround (fit.a + fit.b * (double) (pos + fineWindow / 2)); }, 256);
    inliers.clear();
    for (auto& p : pts)
        if (std::abs (p.second - (fit.a + fit.b * p.first)) <= 64.0)
            inliers.push_back (p);
    r.windowsAgreeing = (int) inliers.size();
    if (inliers.size() < 3)
    {
        r.quality = AlignResult::Quality::rough;
        r.offsetSamples = coarse;
        r.covered.push_back ({ 0, karLen, coarse });
        return r;
    }

    fit = fitLine (inliers);
    if (std::abs (fit.b) < 5e-6)
    {
        // 速さは同じ：ずれは窓の中央値（整数）
        std::vector<double> l;
        for (auto& p : inliers) l.push_back (p.second);
        r.offsetSamples = (juce::int64) std::llround (median (l));
        r.tempoRatio = 1.0;
    }
    else
    {
        r.offsetSamples = (juce::int64) std::llround (fit.a);
        r.tempoRatio = 1.0 + fit.b;
    }
    r.quality = AlignResult::Quality::good;

    // 3) 区間ごとに合うかを確かめる（カット版）。合わない区間は、その区間だけで別のずれを探す
    const auto segFrames = (size_t) std::max (16.0, segmentSeconds * sampleRate / envHop);
    std::vector<Covered> segs;
    for (size_t s0 = 0; s0 < envK.size(); s0 += segFrames)
    {
        const auto n = std::min (segFrames, envK.size() - s0);
        if (n < 16) break;
        const auto karStart = (juce::int64) s0 * envHop;
        const auto karEnd = s0 + n >= envK.size() ? karLen : (juce::int64) (s0 + n) * envHop;

        auto mapped = (long long) std::llround (r.referencePosition (karStart) / envHop);
        // 原曲と重なるフレーム [lo, hi) だけで比べる。区間の一部が原曲の外（オフボの前奏が長い・後奏が長い）でも、
        // 重なる部分が合えばその部分はお手本あり（前は区間ごと外れ、頭の数秒のお手本が消えた。バグチェック 2026-10-05）
        const auto lo = juce::jlimit (0LL, (long long) n, -mapped);
        const auto hi = juce::jlimit (lo, (long long) n, (long long) envR.size() - mapped);
        double corr = -1.0;
        if (hi - lo >= 16 && hi - lo >= (long long) n / 3)
            corr = correlation (envK.data() + s0 + (size_t) lo, envR.data() + mapped + lo, (size_t) (hi - lo));

        if (corr > 0.3)
        {
            const auto start = lo > 0 ? karStart + (juce::int64) lo * envHop : karStart;
            const auto end = hi < (long long) n ? (juce::int64) (s0 + (size_t) hi) * envHop : karEnd;
            segs.push_back ({ start, end, r.offsetSamples, r.tempoRatio });   // 全体の速さの比も持つ（2026-10-03）
            continue;
        }

        // 短い端（区間の 1/3 未満）は単独では探さない：直前の区間のずれで合えばつなぐ
        if (n < segFrames / 3)
        {
            if (! segs.empty() && segs.back().karaokeEnd == karStart)
            {
                // 直前の区間の速さの比も使う（offset だけだと、速さの違う曲で端がずれて見えた。バグチェック 2026-10-05）
                const auto m = (long long) std::llround (segs.back().referencePosition (karStart) / envHop);
                if (m >= 0 && (size_t) m + n <= envR.size() && correlation (envK.data() + s0, envR.data() + m, n) > 0.3)
                    segs.back().karaokeEnd = karEnd;
            }
            continue;
        }

        // この区間だけで探す
        std::vector<float> seg (envK.begin() + (long) s0, envK.begin() + (long) (s0 + n));
        const auto cs = crossCorrelate (envR, seg);
        const auto pk = findPeak (cs, -(long long) (n - 1), 8);
        if (pk.lag < 0 || (size_t) pk.lag + n > envR.size())
            continue;
        if (correlation (seg.data(), envR.data() + pk.lag, n) < 0.4)
            continue;   // どこにも合わない：お手本なし

        const auto segCoarse = (juce::int64) pk.lag * envHop - karStart;
        juce::int64 refPos = 0;
        double pkv = 0.0;
        const auto probe = juce::jmin (karStart + (karEnd - karStart) / 2, karLen - fineWindow);
        auto offset = segCoarse;
        if (gccPhat (ref, refLen, kar, karLen, probe, probe + segCoarse, 1024, refPos, pkv) && pkv > 0.08)
            offset = refPos - probe;
        segs.push_back ({ karStart, karEnd, offset });
    }

    // ずれの違う区間が隣り合う所（カットの継ぎ目）は、区間の幅（10 秒）ではなく包絡の刻み（約 12 ms）で境目を探す
    {
        const long long win = (long long) std::max (8.0, 1.0 * sampleRate / envHop);   // 約 1 秒の窓で合い方を見る
        auto localMatch = [&] (long long frame, const Covered& seg)
        {
            const auto m = (long long) std::llround (seg.referencePosition ((juce::int64) frame * envHop) / envHop);   // 速さの比も使う
            if (frame < 0 || frame + win > (long long) envK.size() || m < 0 || m + win > (long long) envR.size())
                return -1.0;
            return correlation (envK.data() + frame, envR.data() + m, (size_t) win);
        };
        for (size_t i = 1; i < segs.size(); ++i)
        {
            auto& a = segs[i - 1];
            auto& b = segs[i];
            if (a.karaokeEnd != b.karaokeStart || std::llabs (a.offsetSamples - b.offsetSamples) <= 2)
                continue;
            const auto lo = (long long) (a.karaokeStart / envHop), hi = (long long) (b.karaokeEnd / envHop) - win;
            // 境目 f で「前は a のずれ、後は b のずれ」が一番よく合う所
            std::vector<double> ma, mb;
            for (long long f = lo; f <= hi; ++f) { ma.push_back (localMatch (f, a)); mb.push_back (localMatch (f, b)); }
            if (ma.size() < 2) continue;
            std::vector<double> preA (ma.size() + 1, 0.0), sufB (mb.size() + 1, 0.0);
            for (size_t k = 0; k < ma.size(); ++k) preA[k + 1] = preA[k] + ma[k];
            for (size_t k = mb.size(); k-- > 0;) sufB[k] = sufB[k + 1] + mb[k];
            size_t bestK = 0;
            for (size_t k = 0; k <= ma.size(); ++k)
                if (preA[k] + sufB[k] > preA[bestK] + sufB[bestK]) bestK = k;
            // 窓は f から後ろを見るので、境目をまたぐ窓は半分ずつ合う。最適な f は継ぎ目の窓半分手前
            const auto boundary = juce::jlimit (a.karaokeStart, b.karaokeEnd, (juce::int64) (lo + (long long) bestK) * envHop + (juce::int64) win * envHop / 2);
            a.karaokeEnd = b.karaokeStart = boundary;
        }
    }

    // 隣り合って同じずれの区間はつなぐ
    for (auto& s : segs)
    {
        if (! r.covered.empty() && r.covered.back().karaokeEnd == s.karaokeStart
            && std::llabs (r.covered.back().offsetSamples - s.offsetSamples) <= 2
            && std::abs (r.covered.back().tempoRatio - s.tempoRatio) < 1.0e-9)
            r.covered.back().karaokeEnd = s.karaokeEnd;
        else
            r.covered.push_back (s);
    }
    return r;
}

static KeyShiftResult estimateKeyShiftAtRate (const float* ref, juce::int64 refLen, const float* kar, juce::int64 karLen, double sampleRate)
{
    auto chroma = [sampleRate] (const float* x, juce::int64 len)
    {
        constexpr int n = 8192;
        std::array<double, 12> c {};
        std::vector<Complex> buf (n);
        for (juce::int64 start = 0; start + n <= len; start += n)
        {
            for (int i = 0; i < n; ++i)
                buf[(size_t) i] = x[start + i] * (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / (n - 1)));
            fft (buf, false);
            for (int k = 1; k < n / 2; ++k)
            {
                const auto f = k * sampleRate / n;
                if (f < 60.0 || f > 2000.0) continue;
                const auto midi = 69.0 + 12.0 * std::log2 (f / 440.0);
                const auto pc = ((int) std::lround (midi) % 12 + 12) % 12;
                c[(size_t) pc] += std::abs (buf[(size_t) k]);
            }
        }
        const double mean = std::accumulate (c.begin(), c.end(), 0.0) / 12.0;
        double norm = 0.0;
        for (auto& v : c) { v -= mean; norm += v * v; }
        norm = std::sqrt (norm);
        if (norm > 1e-12)
            for (auto& v : c) v /= norm;
        return c;
    };

    const auto cr = chroma (ref, refLen), ck = chroma (kar, karLen);
    std::array<double, 12> score {};
    for (int s = 0; s < 12; ++s)
        for (int i = 0; i < 12; ++i)
            score[(size_t) s] += cr[(size_t) i] * ck[(size_t) ((i + s) % 12)];

    int best = 0;
    for (int s = 1; s < 12; ++s)
        if (score[(size_t) s] > score[(size_t) best]) best = s;
    double second = -1.0;
    for (int s = 0; s < 12; ++s)
        if (s != best) second = std::max (second, score[(size_t) s]);

    KeyShiftResult k;
    k.semitones = best >= 6 ? best - 12 : best;
    k.confidence = score[(size_t) best] > 0.0 ? juce::jlimit (0.0, 1.0, (score[(size_t) best] - second) / score[(size_t) best]) : 0.0;
    return k;
}
LocalLag localLag (const float* a, juce::int64 aLength, const float* b, juce::int64 bLength,
                   juce::int64 center, juce::int64 window, juce::int64 maxLag)
{
    LocalLag out;
    const auto from = juce::jmax ((juce::int64) 0, center - window), to = juce::jmin (bLength, center + window);
    if (to - from < 64 || aLength <= 0)
        return out;

    // 正規化した相関。x(i) と y(i + lag) を i = [lo, hi) で
    auto corr = [] (auto&& x, auto&& y, juce::int64 lo, juce::int64 hi, juce::int64 lag, juce::int64 yLength) -> double
    {
        double xy = 0.0, xx = 0.0, yy = 0.0;
        for (auto i = lo; i < hi; ++i)
        {
            const auto j = i + lag;
            if (j < 0 || j >= yLength)
                continue;
            const double p = x (i), q = y (j);
            xy += p * q; xx += p * p; yy += q * q;
        }
        return xx > 1e-9 && yy > 1e-9 ? xy / std::sqrt (xx * yy) : -2.0;
    };

    // 粗く：4 サンプルずつ平均して間引いた音で（高い音が消えるので、間引いても外さない）
    static constexpr int coarse = 4;   // static：ラムダでキャプチャせずに使える（MSVC は constexpr のローカルも暗黙にはキャプチャしない）
    auto decimate = [] (const float* x, juce::int64 n)
    {
        std::vector<float> d ((size_t) (n / coarse));
        for (size_t k = 0; k < d.size(); ++k)
            d[k] = 0.25f * (x[k * coarse] + x[k * coarse + 1] + x[k * coarse + 2] + x[k * coarse + 3]);
        return d;
    };
    const auto da = decimate (a, aLength), db = decimate (b, bLength);
    const auto dLen = (juce::int64) da.size();
    double best = -2.0;
    juce::int64 bestLag = 0;
    for (auto lag = -maxLag / coarse; lag <= maxLag / coarse; ++lag)
    {
        const auto s = corr ([&] (juce::int64 i) { return db[(size_t) i]; }, [&] (juce::int64 j) { return da[(size_t) j]; },
                             from / coarse, juce::jmin ((juce::int64) db.size(), to / coarse), lag, dLen);
        if (s > best) { best = s; bestLag = lag * coarse; }
    }
    if (best <= -2.0)
        return out;

    // 細かく：元の SR で前後 8 サンプル
    const auto centreLag = bestLag;
    best = -2.0;
    for (auto lag = centreLag - 2 * coarse; lag <= centreLag + 2 * coarse; ++lag)
    {
        const auto s = corr ([&] (juce::int64 i) { return b[i]; }, [&] (juce::int64 j) { return a[j]; }, from, to, lag, aLength);
        if (s > best) { best = s; bestLag = lag; }
    }
    out.found = best > -2.0;
    out.lag = bestLag;
    out.correlation = best;
    return out;
}
//==============================================================================
/** 下げた SR で求めたずれ（k サンプル単位）を、元の SR で ±k サンプルの中から詰め直す。
    区間の中の 3 か所の窓で相関を足し、一番合うずれ。窓が取れなければそのまま */
static juce::int64 refineOffset (const float* ref, juce::int64 refLen, const float* kar, juce::int64 karLen, const Covered& c, int k)
{
    constexpr juce::int64 win = 16384;
    double best = -2.0;
    auto bestOffset = c.offsetSamples;
    for (int d = -k; d <= k; ++d)
    {
        auto shifted = c;
        shifted.offsetSamples += d;
        double xy = 0.0, xx = 0.0, yy = 0.0;
        for (int p = 1; p <= 3; ++p)
        {
            const auto start = c.karaokeStart + (c.karaokeEnd - c.karaokeStart) * p / 4 - win / 2;
            const auto refStart = (juce::int64) std::llround (shifted.referencePosition (start));
            if (start < 0 || start + win > karLen || refStart < 0 || refStart + win > refLen)
                continue;
            for (juce::int64 i = 0; i < win; ++i)
            {
                const double x = kar[start + i], y = ref[refStart + i];
                xy += x * y; xx += x * x; yy += y * y;
            }
        }
        if (xx <= 1.0e-12 || yy <= 1.0e-12)
            continue;
        const auto score = xy / std::sqrt (xx * yy);
        if (score > best) { best = score; bestOffset = shifted.offsetSamples; }
    }
    return bestOffset;
}

// 高い SR の曲は 48 kHz 前後まで下げてから合わせる（#21）。ずれ・区間は元の SR に戻す。
// 下げた SR のずれは k サンプル単位なので、元の SR で詰め直す（原曲−オフボの引き算はサンプル単位で合っていないと
// オフボの高い音が残る。バグチェック 2026-10-05）
AlignResult alignReference (const float* ref, juce::int64 refLen, const float* kar, juce::int64 karLen, double sampleRate)
{
    const auto k = analysisFactor (sampleRate);
    if (k <= 1)
        return alignReferenceAtRate (ref, refLen, kar, karLen, sampleRate);
    const auto r = decimate (ref, refLen, k), q = decimate (kar, karLen, k);
    auto a = alignReferenceAtRate (r.data(), (juce::int64) r.size(), q.data(), (juce::int64) q.size(), sampleRate / k);
    a.offsetSamples *= k;
    if (a.found())
        a.offsetSamples = refineOffset (ref, refLen, kar, karLen, { 0, karLen, a.offsetSamples, a.tempoRatio }, k);
    for (auto& c : a.covered)
    {
        c.karaokeStart = juce::jmin (c.karaokeStart * k, karLen);
        c.karaokeEnd = juce::jmin (c.karaokeEnd * k, karLen);
        c.offsetSamples = refineOffset (ref, refLen, kar, karLen, { c.karaokeStart, c.karaokeEnd, c.offsetSamples * k, c.tempoRatio }, k);
    }
    return a;
}

KeyShiftResult estimateKeyShift (const float* ref, juce::int64 refLen, const float* kar, juce::int64 karLen, double sampleRate)
{
    const auto k = analysisFactor (sampleRate);
    if (k <= 1)
        return estimateKeyShiftAtRate (ref, refLen, kar, karLen, sampleRate);
    const auto r = decimate (ref, refLen, k), q = decimate (kar, karLen, k);
    return estimateKeyShiftAtRate (r.data(), (juce::int64) r.size(), q.data(), (juce::int64) q.size(), sampleRate / k);
}
} // namespace vb::analysis
