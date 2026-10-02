#include "Separation.h"
#include "Fft.h"

namespace vb::analysis::separation
{
namespace
{
    const std::vector<double>& window()
    {
        static const std::vector<double> w = []
        {
            std::vector<double> v ((size_t) nFft);
            for (int i = 0; i < nFft; ++i)
                v[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / nFft);   // 周期 Hann
            return v;
        }();
        return w;
    }

    /** numpy / torch の reflect（端の 1 サンプルは繰り返さない） */
    inline int reflectIndex (int i, int n)
    {
        if (n <= 1) return 0;
        const int period = 2 * (n - 1);
        i %= period;
        if (i < 0) i += period;
        return i < n ? i : period - i;
    }

    inline size_t at (int ch, int bin, int frame, int frames) { return (((size_t) ch * bins + (size_t) bin) * (size_t) frames + (size_t) frame) * 2; }
}

std::vector<float> stft (const float* const* x, int n, int& framesOut)
{
    const int pad = nFft / 2;
    const int frames = 1 + n / hop;   // = 1 + (n + 2·pad − nFft) / hop
    framesOut = frames;
    std::vector<float> spec ((size_t) 2 * bins * (size_t) frames * 2);
    const auto& w = window();
    std::vector<Complex> buf ((size_t) nFft);
    for (int ch = 0; ch < 2; ++ch)
        for (int f = 0; f < frames; ++f)
        {
            for (int k = 0; k < nFft; ++k)
                buf[(size_t) k] = Complex (x[ch][reflectIndex (f * hop + k - pad, n)] * w[(size_t) k], 0.0);
            fft (buf, false);
            for (int b = 0; b < bins; ++b)
            {
                const auto i = at (ch, b, f, frames);
                spec[i] = (float) buf[(size_t) b].real();
                spec[i + 1] = (float) buf[(size_t) b].imag();
            }
        }
    return spec;
}

void istft (const std::vector<float>& spec, int frames, int length, float* const* out)
{
    const int pad = nFft / 2;
    const int total = nFft + hop * (frames - 1);
    const auto& w = window();
    std::vector<double> acc ((size_t) total), wsum ((size_t) total);
    std::vector<Complex> buf ((size_t) nFft);
    for (int f = 0; f < frames; ++f)
        for (int k = 0; k < nFft; ++k)
            wsum[(size_t) (f * hop + k)] += w[(size_t) k] * w[(size_t) k];

    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (acc.begin(), acc.end(), 0.0);
        for (int f = 0; f < frames; ++f)
        {
            // 実数信号の逆変換：bin 1..1024 の共役を後ろ半分に（DC とナイキストの虚部は捨てる。irfft と同じ）
            for (int b = 0; b < bins; ++b)
            {
                const auto i = at (ch, b, f, frames);
                buf[(size_t) b] = Complex (spec[i], (b == 0 || b == bins - 1) ? 0.0 : spec[i + 1]);
            }
            for (int b = 1; b < nFft / 2; ++b)
                buf[(size_t) (nFft - b)] = std::conj (buf[(size_t) b]);
            fft (buf, true);
            for (int k = 0; k < nFft; ++k)
                acc[(size_t) (f * hop + k)] += buf[(size_t) k].real() * w[(size_t) k];
        }
        for (int i = 0; i < length; ++i)
        {
            const auto j = (size_t) (i + pad);
            const auto ws = j < wsum.size() ? wsum[j] : 0.0;
            const auto a = j < acc.size() ? acc[j] : 0.0;
            out[ch][i] = (float) (ws > 1.0e-11 ? a / ws : a);
        }
    }
}

int chunkCount (int samples, int overlap)
{
    const int step = chunkSamples / juce::jmax (1, overlap);
    const int border = chunkSamples - step;
    const int length = samples > 2 * border && border > 0 ? samples + 2 * border : samples;
    return (length + step - 1) / step;
}

bool demix (const juce::AudioBuffer<float>& mixIn, const Model& model, int overlap, juce::AudioBuffer<float>& vocals,
            const std::function<bool (float)>& progress)
{
    const int chunk = chunkSamples, fade = chunk / 10;
    const int step = chunk / juce::jmax (1, overlap);
    const int border = chunk - step;
    const int n0 = mixIn.getNumSamples();
    if (n0 <= 0 || mixIn.getNumChannels() < 1)
        return false;

    // 両端を reflect で延ばす（曲が短ければ延ばさない）
    const bool padded = n0 > 2 * border && border > 0;
    const int n = padded ? n0 + 2 * border : n0;
    juce::AudioBuffer<float> mix (2, n);
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* src = mixIn.getReadPointer (juce::jmin (ch, mixIn.getNumChannels() - 1));
        auto* dst = mix.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            dst[i] = src[padded ? reflectIndex (i - border, n0) : i];
    }

    std::vector<float> win ((size_t) chunk, 1.0f);
    for (int i = 0; i < fade; ++i)
    {
        const auto v = (float) i / (float) (fade - 1);   // np.linspace (0, 1, fade)
        win[(size_t) i] = v;
        win[(size_t) (chunk - 1 - i)] = v;
    }

    juce::AudioBuffer<float> result (2, n), part (2, chunk), outPart (2, chunk);
    result.clear();
    std::vector<float> counter ((size_t) n, 0.0f);
    std::vector<float> est;
    const int total = (n + step - 1) / step;
    int index = 0;
    for (int i = 0; i < n; i += step, ++index)
    {
        const int len = juce::jmin (chunk, n - i);
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto* src = mix.getReadPointer (ch) + i;
            auto* dst = part.getWritePointer (ch);
            for (int k = 0; k < chunk; ++k)
                dst[k] = k < len ? src[k] : (len > chunk / 2 ? src[reflectIndex (k, len)] : 0.0f);
        }

        int frames = 0;
        const auto spec = stft (part.getArrayOfReadPointers(), chunk, frames);
        if (! model (spec, frames, est))
            return false;
        istft (est, frames, chunk, outPart.getArrayOfWritePointers());

        const bool first = i == 0, last = i + step >= n;
        for (int k = 0; k < len; ++k)
        {
            float wk = win[(size_t) k];
            if (first && k < fade) wk = 1.0f;                  // 最初のチャンクはフェードインしない
            else if (last && ! first && k >= chunk - fade) wk = 1.0f;   // 最後のチャンクはフェードアウトしない
            for (int ch = 0; ch < 2; ++ch)
                result.getWritePointer (ch)[i + k] += outPart.getSample (ch, k) * wk;
            counter[(size_t) (i + k)] += wk;
        }
        if (progress && ! progress ((float) (index + 1) / (float) total))
            return false;
    }

    vocals.setSize (2, n0);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* dst = vocals.getWritePointer (ch);
        const auto* r = result.getReadPointer (ch);
        for (int i = 0; i < n0; ++i)
        {
            const auto j = (size_t) (padded ? i + border : i);
            const auto c = counter[j];
            const auto v = c > 0.0f ? r[j] / c : 0.0f;
            dst[i] = std::isfinite (v) ? v : 0.0f;
        }
    }
    return true;
}
} // namespace vb::analysis::separation
