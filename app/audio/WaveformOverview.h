#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/*  波形の概形（表示用のピーク表。DESIGN 4.5 / B1）
    曲を一度だけ頭から読み、一定サンプルごとの最小・最大を持つ。
    画面はここから引くだけ（描画のたびにファイルを読まない）。

    - 全チャンネルをまとめた値（どのチャンネルの山も見落とさない）
    - 最小・最大に加えて RMS も持つ。市販曲は音圧が高くピークがほぼ常に 0 dBFS に張り付くため、
      ピークだけでは A メロ / サビの区別がつかない（実曲で確認）。RMS で曲の起伏を見せる
    - 2 段（細かい / 粗い）。曲全体を見るときは粗い方を使う（長尺曲の LOD。DESIGN 13）
    - 作り終えたら変更しない。スレッド間は const のまま共有してよい */

namespace vb::audio
{
using int64 = juce::int64;

class WaveformOverview
{
public:
    static constexpr int samplesPerBin = 256;          // 細かい段（48 kHz で約 5.3 ms）
    static constexpr int binsPerCoarseBin = 16;        // 粗い段 = 4096 サンプル

    struct Peak
    {
        float min = 0.0f, max = 0.0f;
        float magnitude() const { return juce::jmax (-min, max); }
    };

    /** 曲の長さ（サンプル）を先に決める。足りない分は無音扱い */
    explicit WaveformOverview (int64 lengthSamples = 0);

    /** 頭から順に流し込む。channels[c][i]（numChannels 本、各 numSamples） */
    void append (const float* const* channels, int numChannels, int numSamples);

    int64 getLengthSamples() const   { return length; }
    int64 getSamplesAppended() const { return appended; }
    bool  isComplete() const         { return appended >= length; }

    /** [start, end) を含むビンの最小・最大。範囲外・空なら 0
        ビン単位で丸めるため、端のビンぶん（最大 samplesPerBin - 1）広めに拾う */
    Peak getPeak (int64 start, int64 end) const;

    /** [start, end) を含むビンの RMS（全チャンネルの平均パワー）。範囲外・空なら 0 */
    float getRms (int64 start, int64 end) const;

    /** 曲全体の最大振幅（0 dBFS = 1.0） */
    float getOverallMagnitude() const { return overall; }

private:
    void finishBin();

    int64 length = 0, appended = 0;
    int64 binCount (size_t fineIndex) const;

    std::vector<Peak> fine, coarse;
    std::vector<float> fineSq;      // ビン内の Σ(各サンプルのチャンネル平均パワー)
    std::vector<double> coarseSq;
    Peak current;
    double currentSq = 0.0;
    int inCurrent = 0;
    float overall = 0.0f;
};
} // namespace vb::audio
