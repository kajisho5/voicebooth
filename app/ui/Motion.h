#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

/*  動き（DESIGN 4.10 / 4.10.1）の計算だけ。画面にも JUCE にも依存しない（テストから使う）
    数値は部品ラボの「C こだわり」に合わせてある。

    守ること（DESIGN 4.10）：
    - 値の真実は常に数値。ばね・吸い付きは「見た目」と「操作の手応え」だけで、保存する値を丸めない
      （吸い付きで決まった値は既定値そのもの）
    - reduced（OS の「視差効果を減らす」等）なら最終状態だけを返す */

namespace vb::motion
{
//==============================================================================
/** ばね。k：硬さ、c：減衰（小さいと少し行き過ぎて戻る）。4 ms 刻みで積分するので dt に左右されにくい */
struct Spring
{
    float x = 0.0f, v = 0.0f;

    Spring() = default;
    explicit Spring (float start) : x (start) {}

    float step (float target, float dt, float k, float c, bool reduced = false)
    {
        if (reduced || dt <= 0.0f)
        {
            if (reduced) snap (target);
            return x;
        }

        const int n = std::max (1, (int) std::ceil (dt / 0.004f));
        const float h = dt / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float a = k * (target - x) - c * v;
            v += a * h;
            x += v * h;
        }
        return x;
    }

    void snap (float target) { x = target; v = 0.0f; }

    /** 止まったか（位置と速さがほぼ 0） */
    bool atRest (float target, float posEps = 0.002f, float velEps = 0.02f) const
    {
        return std::abs (target - x) < posEps && std::abs (v) < velEps;
    }
};

/** 目標へ指数で近づく（rate が大きいほど速い。1/rate 秒で約 63%） */
inline float approach (float x, float target, float rate, float dt, bool reduced = false)
{
    if (reduced) return target;
    return x + (target - x) * (1.0f - std::exp (-rate * dt));
}

//==============================================================================
/** 吸い付き（デッドゾーン）。つまみの「生の位置」raw → 値。
    centre の前後 zone だけ動かしても値は centre のまま（カチッと止まる）。
    外へ出たら zone の分だけずらして続けるので、値は飛ばず、どの値にも届く。 */
struct Detent
{
    double centre = 0.0, zone = 0.0;

    double toValue (double raw) const
    {
        const double d = raw - centre;
        if (std::abs (d) <= zone) return centre;
        return centre + (d > 0.0 ? d - zone : d + zone);
    }

    /** 値 → 生の位置（掴み直した時の起点）。centre ちょうどなら centre */
    double toRaw (double value) const
    {
        const double d = value - centre;
        if (! (d < 0.0) && ! (d > 0.0)) return centre;
        return centre + (d > 0.0 ? d + zone : d - zone);
    }
};

//==============================================================================
/** フェーダー（0..1、0.75 = 0 dB）。DESIGN 4.10 */
namespace fader
{
    constexpr double unity = 0.75;          // 0 dB
    constexpr double detentPixels = 5.0;    // 0 dB で止まる長さ（つまみの移動量）
    constexpr double fineScale = 0.25;      // Shift：微調整

    /** 縦ドラッグ dy（下が正）→ 生の位置の変化。travel はつまみが動ける長さ（px） */
    inline double dragDelta (double dyPixels, double travelPixels, bool fine)
    {
        return -dyPixels / std::max (1.0, travelPixels) * (fine ? fineScale : 1.0);
    }

    inline Detent detent (double travelPixels, double centre = unity)
    {
        return { centre, detentPixels / std::max (1.0, travelPixels) };
    }

    /** ドラッグ中の値。生の位置を 0..1 の外へ出さないように抑えてから吸い付きを通す */
    inline double valueFromRaw (double& raw, const Detent& d)
    {
        const double lo = d.toRaw (0.0), hi = d.toRaw (1.0);
        raw = std::clamp (raw, lo, hi);
        if (raw <= lo) return 0.0;   // 端はちょうど（浮動小数の誤差を残さない）
        if (raw >= hi) return 1.0;
        return std::clamp (d.toValue (raw), 0.0, 1.0);
    }

    // ばね（ダブルクリックで 0 dB に戻る時の見た目）
    constexpr float springK = 260.0f, springC = 22.0f;
}

//==============================================================================
/** エンコーダー（テンポ / キー）。DESIGN 4.10：速く回すと大きく、ゆっくりだと細かく */
namespace encoder
{
    constexpr double detentFraction = 0.024;   // 既定値で止まる幅（範囲に対する割合。ゆっくり回して約 8 px）

    /** 縦ドラッグ dy（上が正）を、その間の時間 ms と値の範囲 range から値の変化へ */
    inline double dragDelta (double dyPixels, double ms, double range, bool fine)
    {
        const double speed = std::abs (dyPixels) / std::max (1.0, ms);           // px / ms
        const double base = (fine ? 0.0012 : 0.003) * range;                      // ゆっくりの時：1 px あたり
        return dyPixels * base * (1.0 + std::min (3.0, speed * 2.5));
    }

    inline Detent detent (double defaultValue, double range)
    {
        return { defaultValue, detentFraction * range };
    }

