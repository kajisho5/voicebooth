#pragma once

#include "DummySession.h"
#include "Timeline.h"

namespace vb
{
/** DESIGN 4.5 波形レーン。現在トラックを大きく、他は薄く小さく。全トラック同じ高さで並べない */
class WaveLane : public juce::Component
{
public:
    explicit WaveLane (const dummy::Session&);

    void paint (juce::Graphics&) override;

    static constexpr int height = 140;

private:
    struct Row { project::TrackType type; juce::String name; juce::Rectangle<float> area; bool current; };

    std::vector<Row> layoutRows() const;
    void drawCompBar (juce::Graphics&, const TimeMap&, juce::Rectangle<float>, project::TrackType);
    void drawWave (juce::Graphics&, const TimeMap&, const Row&);

    const dummy::Session& session;
};
} // namespace vb
