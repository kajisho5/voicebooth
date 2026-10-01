#include "MainComponent.h"

namespace vb
{
MainComponent::MainComponent()
{
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &header, &transport, &pitch, &lyrics, &wave, &tracks, &controls, &status })
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

    header.setBounds (r.removeFromTop (HeaderBar::height));
    transport.setBounds (r.removeFromTop (TransportBar::height));
    status.setBounds (r.removeFromBottom (StatusBar::height));
    controls.setBounds (r.removeFromBottom (ControlPanel::height));
    tracks.setBounds (r.removeFromBottom (TrackTabs::height));
    wave.setBounds (r.removeFromBottom (WaveLane::height));
    lyrics.setBounds (r.removeFromBottom (LyricsLane::height));
    pitch.setBounds (r);   // 残りは全部ピッチレーン（主役）
}
} // namespace vb
