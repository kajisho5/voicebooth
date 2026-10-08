#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

/*  共有用の動画（DESIGN 9.1）の MP4 を書く。映像は H.264、音は AAC。OS の仕組みを使う（ffmpeg は同梱しない）
      - Windows：Media Foundation の Sink Writer
      - Mac：AVFoundation の AVAssetWriter（VideoEncoder.mm がこのファイルを Objective-C++ として読む）
      - Linux（開発用）：PATH に ffmpeg があれば使う。なければ書けない（available() が false）
    使い方：open（音は先に全部渡す）→ 1 コマずつ addFrame → finish。音は各コマの時刻までを中で交互に書く。
    1 つのスレッドから使う（バックグラウンドのスレッド）。UI に依存しない */

namespace vb::video
{
/** 音の SR（AAC の符号化器が受け取る値。Windows の AAC は 44.1 / 48 kHz だけ） */
constexpr int audioRate = 48000;

struct Spec
{
    int width = 1080, height = 1920;   // 偶数
    int fps = 30;
    int videoBitrate = 8000000;
    int audioBitrate = 192000;         // Windows の AAC は 96 / 128 / 160 / 192 kbps だけ
};

class Encoder
{
public:
    virtual ~Encoder() = default;

    /** この OS で書けるか（Linux は ffmpeg があるか。Mac は H.264 で 1 コマ符号化できるか） */
    static bool available() { return problem().isEmpty(); }
    /** 書けない理由（英語の短い文。書けるなら空）。初めて呼ぶときに確かめ、あとは覚えておく */
    static juce::String problem();
    static std::unique_ptr<Encoder> create();

    /** audio は audioRate のステレオ。nullptr か空なら音なし。dest は上書きする。戻り値は失敗の理由（空なら成功。英語の短い文） */
    virtual juce::String open (const juce::File& dest, const Spec&, std::shared_ptr<const juce::AudioBuffer<float>> audio) = 0;

    /** 1 コマ。幅 × 高さ、メモリの並びは B G R A（JUCE の ARGB の画像と同じ）、上の行から。lineStride は 1 行のバイト数 */
    virtual juce::String addFrame (const juce::uint8* bgra, int lineStride) = 0;

    /** 残りの音を書いて閉じる。失敗したら dest は残さない */
    virtual juce::String finish() = 0;
};

/** dest の seconds 秒のあたりのコマを 1 枚読む（書いた動画の確認。テスト用）。argb は 0xAARRGGBB で上の行から。読めなければ false */
bool readFrame (const juce::File&, double seconds, int& width, int& height, std::vector<juce::uint32>& argb);

/** 音の 1 コマ分の区切り：frame（0 始まり）のコマの終わりまでに書く音のサンプル数 */
inline juce::int64 audioSamplesUntilFrame (juce::int64 frame, int fps)
{
    return fps > 0 ? (frame + 1) * (juce::int64) audioRate / fps : 0;
}

/** 長さ（秒）からコマの数（端数は 1 コマに切り上げ） */
inline juce::int64 frameCount (double seconds, int fps)
{
    return seconds > 0.0 && fps > 0 ? (juce::int64) std::ceil (seconds * fps - 1.0e-9) : 0;
}
} // namespace vb::video
