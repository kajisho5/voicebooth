#include "OffVocal.h"
#include "audio/Resample.h"

namespace vb::analysis
{
juce::AudioBuffer<float> offVocalFrom (const audio::SongAudio& original, const audio::SongAudio& vocals)
{
    juce::AudioBuffer<float> out (original.buffer);
    if (vocals.buffer.getNumChannels() == 0 || vocals.length() == 0)
        return out;

    std::shared_ptr<const audio::SongAudio> v;
    if (std::abs (vocals.sampleRate - original.sampleRate) > 0.5)
        v = audio::resampleSong (vocals, original.sampleRate);
    else
        v = std::shared_ptr<const audio::SongAudio> (&vocals, [] (const audio::SongAudio*) {});
    if (v == nullptr)
        return out;

    const auto n = (int) juce::jmin ((juce::int64) out.getNumSamples(), v->length());
    const auto vch = v->buffer.getNumChannels();
    for (int ch = 0; ch < out.getNumChannels(); ++ch)
    {
        if (out.getNumChannels() == 1 && vch >= 2)
        {
            out.addFrom (0, 0, v->buffer, 0, 0, n, -0.5f);
            out.addFrom (0, 0, v->buffer, 1, 0, n, -0.5f);
        }
        else
            out.addFrom (ch, 0, v->buffer, juce::jmin (ch, vch - 1), 0, n, -1.0f);
    }
    return out;
}
} // namespace vb::analysis
