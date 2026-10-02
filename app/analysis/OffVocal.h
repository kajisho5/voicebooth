#pragma once

#include "audio/SongLoader.h"

/*  原曲だけ（B16。DESIGN 7.1.1）：原曲から分離した声を引いてオフボを作る。
    分離プロセスの伴奏（44.1 kHz の mix − vocals）をそのまま使わず、声を原曲の SR にそろえてから原曲から引く。
    こうすると「原曲 − オフボ」がちょうど分離した声になり、お手本の線（B9 の引き算）がそのまま使える。
    UI・音声デバイスに依存しない（テストから使う） */

namespace vb::analysis
{
/** 長さ・チャンネル数・SR は原曲と同じ。vocals はどの SR・長さでもよい（足りない所は 0、余りは捨てる）。
    原曲がモノラルなら声の左右の平均を引く。原曲が 3 ch 以上なら 3 ch 目からは声の右を引く */
juce::AudioBuffer<float> offVocalFrom (const audio::SongAudio& original, const audio::SongAudio& vocals);
} // namespace vb::analysis
