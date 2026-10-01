#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

/*  通し録音（DESIGN 6 / Phase B5）。入力 1 ch（モニターより前の素の声）を 24bit モノラル WAV に書く
    オーディオスレッドは ThreadedWriter の FIFO に入れるだけ。ファイルへの書き込みは裏のスレッド（DESIGN 17）。

    位置：テイクの 1 サンプル目が曲のどこか（startSample）を、録音を始めたブロックの再生位置から決める。
    入力の i サンプル目は、同じコールバックで鳴らした曲の i サンプル目に当たる（デバイスの遅れの補正は B6）。
    曲が止まった・終わったら、そこで録音を閉じる（曲の外は録らない）。

    使い方（メッセージスレッド）：begin() → 再生 → … → finish()。曲の終わりで止まったら hasEnded() が true */

namespace vb::audio
{
class TakeRecorder
{
public:
    TakeRecorder();
    ~TakeRecorder();

    struct Result
    {
        juce::File file;
        juce::int64 startSample = 0;     // 曲頭基準
        juce::int64 length = 0;          // 書いたサンプル数（0 なら何も録れていない）
        float peak = 0.0f;               // 最大振幅（0..1 以上）
        bool clipped = false;            // -0.1 dBFS 以上が来た（DESIGN 13：テイクを赤く）
        bool dropped = false;            // 書き込みが追いつかず落ちた（壊れたテイク。採用しない）
    };

    /** ファイルを作って待機する。録音は次に再生中のブロックが来た時から。失敗なら理由（空なら成功）。
        同じ名前のファイルがあれば録らない（録った声を上書きしない） */
    juce::String begin (const juce::File& file, double sampleRate);

    /** 録音を閉じて結果を返す（ファイルは書ききってから返る）。begin していなければ空の結果 */
    Result finish();

    bool isActive() const { return active.load(); }
    /** 録音が始まった後に曲が止まった（終わりまで行った・止められた）。finish() を呼ぶ合図 */
    bool hasEnded() const { return ended.load(); }
    /** いま書けたサンプル数（表示用） */
    juce::int64 getRecordedSamples() const { return recorded.load(); }

    /** オーディオスレッド。input は 1 ch。rendered は同じコールバックで再生側が鳴らした範囲 */
    void process (const float* input, int numSamples, juce::int64 songStart, int songPlayed, bool wrapped) noexcept;

    /** -0.1 dBFS（InputMeter と同じ線） */
    static constexpr float clipLevel = 0.98855309f;

private:
    juce::TimeSliceThread writerThread { "VoiceBooth take writer" };
    juce::SpinLock lock;                                             // writer の差し替えだけ守る
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer; // lock で保護
    juce::File file;

    std::atomic<bool> active { false }, started { false }, ended { false }, dropped { false };
    std::atomic<juce::int64> startSample { 0 }, recorded { 0 };
    std::atomic<float> peak { 0.0f };
};
} // namespace vb::audio
