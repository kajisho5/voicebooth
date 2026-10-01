#pragma once

#include <complex>
#include <vector>
#include <cmath>

/*  解析用の小さな FFT（基数 2、その場で計算）。オーディオスレッドでは使わない（オフライン解析用）
    juce_dsp を足さずに済ませるための最小限 */

namespace vb::analysis
{
using Complex = std::complex<double>;

inline size_t nextPow2 (size_t n)
{
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

/** a.size() は 2 の累乗。inverse なら 1/N も掛ける */
inline void fft (std::vector<Complex>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * 3.14159265358979323846 / (double) len * (inverse ? 1.0 : -1.0);
        const Complex wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            Complex w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse)
        for (auto& x : a) x /= (double) n;
}
} // namespace vb::analysis
