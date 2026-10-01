#include "MainComponent.h"

namespace vb
{
MainComponent::MainComponent()
{
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &top, &transport, &pitch, &lyrics, &wave, &tracks, &rack, &status })
        addAndMakeVisible (c);

    setSize (defaultWidth, defaultHeight);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    top.setBounds (r.removeFromTop (TopBar::height));
    transport.setBounds (r.removeFromTop (TransportBar::height));
    status.setBounds (r.removeFromBottom (StatusBar::height));
    rack.setBounds (r.removeFromRight (metrics::rackWidth));

    // キャンバス：下から積み、残りはすべてピッチレーン（主役）
    tracks.setBounds (r.removeFromBottom (TrackTabs::height));
    wave.setBounds (r.removeFromBottom (WaveLane::height));
    lyrics.setBounds (r.removeFromBottom (LyricsLane::height));
    pitch.setBounds (r);
}
} // namespace vb
