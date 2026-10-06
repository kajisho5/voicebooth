#include "Rmvpe.h"
#include "Fft.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace vb::analysis::rmvpe
{
namespace
{
    constexpr double pi = 3.14159265358979323846;

    // 強さ → 声のある確率（isotonic。pitchbench out/calib.json の rmvpe q。MIR-1K dev2 で合わせた）
    constexpr float calib[][2] = {
        { 0.00002137f, 0.001000f }, { 0.00063196f, 0.001000f }, { 0.00063202f, 0.001587f }, { 0.00113347f, 0.001587f },
        { 0.00113368f, 0.001957f }, { 0.00122491f, 0.001957f }, { 0.00122586f, 0.003240f }, { 0.00188634f, 0.003240f },
        { 0.00188637f, 0.003775f }, { 0.00254402f, 0.003775f }, { 0.00254422f, 0.005780f }, { 0.00365537f, 0.005780f },
        { 0.00365540f, 0.009825f }, { 0.00403795f, 0.009825f }, { 0.00403821f, 0.013274f }, { 0.00435913f, 0.013274f },
        { 0.00435925f, 0.014395f }, { 0.00537795f, 0.014395f }, { 0.00537929f, 0.019602f }, { 0.00751114f, 0.019602f },
        { 0.00751141f, 0.023877f }, { 0.00950196f, 0.023877f }, { 0.00950232f, 0.023929f }, { 0.01014566f, 0.023929f },
        { 0.01014593f, 0.032021f }, { 0.01260206f, 0.032021f }, { 0.01260215f, 0.037938f }, { 0.01358590f, 0.037938f },
        { 0.01358899f, 0.039548f }, { 0.01414114f, 0.039548f }, { 0.01414192f, 0.047035f }, { 0.01570427f, 0.047035f },
        { 0.01570469f, 0.052111f }, { 0.01692677f, 0.052111f }, { 0.01693013f, 0.053892f }, { 0.01794595f, 0.053892f },
        { 0.01794708f, 0.055100f }, { 0.02259523f, 0.055100f }, { 0.02259627f, 0.061404f }, { 0.02314913f, 0.061404f },
        { 0.02315134f, 0.071429f }, { 0.02316621f, 0.071429f }, { 0.02316755f, 0.072037f }, { 0.02574816f, 0.072037f },
        { 0.02574995f, 0.074786f }, { 0.02652416f, 0.074786f }, { 0.02652463f, 0.076923f }, { 0.02686548f, 0.076923f },
        { 0.02686685f, 0.076999f }, { 0.02875659f, 0.076999f }, { 0.02875814f, 0.084826f }, { 0.03419119f, 0.084826f },
        { 0.03419131f, 0.097491f }, { 0.03708896f, 0.097491f }, { 0.03709257f, 0.098139f }, { 0.04107848f, 0.098139f },
        { 0.04107851f, 0.099609f }, { 0.04229906f, 0.099609f }, { 0.04230005f, 0.108696f }, { 0.04242817f, 0.108696f },
        { 0.04243016f, 0.127726f }, { 0.04559112f, 0.127726f }, { 0.04559177f, 0.137778f }, { 0.04612580f, 0.137778f },
        { 0.04612589f, 0.143091f }, { 0.04920566f, 0.143091f }, { 0.04920638f, 0.152174f }, { 0.04968947f, 0.152174f },
        { 0.04969025f, 0.156688f }, { 0.05179825f, 0.156688f }, { 0.05179861f, 0.157216f }, { 0.05727598f, 0.157216f },
        { 0.05727607f, 0.157895f }, { 0.05747205f, 0.157895f }, { 0.05747232f, 0.162821f }, { 0.05989280f, 0.162821f },
        { 0.05989370f, 0.171642f }, { 0.06078762f, 0.171642f }, { 0.06079045f, 0.195018f }, { 0.06617624f, 0.195018f },
        { 0.06618136f, 0.204348f }, { 0.06696880f, 0.204348f }, { 0.06696919f, 0.214286f }, { 0.06701398f, 0.214286f },
        { 0.06701902f, 0.219560f }, { 0.07422432f, 0.219560f }, { 0.07423308f, 0.220779f }, { 0.07558823f, 0.220779f },
        { 0.07559577f, 0.224069f }, { 0.08147445f, 0.224069f }, { 0.08147529f, 0.228616f }, { 0.08404851f, 0.228616f },
        { 0.08404890f, 0.233198f }, { 0.08918670f, 0.233198f }, { 0.08918768f, 0.236025f }, { 0.09064001f, 0.236025f },
        { 0.09064335f, 0.251282f }, { 0.09680897f, 0.251282f }, { 0.09680918f, 0.257019f }, { 0.09901518f, 0.257019f },
        { 0.09901634f, 0.272727f }, { 0.09906206f, 0.272727f }, { 0.09906349f, 0.277622f }, { 0.10898322f, 0.277622f },
        { 0.10898721f, 0.285182f }, { 0.11447904f, 0.285182f }, { 0.11448616f, 0.290323f }, { 0.11502463f, 0.290323f },
        { 0.11504227f, 0.300546f }, { 0.11588463f, 0.300546f }, { 0.11589506f, 0.303483f }, { 0.11781529f, 0.303483f },
        { 0.11781627f, 0.309735f }, { 0.11834911f, 0.309735f }, { 0.11835235f, 0.312000f }, { 0.12407818f, 0.312000f },
        { 0.12408099f, 0.317585f }, { 0.12822956f, 0.317585f }, { 0.12823832f, 0.322200f }, { 0.13123882f, 0.322200f },
        { 0.13124084f, 0.333333f }, { 0.13130066f, 0.333333f }, { 0.13130090f, 0.333977f }, { 0.13763317f, 0.333977f },
        { 0.13766235f, 0.335616f }, { 0.14301887f, 0.335616f }, { 0.14301899f, 0.347518f }, { 0.15247583f, 0.347518f },
        { 0.15247834f, 0.352066f }, { 0.15640223f, 0.352066f }, { 0.15640292f, 0.375000f }, { 0.15698206f, 0.375000f },
        { 0.15698391f, 0.397196f }, { 0.16002306f, 0.397196f }, { 0.16003484f, 0.402332f }, { 0.16246024f, 0.402332f },
        { 0.16247374f, 0.420162f }, { 0.17309925f, 0.420162f }, { 0.17310083f, 0.425220f }, { 0.18090737f, 0.425220f },
        { 0.18091735f, 0.427885f }, { 0.18263593f, 0.427885f }, { 0.18263787f, 0.435374f }, { 0.18568665f, 0.435374f },
        { 0.18569255f, 0.436823f }, { 0.18766919f, 0.436823f }, { 0.18766972f, 0.446058f }, { 0.19514331f, 0.446058f },
        { 0.19514999f, 0.470822f }, { 0.20082447f, 0.470822f }, { 0.20082569f, 0.488152f }, { 0.20972139f, 0.488152f },
        { 0.20975223f, 0.491961f }, { 0.22018561f, 0.491961f }, { 0.22018892f, 0.494565f }, { 0.22336626f, 0.494565f },
        { 0.22337803f, 0.500000f }, { 0.22338110f, 0.500000f }, { 0.22338265f, 0.519676f }, { 0.23099676f, 0.519676f },
        { 0.23100069f, 0.527586f }, { 0.25374228f, 0.527586f }, { 0.25375086f, 0.547945f }, { 0.25437686f, 0.547945f },
        { 0.25439933f, 0.578818f }, { 0.27599132f, 0.578818f }, { 0.27600574f, 0.578947f }, { 0.27613890f, 0.578947f },
        { 0.27614063f, 0.597064f }, { 0.28158188f, 0.597064f }, { 0.28158447f, 0.613043f }, { 0.28373182f, 0.613043f },
        { 0.28373349f, 0.622368f }, { 0.31246704f, 0.622368f }, { 0.31249291f, 0.634188f }, { 0.31877226f, 0.634188f },
        { 0.31877452f, 0.636364f }, { 0.31884378f, 0.636364f }, { 0.31885615f, 0.642857f }, { 0.31900159f, 0.642857f },
        { 0.31900865f, 0.670732f }, { 0.31975204f, 0.670732f }, { 0.31977248f, 0.680168f }, { 0.34492302f, 0.680168f },
        { 0.34496516f, 0.684606f }, { 0.35285115f, 0.684606f }, { 0.35285389f, 0.692308f }, { 0.35303193f, 0.692308f },
        { 0.35303375f, 0.694545f }, { 0.35575536f, 0.694545f }, { 0.35576332f, 0.720379f }, { 0.36670083f, 0.720379f },
        { 0.36671799f, 0.721805f }, { 0.36820382f, 0.721805f }, { 0.36820620f, 0.734892f }, { 0.39040303f, 0.734892f },
        { 0.39043033f, 0.750158f }, { 0.40683967f, 0.750158f }, { 0.40684292f, 0.781651f }, { 0.41773945f, 0.781651f },
        { 0.41777062f, 0.782051f }, { 0.45693165f, 0.782051f }, { 0.45693469f, 0.785714f }, { 0.46118870f, 0.785714f },
        { 0.46119294f, 0.786952f }, { 0.47034389f, 0.786952f }, { 0.47034609f, 0.799003f }, { 0.48147404f, 0.799003f },
        { 0.48147452f, 0.801282f }, { 0.48796561f, 0.801282f }, { 0.48796585f, 0.823917f }, { 0.50497293f, 0.823917f },
        { 0.50499445f, 0.830508f }, { 0.50605768f, 0.830508f }, { 0.50605994f, 0.851406f }, { 0.51262021f, 0.851406f },
        { 0.51262218f, 0.852861f }, { 0.51585174f, 0.852861f }, { 0.51588368f, 0.853834f }, { 0.52699488f, 0.853834f },
        { 0.52699882f, 0.868109f }, { 0.55891109f, 0.868109f }, { 0.55891597f, 0.869942f }, { 0.56207025f, 0.869942f },
        { 0.56208569f, 0.877019f }, { 0.56852204f, 0.877019f }, { 0.56852704f, 0.877670f }, { 0.57265323f, 0.877670f },
        { 0.57265478f, 0.878882f }, { 0.58547479f, 0.878882f }, { 0.58547521f, 0.880933f }, { 0.61353427f, 0.880933f },
        { 0.61353612f, 0.899485f }, { 0.63031721f, 0.899485f }, { 0.63031900f, 0.906404f }, { 0.63575906f, 0.906404f },
        { 0.63576245f, 0.907981f }, { 0.65747482f, 0.907981f }, { 0.65749419f, 0.913043f }, { 0.65773201f, 0.913043f },
        { 0.65774655f, 0.918455f }, { 0.65926588f, 0.918455f }, { 0.65926600f, 0.923434f }, { 0.68789732f, 0.923434f },
        { 0.68789840f, 0.925816f }, { 0.68985128f, 0.925816f }, { 0.68985212f, 0.927083f }, { 0.69031417f, 0.927083f },
        { 0.69031751f, 0.936170f }, { 0.69056869f, 0.936170f }, { 0.69056892f, 0.940693f }, { 0.70819539f, 0.940693f },
        { 0.70820618f, 0.944444f }, { 0.70881402f, 0.944444f }, { 0.70881623f, 0.944871f }, { 0.72981536f, 0.944871f },
        { 0.72981912f, 0.946502f }, { 0.73311168f, 0.946502f }, { 0.73311949f, 0.950495f }, { 0.73358852f, 0.950495f },
        { 0.73358941f, 0.954545f }, { 0.73368669f, 0.954545f }, { 0.73369801f, 0.956395f }, { 0.73670024f, 0.956395f },
        { 0.73671085f, 0.958660f }, { 0.74824864f, 0.958660f }, { 0.74825317f, 0.967727f }, { 0.76580203f, 0.967727f },
        { 0.76580667f, 0.969522f }, { 0.77458346f, 0.969522f }, { 0.77458382f, 0.969900f }, { 0.77546722f, 0.969900f },
        { 0.77546859f, 0.972873f }, { 0.77823222f, 0.972873f }, { 0.77823228f, 0.976629f }, { 0.78930819f, 0.976629f },
        { 0.78930914f, 0.979339f }, { 0.78999805f, 0.979339f }, { 0.79000843f, 0.983283f }, { 0.79371721f, 0.983283f },
        { 0.79372257f, 0.984359f }, { 0.81289756f, 0.984359f }, { 0.81289887f, 0.988100f }, { 0.81858492f, 0.988100f },
        { 0.81859034f, 0.989226f }, { 0.82906342f, 0.989226f }, { 0.82906544f, 0.993314f }, { 0.83355117f, 0.993314f },
        { 0.83355510f, 0.993831f }, { 0.83609098f, 0.993831f }, { 0.83609247f, 0.994044f }, { 0.85236585f, 0.994044f },
        { 0.85236913f, 0.994146f }, { 0.86032397f, 0.994146f }, { 0.86032534f, 0.995133f }, { 0.87239867f, 0.995133f },
        { 0.87239963f, 0.996466f }, { 0.89564300f, 0.996466f }, { 0.89564431f, 0.997236f }, { 0.91884178f, 0.997236f },
        { 0.91884309f, 0.997996f }, { 0.92764437f, 0.997996f }, { 0.92764479f, 0.998210f }, { 0.95347106f, 0.998210f },
        { 0.95347214f, 0.998822f }, { 0.95972306f, 0.998822f }, { 0.95972311f, 0.998837f }, { 0.96400809f, 0.998837f },
        { 0.96400821f, 0.999000f }, { 0.98116863f, 0.999000f },
    };

    // Viterbi の設定（pitchbench out/tuned.json の rmvpe1.viterbi）
    constexpr float smallStep = 250.0f;   // これ以下の動きは近さに比例した小さな罰
    constexpr float lam = 1.0f;
    constexpr float jump = 2.0f;          // それより大きい跳び
    constexpr float voicedSwitch = 1.0f;  // 声あり ↔ 無声
    constexpr float shiftBeta = 0.15f;    // ±1 オクターブの候補の重み
    constexpr float minCandidate = 2400.0f, maxCandidate = 9000.0f;   // 40 Hz .. 1.8 kHz

    float transition (float a, float b)
    {
        const auto d = std::abs (a - b);
        if (d <= smallStep)
            return -lam * d / 100.0f;
        return -(lam * smallStep / 100.0f + jump);
    }

    std::vector<float> melFilters()
    {
        // librosa.filters.mel(sr=16000, n_fft=1024, n_mels=128, fmin=30, fmax=8000, htk=True)（norm="slaney"）
        auto toMel = [] (double f) { return 2595.0 * std::log10 (1.0 + f / 700.0); };
        auto toHz = [] (double m) { return 700.0 * (std::pow (10.0, m / 2595.0) - 1.0); };
        const int nBins = nFft / 2 + 1;
        std::vector<double> edges (mels + 2);
        const auto lo = toMel (30.0), hi = toMel (8000.0);
        for (int i = 0; i < mels + 2; ++i)
            edges[(size_t) i] = toHz (lo + (hi - lo) * i / (mels + 1));
        std::vector<float> w ((size_t) (mels * nBins), 0.0f);
        for (int m = 0; m < mels; ++m)
        {
            const auto f0 = edges[(size_t) m], f1 = edges[(size_t) m + 1], f2 = edges[(size_t) m + 2];
            const auto norm = 2.0 / (f2 - f0);
            for (int k = 0; k < nBins; ++k)
            {
                const auto f = k * sampleRate / nFft;
                const auto v = std::max (0.0, std::min ((f - f0) / (f1 - f0), (f2 - f) / (f2 - f1)));
                w[(size_t) (m * nBins + k)] = (float) (v * norm);
            }
        }
        return w;
    }
}

std::vector<float> resampleTo16k (const float* x, int64_t n, double rate)
{
    if (n <= 0 || rate <= 0.0)
        return {};
    const auto ratio = sampleRate / rate;
    const auto outN = (int64_t) std::floor ((double) n * ratio);
    std::vector<float> out ((size_t) std::max ((int64_t) 0, outN));
    if (std::abs (ratio - 1.0) < 1e-9)
    {
        // 壊れた値（NaN・Inf）は 0 に（ユーザーの float の WAV をそのまま読む経路がある。NaN は判定の表の外を読み、
        // その後の有声・無声の判断も崩していた。バグチェック 2026-10-05）
        std::transform (x, x + out.size(), out.begin(), [] (float v) { return std::isfinite (v) ? v : 0.0f; });
        return out;
    }
    // 窓付き sinc。カットオフは低い方のナイキストの 0.95 倍
    const auto cutoff = std::min (1.0, ratio) * 0.95;
    constexpr int zeros = 16;
    const auto halfWidth = zeros / cutoff;   // 入力のサンプル数
    for (int64_t i = 0; i < outN; ++i)
    {
        const auto t = (double) i / ratio;
        const auto first = (int64_t) std::ceil (t - halfWidth), last = (int64_t) std::floor (t + halfWidth);
        double acc = 0.0;
        for (auto j = std::max ((int64_t) 0, first); j <= std::min (n - 1, last); ++j)
        {
            const auto d = (double) j - t;
            const auto a = pi * d * cutoff;
            const auto s = std::abs (a) < 1e-9 ? 1.0 : std::sin (a) / a;
            const auto win = 0.5 + 0.5 * std::cos (pi * d / halfWidth);
            if (std::isfinite (x[j]))
                acc += x[j] * s * win;
        }
        out[(size_t) i] = (float) (acc * cutoff);
    }
    return out;
}

std::vector<float> logMel (const std::vector<float>& x, int& frames)
{
    static const auto filters = melFilters();
    const int n = (int) x.size();
    const int pad = nFft / 2;
    frames = 1 + n / hop;
    const int nBins = nFft / 2 + 1;
    std::vector<double> window ((size_t) nFft);
    for (int i = 0; i < nFft; ++i)
        window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * pi * i / nFft);   // 周期 Hann
    // reflect（端の点は繰り返さない）。短すぎる時は 0 で埋める
    auto at = [&] (int i) -> double
    {
        if (n == 0) return 0.0;
        if (n == 1) return x[0];
        const int period = 2 * (n - 1);
        i = ((i % period) + period) % period;
        return x[(size_t) (i < n ? i : period - i)];
    };

    std::vector<float> out ((size_t) mels * (size_t) frames);
    std::vector<Complex> buf ((size_t) nFft);
    std::vector<double> mag ((size_t) nBins);
    for (int f = 0; f < frames; ++f)
    {
        const int start = f * hop - pad;
        for (int k = 0; k < nFft; ++k)
            buf[(size_t) k] = Complex (at (start + k) * window[(size_t) k], 0.0);
        fft (buf, false);
        for (int k = 0; k < nBins; ++k)
            mag[(size_t) k] = std::abs (buf[(size_t) k]);
        for (int m = 0; m < mels; ++m)
        {
            const float* w = filters.data() + (size_t) m * (size_t) nBins;
            double s = 0.0;
            for (int k = 0; k < nBins; ++k)
                s += w[k] * mag[(size_t) k];
            out[(size_t) m * (size_t) frames + (size_t) f] = (float) std::log (std::max (s, 1e-5));
        }
    }
    return out;
}

