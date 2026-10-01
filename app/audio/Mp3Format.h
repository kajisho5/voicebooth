#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

/*  mp3 を読む（全 OS 共通。DESIGN 7.1 / 19）
    同梱の minimp3（CC0、third_party/minimp3）を使う。OS のデコーダは使わない。

    理由（CI と実曲で計測、2026-10-01）
      - OS によって曲の頭の位置が違った：Mac（Core Audio）0、Windows（Windows Media）+1729 サンプル。
        同じプロジェクトを Win と Mac で開くとオフボと歌の頭が数十 ms ずれる（DESIGN 14）
      - Windows Media の読み手は途中から読むと位置がさらにずれた
    minimp3 は LAME / Xing タグの遅延・詰め物で頭と尻を切り（ffmpeg と同じ規約）、サンプル単位でシークできる。
    開けない mp3 は OS の読み手に回る（登録順で後ろ） */

namespace vb::audio
{
class Mp3AudioFormat final : public juce::AudioFormat
{
public:
    Mp3AudioFormat();

    juce::Array<int> getPossibleSampleRates() override { return {}; }
    juce::Array<int> getPossibleBitDepths() override   { return {}; }
    bool canDoStereo() override                         { return true; }
    bool canDoMono() override                           { return true; }

    juce::AudioFormatReader* createReaderFor (juce::InputStream*, bool deleteStreamIfOpeningFails) override;

    // 読むだけ
    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (std::unique_ptr<juce::OutputStream>&,
                                                              const juce::AudioFormatWriterOptions&) override
    {
        return nullptr;
    }
};
} // namespace vb::audio
