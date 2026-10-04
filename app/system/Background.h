#pragma once

#include <juce_core/juce_core.h>
#include <functional>

/*  バックグラウンドの作業（解析・書き出し・分離の準備など）を動かす（#26。2026-10-04）
    前は作業ごとに普通の優先度のスレッドを立てていて、上限がなかった（テイクごとに 1 本。曲を開いた直後は解析が何本も重なり、
    録音・再生とCPU を取り合う）。ここでは低い優先度のスレッドで動かし、同時に動くのは limit() 本まで。超えた分は空くまで待つ */

namespace vb::background
{
/** 同時に動かせる数（論理コア − 1。2〜4 本） */
int limit();

/** 低い優先度で job を動かす。limit() 本が動いていれば、どれかが終わるまで待ってから始める */
void run (std::function<void()> job);

/** いま動いている数（テスト用） */
int running();
} // namespace vb::background
