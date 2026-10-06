#pragma once

#include "Theme.h"

namespace vb
{
/** サンプル位置 ↔ X 座標。ピッチと波形でズームを共有するために使う */
struct TimeMap
{
    int64 start = 0, end = 1;
    float x0 = 0.0f, x1 = 1.0f;

    float x (int64 sample) const
    {
        return x0 + (float) ((double) (sample - start) / (double) (end - start)) * (x1 - x0);
    }

    int64 sampleAt (float px) const
    {
        return start + (int64) ((double) (px - x0) / (double) (x1 - x0) * (double) (end - start));
    }
};

/** サンプリングレートの kHz 表示。44100 → "44.1"、48000 → "48" */
inline juce::String formatKhz (int sampleRate)
{
    return sampleRate % 1000 == 0 ? juce::String (sampleRate / 1000) : juce::String (sampleRate / 1000.0, 1);
}

/** 録音・書き出しのビット数の表示（24 → "24bit"、32 → "32bit float"） */
inline juce::String formatBits (int bitDepth)
{
    return bitDepth >= 32 ? juce::String ("32bit float") : juce::String (bitDepth) + "bit";
}

/** dBFS の表示（小数 1 桁）。メーターの下限（-100）は "-inf" */
inline juce::String formatDb (float db)
{
    return db <= -99.95f ? juce::String ("-inf") : juce::String (db, 1);
}

/** m:ss / m:ss.mmm（表示専用。内部はサンプルが真実） */
inline juce::String formatTime (int64 samples, int sampleRate, bool withMillis)
{
    const auto totalMs = std::abs (samples) * 1000 / juce::jmax (1, sampleRate);   // 負の値は符号を付けて（前は「0:00.0-6」）
    const auto m  = totalMs / 60000;
    const auto s  = (totalMs / 1000) % 60;
    const auto ms = totalMs % 1000;
    auto str = juce::String (samples < 0 ? "-" : "") + juce::String (m) + ":" + juce::String (s).paddedLeft ('0', 2);
    if (withMillis)
        str << "." << juce::String (ms).paddedLeft ('0', 3);
    return str;
}
} // namespace vb
