#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

/*  Windows で m4a（AAC）を読む（DESIGN 7.1 / 19）
    JUCE 標準の読み手は Windows で m4a を読めないため、OS 標準の Media Foundation を使う。
    追加のライブラリ・ライセンスは不要（Windows 7 以降に標準。N エディションは Media Feature Pack が要る）。

    - 対象は .m4a / .mp4 / .aac。mp3 は従来どおり Windows Media（JUCE 標準）
    - 32bit float で受け取る。SR・チャンネルは元のまま（リサンプルしない）
    - Windows 以外ではこのクラスは無い（Mac の m4a は Core Audio、Linux は開発用で非対応） */

namespace vb::audio
{
#if JUCE_WINDOWS
class MediaFoundationAudioFormat final : public juce::AudioFormat
{
public:
    MediaFoundationAudioFormat();

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
#endif

/** 曲を開くための読み手一式（JUCE 標準 ＋ Windows では Media Foundation） */
void registerSongFormats (juce::AudioFormatManager&);
} // namespace vb::audio
