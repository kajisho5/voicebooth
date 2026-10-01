#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

/*  テンポの手入力（DESIGN 7.5.1）。画面にはまだつながない（B4b で結線）

    - タップテンポ：4 回以上叩くと、直近の間隔の平均から BPM を出す。2 秒あくと数え直し
    - ×2 / ÷2：自動推定でよくある倍・半分の間違いを 1 回で直す（範囲 minBpm〜maxBpm に収める）
    - 値は小数 2 桁に丸めて扱う（入力欄と同じ精度） */

namespace vb::song
{
constexpr double minBpm = 30.0, maxBpm = 300.0;

inline double roundBpm (double bpm) { return std::round (bpm * 100.0) / 100.0; }

inline double clampBpm (double bpm) { return juce::jlimit (minBpm, maxBpm, bpm); }

/** ×2。範囲を超えるなら元のまま */
inline double doubledBpm (double bpm) { return bpm * 2.0 <= maxBpm ? roundBpm (bpm * 2.0) : bpm; }

/** ÷2。範囲を下回るなら元のまま */
inline double halvedBpm (double bpm) { return bpm / 2.0 >= minBpm ? roundBpm (bpm / 2.0) : bpm; }

class TapTempo
{
public:
    static constexpr int minTaps = 4;           // これ未満は BPM を出さない
    static constexpr int maxIntervals = 8;      // 直近の間隔だけ使う（叩き始めの迷いを捨てる）
    static constexpr double resetSeconds = 2.0;

    /** 叩いた時刻（秒、単調増加）。BPM が出せれば返す。出せなければ 0 */
    double tap (double timeSeconds)
    {
        if (! taps.empty() && (timeSeconds - taps.back() > resetSeconds || timeSeconds <= taps.back()))
            taps.clear();

        taps.push_back (timeSeconds);
        if ((int) taps.size() > maxIntervals + 1)
            taps.erase (taps.begin());

        return bpm();
    }

    double bpm() const
    {
        if ((int) taps.size() < minTaps)
            return 0.0;
        const auto mean = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        return mean > 0.0 ? roundBpm (clampBpm (60.0 / mean)) : 0.0;
    }

    /** 間隔のばらつき（ミリ秒の標準偏差）。安定して叩けているかの目安 */
    double jitterMs() const
    {
        if (taps.size() < 3)
            return 0.0;
        const auto n = (double) (taps.size() - 1);
        const auto mean = (taps.back() - taps.front()) / n;
        double sum = 0.0;
        for (size_t i = 1; i < taps.size(); ++i)
            sum += std::pow ((taps[i] - taps[i - 1]) - mean, 2.0);
        return std::sqrt (sum / n) * 1000.0;
    }

    int count() const { return (int) taps.size(); }
    void reset() { taps.clear(); }

private:
    std::vector<double> taps;
};
} // namespace vb::song
