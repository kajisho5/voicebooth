#pragma once

#include <juce_core/juce_core.h>

/*  このパソコンが動作環境（DESIGN 11.6.1）を満たすか。設定画面の「このパソコン」に出す
    数値は暫定（B8 / B16 で実測して確定）。OS の下限はビルドで決まる（Win 10 1607 / macOS 11）ので、ここでは見ない */

namespace vb::system
{
struct Requirements
{
    // メモリは「8 GB の機械」が OS の予約分で 7.8 GB などと出るので、少し下に線を引く
    juce::int64 minMemoryMB = 7500, recMemoryMB = 15000;
    int minCores = 4, recCores = 6;                            // 物理コア
    juce::int64 minFreeDiskMB = 2 * 1024, recFreeDiskMB = 10 * 1024;
};

struct Info
{
    juce::String osName;
    juce::String cpuModel;       // 短くした名前（"Intel Core i5-8250U" など）
    juce::String architecture;   // "arm64" / "x86_64"
    int physicalCores = 0;
    juce::int64 memoryMB = 0;
    juce::int64 freeDiskMB = -1; // 分からなければ負
};

enum class Level { ok, belowRecommended, belowMinimum };

enum class Item { cores, memory, disk };

struct Verdict
{
    Level level = Level::ok;
    juce::Array<Item> belowMinimum, belowRecommended;
};

/** いまのパソコンを調べる（速い。メッセージスレッドから呼んでよい）。disk は空き容量を見る場所 */
Info gather (const juce::File& disk);

Verdict evaluate (const Info&, const Requirements& = {});

/** "Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz" → "Intel Core i5-8250U" */
juce::String shortCpuName (juce::String);
} // namespace vb::system
