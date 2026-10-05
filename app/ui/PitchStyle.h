#pragma once

#include "analysis/TakeStats.h"
#include <vector>

/*  自分の音程の線の見た目（#28）：合う・ずれ・大きくずれを、色だけでなく線の形でも分ける（色が見分けにくい人・明るい現場でも分かる）
      合う       … 実線
      ずれ       … 長い破線
      大きくずれ … 短い破線（少し太く）
    今の音の点には、ずれていれば直す向き（上げる ▲ / 下げる ▼）の印を添える */

namespace vb::pitchstyle
{
enum class Level { ok, near, far };

/** cents：お手本からのずれ（自分 − お手本。正 = 高い）。tolerance：合っているとみなす幅（設定の許容 cent） */
inline Level levelFor (float cents, float toleranceCents)
{
    const auto a = std::abs (cents);
    if (a <= toleranceCents)                                   return Level::ok;
    if (a <= analysis::pitchWarnLimitCents (toleranceCents))  return Level::near;
    return Level::far;
}

/** 破線の長さ（線の太さの倍ではなく px。空 = 実線） */
inline std::vector<float> dashes (Level l)
{
    switch (l)
    {
        case Level::ok:   break;
        case Level::near: return { 7.0f, 4.0f };
        case Level::far:  return { 2.5f, 3.5f };
    }
    return {};
}

/** 線の太さ（px） */
inline float width (Level l) { return l == Level::far ? 3.2f : 2.6f; }

/** 形に使う段階：同じ段階が minRun 点より短く続く所（ビブラートで一瞬だけ外れる・一瞬だけ合う）は、直前の段階に合わせる
    （細切れの破線は線が途切れて見えて読みにくい。色は 1 点ずつのまま）。先頭が短ければ次の段階に合わせる */
inline std::vector<Level> shapeLevels (const std::vector<Level>& raw, int minRun)
{
    std::vector<Level> out (raw);
    const auto n = (int) raw.size();
    std::vector<std::pair<int, int>> runs;   // [始め, 終わり)
    for (int i = 0; i < n;)
    {
        int j = i + 1;
        while (j < n && raw[(size_t) j] == raw[(size_t) i])
            ++j;
        runs.emplace_back (i, j);
        i = j;
    }
    if (runs.size() < 2)
        return out;
    for (size_t r = 0; r < runs.size(); ++r)
    {
        const auto [a, b] = runs[r];
        if (b - a >= minRun)
            continue;
        const auto with = r > 0 ? out[(size_t) a - 1] : raw[(size_t) runs[1].first];
        for (int i = a; i < b; ++i)
            out[(size_t) i] = with;
    }
    return out;
}

/** 直す向き：+1 = 上げる（低い）、-1 = 下げる（高い）、0 = 合っている（印を出さない） */
inline int correction (float cents, float toleranceCents)
{
    if (levelFor (cents, toleranceCents) == Level::ok)
        return 0;
    return cents < 0.0f ? 1 : -1;
}
} // namespace vb::pitchstyle
