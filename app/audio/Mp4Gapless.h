#pragma once

#include <juce_core/juce_core.h>

/*  m4a（MP4 の AAC）の頭の詰め物と、本当の長さを読む（DESIGN 19）
    AAC は先頭に詰め物（priming。ffmpeg は 1024、iTunes は 2112 サンプル）が付く。
    MP4 にはそれを飛ばす指示が書いてある：
      1. 音声トラックの edit list（moov/trak/edts/elst の media_time と segment_duration）
      2. iTunes のタグ iTunSMPB（moov/udta/meta/ilst/----）
    Windows の Media Foundation はこれを無視して詰め物ごと出す（CI で +1024 を計測）ので、
    読み手の側で飛ばす。Mac の Core Audio は自分で処理する（計測で 0）ので使わない。 */

namespace vb::audio
{
struct Mp4Gapless
{
    bool found = false;
    juce::int64 priming = 0;          // 頭で飛ばすサンプル数（デコード後の SR）
    juce::int64 validSamples = -1;    // 本当の長さ（分からなければ -1）
    juce::String source;              // "elst" / "iTunSMPB"（記録用）
};

/** ファイルの先頭から MP4 の箱をたどって読む。MP4 でなければ found = false */
Mp4Gapless readMp4Gapless (juce::InputStream&, double sampleRate);
} // namespace vb::audio
