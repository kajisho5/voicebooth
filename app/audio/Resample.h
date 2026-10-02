#pragma once

#include "SongLoader.h"

/*  曲（伴奏）を別のサンプリングレートに変える（録音形式。DESIGN 6.5 / 13、2026-10-01）
    録音の SR を曲と違う値（例：曲 48 kHz・録音 96 kHz）にした時だけ使う。時間軸を録音の SR にそろえ、
    テイク・採用区間・書き出しはすべて録音の SR のサンプルで数える（頭の位置は時間で合う）。

    - 窓付き sinc（JUCE の WindowedSincInterpolator）。下げる時は先に低域通過（8 次 Butterworth を前後に掛けて位相を 0 に。
      折り返し防止。位相を回さないので時間がずれない）
    - 補間の遅れ（algorithmicLatency）を差し引いて、同じ時刻が同じ時刻に来るようにする。大きさも 1 倍に補正する
    - 長さは round(元の長さ × 新 SR / 元 SR)
    - 変換した伴奏は再生用。書き出しには入らない（ボーカルだけ）

    重い（5 分のステレオを 48 → 192 kHz で数秒）のでバックグラウンドで呼ぶ */

namespace vb::audio
{
/** 変換後の長さ */
juce::int64 resampledLength (juce::int64 length, double fromRate, double toRate) noexcept;

/** 変換した新しい曲を返す。progress（0..1）が false を返したら中止して nullptr。同じ SR ならそのままの写し */
std::shared_ptr<SongAudio> resampleSong (const SongAudio&, double toRate, const std::function<bool (float)>& progress = {});

/** 録音で選べる SR（機器が対応していれば）。DESIGN 6.5 */
const juce::Array<double>& recordingRates();
} // namespace vb::audio
