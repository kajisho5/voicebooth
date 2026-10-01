#pragma once

#include "UiSession.h"
#include "parts/KeyButton.h"

namespace vb
{
/** DESIGN 4.4 歌詞。現在フレーズを強調（歌った分を点灯色でワイプ）、次フレーズを薄く */
class LyricsLane : public juce::Component, private SessionView
{
public:
    explicit LyricsLane (UiSession&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 60;

private:
    void onSessionChanged (juce::uint32 c) override { if (c & (change::playhead | change::transport)) repaint (textArea); }

    KeyButton editButton { {}, KeyButton::Kind::ghost };
    juce::Rectangle<int> textArea;
};
} // namespace vb