void decodeFrame (const float* s, float& cents, float& strength)
{
    int c = 0;
    for (int i = 1; i < bins; ++i)
        if (s[i] > s[c]) c = i;
    strength = s[c];
    double num = 0.0, den = 0.0;
    for (int i = c - 4; i <= c + 4; ++i)
    {
        if (i < 0 || i >= bins) continue;   // RVC は外を 0 で埋める（重み 0）
        num += (double) s[i] * (20.0 * i + 1997.3794084376191);
        den += s[i];
    }
    cents = den > 1e-9 ? (float) (num / den) : 0.0f;
}

float voicedProbability (float p)
{
    constexpr int n = (int) (sizeof (calib) / sizeof (calib[0]));
    if (! std::isfinite (p) || p <= calib[0][0]) return calib[0][1];   // NaN は表の外を読んでいた
    if (p >= calib[n - 1][0]) return calib[n - 1][1];
    const auto* it = std::upper_bound (std::begin (calib), std::end (calib), p, [] (float v, const float (&e)[2]) { return v < e[0]; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const auto t = b[0] > a[0] ? (p - a[0]) / (b[0] - a[0]) : 0.0f;
    return a[1] + (b[1] - a[1]) * t;
}

std::vector<float> smoothPath (const std::vector<Frame>& frames)
{
    // 候補：RMVPE の音程（重み 1）と ±1200 セント（重み shiftBeta）。最後の列が無声
    constexpr int K = 3, S = K + 1;
    const auto T = frames.size();
    std::vector<float> path (T, 0.0f);
    if (T == 0)
        return path;
    constexpr float none = -1e30f;
    std::vector<std::array<float, K>> cand (T);
    std::vector<std::array<float, S>> emis (T);
    for (size_t t = 0; t < T; ++t)
    {
        const auto& f = frames[t];
        const auto V = std::clamp (voicedProbability (f.strength), 1e-6f, 1.0f - 1e-6f);
        cand[t] = { 0.0f, 0.0f, 0.0f };
        emis[t] = { none, none, none, std::log (1.0f - V) };
        if (f.cents <= 0.0f)
            continue;
        const float c[K] = { f.cents, f.cents - 1200.0f, f.cents + 1200.0f };
        const float w[K] = { 1.0f, shiftBeta, shiftBeta };
        float total = 1.0f;
        for (int k = 1; k < K; ++k)
            if (c[k] > minCandidate && c[k] < maxCandidate) total += w[k];
        for (int k = 0; k < K; ++k)
        {
            if (k > 0 && ! (c[k] > minCandidate && c[k] < maxCandidate))
                continue;
            cand[t][(size_t) k] = c[k];
            emis[t][(size_t) k] = std::log (V * std::max (w[k] / total, 1e-6f));
        }
    }

    std::vector<std::array<unsigned char, S>> back (T);
    std::array<float, S> score = emis[0];
    for (size_t t = 1; t < T; ++t)
    {
        std::array<float, S> next;
        for (int j = 0; j < S; ++j)
        {
            next[(size_t) j] = none;
            back[t][(size_t) j] = 0;
            if (emis[t][(size_t) j] <= none)
                continue;
            float best = none;
            int arg = 0;
            for (int i = 0; i < S; ++i)
            {
                if (score[(size_t) i] <= none)
                    continue;
                float tr;
                if (i == K && j == K)       tr = 0.0f;
                else if (i == K || j == K)  tr = -voicedSwitch;
                else                        tr = transition (cand[t - 1][(size_t) i], cand[t][(size_t) j]);
                const auto s = score[(size_t) i] + tr;
                if (s > best) { best = s; arg = i; }
            }
            next[(size_t) j] = best + emis[t][(size_t) j];
            back[t][(size_t) j] = (unsigned char) arg;
        }
        const auto mx = *std::max_element (next.begin(), next.end());
        for (auto& v : next)
            v = v <= none ? none : v - mx;   // 数を小さく保つ
        score = next;
    }
    int k = (int) (std::max_element (score.begin(), score.end()) - score.begin());
    for (size_t t = T; t-- > 0;)
    {
        path[t] = k < K ? cand[t][(size_t) k] : 0.0f;
        if (t > 0)
            k = back[t][(size_t) k];
    }
    return path;
}

void gateQuiet (std::vector<audio::PitchFrame>& points, const float* vocals, int64_t n, double rate, float dbBelow,
                const float* loudFrom)
{
    if (n <= 0 || points.empty())
        return;
    // 10 ms ごとのピーク
    const auto block = std::max ((int64_t) 1, (int64_t) std::llround (0.01 * rate));
    auto blockPeaks = [&] (const float* x)
    {
        std::vector<float> p ((size_t) ((n + block - 1) / block), 0.0f);
        for (int64_t i = 0; i < n; ++i)
            p[(size_t) (i / block)] = std::max (p[(size_t) (i / block)], std::abs (x[i]));
        return p;
    };
    const auto peaks = blockPeaks (vocals);
    auto sorted = loudFrom != nullptr ? blockPeaks (loudFrom) : peaks;
    const auto k = (size_t) ((double) (sorted.size() - 1) * 0.99);
    std::nth_element (sorted.begin(), sorted.begin() + (ptrdiff_t) k, sorted.end());
    const auto loud = sorted[k];
    if (loud <= 0.0f)
        return;
    const auto floor = loud * std::pow (10.0f, -dbBelow / 20.0f);
    for (auto& p : points)
    {
        if (p.confidence <= 0.0f)
            continue;
        const auto b = p.songSample / block;
        float peak = 0.0f;
        for (auto j = b - 1; j <= b + 1; ++j)
            if (j >= 0 && j < (int64_t) peaks.size())
                peak = std::max (peak, peaks[(size_t) j]);
        if (peak < floor)
        {
            p.midi = 0.0f;
            p.confidence = 0.0f;
        }
    }
}

std::vector<audio::PitchFrame> toPitchFrames (const std::vector<Frame>& frames, double rate, int64_t offsetSamples)
{
    const auto path = smoothPath (frames);
    std::vector<audio::PitchFrame> out (frames.size());
    for (size_t t = 0; t < frames.size(); ++t)
    {
        auto& p = out[t];
        p.songSample = offsetSamples + (int64_t) std::llround ((double) t * hop / sampleRate * rate);
        if (path[t] > 0.0f)
        {
            p.midi = centsToMidi (path[t]);
            p.confidence = std::max (0.5f, voicedProbability (frames[t].strength));   // 声あり（画面・判定は 0.5 以上）
            p.levelDb = 0.0f;
        }
    }
    return out;
}
} // namespace vb::analysis::rmvpe
