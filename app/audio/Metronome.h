#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

/*  クリック（メトロノーム）とカウントインの音（2026-10-02）。デバイスに依存しない中身。
    PlaybackCore が出力 1 サンプルごとに next() を呼ぶ（オーディオスレッド）。ほかはどのスレッドからでも

    - 拍は曲のテンポ（BPM・拍子・1 小節目の位置）から。位置は曲のサンプルで、丸め方は song::beatSample と同じ
      （画面の目盛り・BAR.BEAT と同じ所で鳴る）。自動推定の拍のずれ（TempoInfo::beats）は使わない：目盛りが一定テンポなので
    - 鳴らすのは出力の側：「このサンプルで聞こえている曲の位置」が拍に届いた出力のサンプルで音を立てる。
      曲に混ぜてから Rubber Band を通すと、練習のキーで高さが変わり、ストレッチでにじむ。出力で作れば
      練習のテンポでは拍の間だけが伸び縮みし、音はいつも同じ短いクリックのまま
    - 音はその場で作る（サンプルを持たない・確保しない）。頭が立った正弦波を約 40 ms で消す。1 拍目は高く大きく
    - 位置が戻った・飛んだ（シーク・ループ・再生の頭）時は次の拍を探し直す。ちょうど拍の上なら、その場で鳴る
    - 鳴らさない間（クリック Off）も拍は数える（後から On にした時に、過ぎた拍をまとめて鳴らさない）
    - 出力（耳）だけ。録音（入力の素の声）・書き出し（テイクから作る）には入らない */

namespace vb::audio
{
class Metronome
{
public:
    /** 拍の並び（曲のサンプル） */
    struct Grid
    {
        double samplesPerBeat = 0.0;     // 0 = テンポが分からない（鳴らさない）
        juce::int64 downbeat = 0;        // 1 小節目の頭
        int beatsPerBar = 4;             // 6/8 は 2（付点四分で数える。song::TimeSignature::beatsPerBar）

        bool valid() const { return samplesPerBeat > 0.0 && beatsPerBar > 0; }
        /** beat 番目の拍の位置（0 = 1 小節目の 1 拍目。負も可）。song::beatSample と同じ丸め */
        juce::int64 beatSample (juce::int64 beat) const;
        /** pos 以降（pos を含む）で最初の拍の番号 */
        juce::int64 firstBeatFrom (double pos) const;
        /** 小節の 1 拍目か（弱起の負の拍も） */
        bool isDownbeat (juce::int64 beat) const;
    };

    /** 拍の並びを変える（テンポ・拍子・1 小節目を直した時）。次のサンプルから探し直す */
    void setGrid (Grid);
    Grid getGrid() const;

    /** 音量（直線の倍率。PlaybackCore::faderToGain の値）。次に鳴る音から */
    void setLevel (float linearGain) { level = juce::jmax (0.0f, linearGain); }

    /** デバイスが始まる時（オーディオスレッドは止まっている）。鳴りかけの音も消す */
    void prepare (double outputSampleRate);

    /** オーディオスレッド：次のサンプルで拍を探し直す（再生の頭・位置を飛ばした時） */
    void resync() noexcept { needResync = true; }

    /** オーディオスレッド：出力 1 サンプル分。songPos はこのサンプルで聞こえる曲の位置（カウントイン中は曲より前で負も可）。
        audible = 拍に来たら鳴らす（false でも拍は数える）。戻り値は足す値（音量込み・両耳に同じ） */
    float next (double songPos, bool audible) noexcept;

    /** オーディオスレッド：曲が止まっている・終わった後。鳴りかけの音の残りだけ */
    float tail() noexcept { return voice(); }

    /** 1 拍目と、それ以外の音（高さ・大きさ）。1 拍目は 0 dB で頭が -6 dBFS */
    static constexpr float accentHz = 1600.0f, beatHz = 1100.0f;
    static constexpr float accentPeak = 0.5f, beatPeak = 0.32f;
    static constexpr double decaySeconds = 0.008;   // 約 40 ms（時定数の 5 倍）で消える
    static constexpr double lengthSeconds = 0.04;

private:
    float voice() noexcept;

    // 拍の並び（UI から。フィールドごとの atomic。変えた瞬間に混ざっても 1 つの拍がずれるだけ）
    std::atomic<double> spb { 0.0 };
    std::atomic<juce::int64> downbeat { 0 };
    std::atomic<int> perBar { 4 };
    std::atomic<int> gridSerial { 0 };
    std::atomic<float> level { 1.0f };

    // オーディオスレッドだけが触る
    double rate = 48000.0;
    bool needResync = true;
    int seenSerial = -1;
    double lastPos = 0.0;
    juce::int64 nextBeat = 0, nextBeatPos = 0;
    int left = 0;                   // 鳴っている音の残り（出力のサンプル）
    double phase = 0.0, phaseStep = 0.0;
    float amp = 0.0f, env = 0.0f, envMul = 1.0f;
};
} // namespace vb::audio