    // ばね（指標・LED の輪・数値が追う）
    constexpr float springK = 420.0f, springC = 34.0f;
}

//==============================================================================
/** キー（ボタン）：押すとばねで沈む。LED は消える時に余韻を残す。REC 中は LED がゆっくり呼吸 */
namespace key
{
    constexpr float springK = 900.0f, springC = 18.0f;
    constexpr float pressDepthPx = 1.5f;     // 沈む深さ
    constexpr float pressScale = 0.03f;      // 沈んだ時の縮み
    constexpr float flashSeconds = 0.12f;    // ショートカットで押された時に沈んでいる長さ

    /** LED の明るさ（0..1）を 1 フレーム進める。点く時はすぐ（約 40 ms）、消える時は約 0.35 秒で余韻 */
    inline float ledLevel (float level, bool lit, float dt, bool reduced)
    {
        if (reduced) return lit ? 1.0f : 0.0f;
        const float next = lit ? approach (level, 1.0f, 60.0f, dt) : approach (level, 0.0f, 11.0f, dt);
        if (lit && next > 0.995f) return 1.0f;
        if (! lit && next < 0.01f) return 0.0f;
        return next;
    }

    /** 呼吸（REC 中）。1 = いちばん明るい、2.2 秒で一巡、なめらかに（0.55..1） */
    constexpr double breathePeriod = 2.2;
    inline float breathe (double seconds)
    {
        const double phase = std::fmod (seconds, breathePeriod) / breathePeriod;   // 0..1
        const double s = 0.5 + 0.5 * std::cos (phase * 2.0 * 3.14159265358979323846);  // 1 → 0 → 1
        return (float) (0.55 + 0.45 * s);
    }
}

/** 切り替え（セグメント）：キーキャップごとばねで滑り、少し行き過ぎて戻る。止まってから LED が点く */
namespace segment
{
    constexpr float springK = 220.0f, springC = 15.0f;

    inline bool landed (const Spring& s, float target) { return s.atRest (target, 0.01f, 0.05f); }
}

/** 新しいバージョンの知らせ：ばねで滑り込み、LED が 2 回点滅して点灯のまま */
namespace notice
{
    constexpr float springK = 260.0f, springC = 17.0f;
    constexpr double blinkSeconds = 0.8;   // 2 回点滅（0.2 秒ごとに入れ替わる）

    /** LED の明るさ。0..0.8 秒は 0.2 秒ごとに点く / 消える（2 回）、その後は点いたまま */
    inline float led (double secondsSinceShown)
    {
        if (secondsSinceShown < 0.0 || secondsSinceShown >= blinkSeconds) return 1.0f;
        return ((int) (secondsSinceShown / 0.2) % 2 == 0) ? 1.0f : 0.15f;
    }
}

/** 失敗した時の小さな揺れ（0.36 秒）。横のずれ（px） */
namespace shake
{
    constexpr double seconds = 0.36;

    inline float offset (double t)
    {
        if (t <= 0.0 || t >= seconds) return 0.0f;
        // 20%,80%：-2px / 40%,60%：+3px（部品ラボの keyframes）を線でつなぐ
        const double u = t / seconds;
        const double keys[] = { 0.0, 0.2, 0.4, 0.6, 0.8, 1.0 };
        const double vals[] = { 0.0, -2.0, 3.0, 3.0, -2.0, 0.0 };
        for (int i = 0; i < 5; ++i)
            if (u <= keys[i + 1])
                return (float) (vals[i] + (vals[i + 1] - vals[i]) * (u - keys[i]) / (keys[i + 1] - keys[i]));
        return 0.0f;
    }
}

//==============================================================================
/** 再生ヘッド（DESIGN 4.10.1 PH）：右から 7 割の位置を越えたら、画面がなめらかに先回りしてスクロールする。
    ページ送りはしない。戻った（ループ・シーク）時と、画面の外へ出た時だけ飛ぶ。
    戻り値は新しい表示の頭（サンプル）。len は表示の長さ、songLength は曲の長さ */
namespace playhead
{
    constexpr double aheadAt = 0.70;   // 画面のこの位置から先回り
    constexpr float followRate = 7.0f;

    inline std::int64_t follow (std::int64_t viewStart, std::int64_t len, std::int64_t head,
                                std::int64_t songLength, double dt, bool reduced)
    {
        const std::int64_t maxStart = std::max<std::int64_t> (0, songLength - len);
        const auto clampStart = [maxStart] (std::int64_t s) { return std::clamp<std::int64_t> (s, 0, maxStart); };

        const std::int64_t target = clampStart (head - (std::int64_t) std::llround ((double) len * aheadAt));

        // 画面の外（戻った・飛んだ）：その場で合わせる
        if (head < viewStart || head > viewStart + len)
            return target;

        // まだ 7 割に届いていない：動かさない（戻る方向には動かさない）
        if (target <= viewStart)
            return viewStart;

        if (reduced)
            return target;

        const double next = (double) viewStart + (double) (target - viewStart) * (1.0 - std::exp (-(double) followRate * dt));
        return clampStart (std::max (viewStart + 1, (std::int64_t) std::llround (next)));
    }
}
} // namespace vb::motion
