#include "PitchShift.h"
#include <rubberband/RubberBandStretcher.h>
#include <cmath>

namespace vb::audio
{
std::vector<float> shiftPitch (const float* mono, juce::int64 length, double sampleRate, int semitones)
{
    std::vector<float> out (mono, mono + length);
    if (semitones == 0 || length <= 0)
        return out;

    using RB = RubberBand::RubberBandStretcher;
    RB rb ((size_t) sampleRate, 1, RB::OptionProcessOffline | RB::OptionEngineFiner | RB::OptionFormantPreserved,
           1.0, std::pow (2.0, semitones / 12.0));
    rb.setExpectedInputDuration ((size_t) length);

    constexpr juce::int64 block = 8192;
    for (juce::int64 pos = 0; pos < length; pos += block)
    {
        const float* p = mono + pos;
        const auto n = (size_t) std::min (block, length - pos);
        rb.study (&p, n, pos + block >= length);
    }

    juce::int64 written = 0;
    std::vector<float> buf;
    auto drain = [&]
    {
        for (auto avail = rb.available(); avail > 0; avail = rb.available())
        {
            buf.resize ((size_t) avail);
            float* q = buf.data();
            const auto got = (juce::int64) rb.retrieve (&q, (size_t) avail);
            for (juce::int64 i = 0; i < got && written < length; ++i)
                out[(size_t) written++] = buf[(size_t) i];
        }
    };
    for (juce::int64 pos = 0; pos < length; pos += block)
    {
        const float* p = mono + pos;
        const auto n = (size_t) std::min (block, length - pos);
        rb.process (&p, n, pos + block >= length);
        drain();
    }
    drain();
    for (; written < length; ++written)
        out[(size_t) written] = 0.0f;
    return out;
}
} // namespace vb::audio
