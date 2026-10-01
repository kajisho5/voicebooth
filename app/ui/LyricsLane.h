#pragma once

#include "DummySession.h"
#include "parts/KeyButton.h"

namespace vb
{
/** DESIGN 4.4 歌詞。現在フレーズを強調（歌った分を点灯色でワイプ）、次フレーズを薄く */
class LyricsLane : public juce::Component
{
public:
    explicit LyricsLane (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 60;

private:
    const dummy::Session& session;
    KeyButton editButton { jp ("歌詞パッド"), KeyButton::Kind::ghost };
    juce::Rectangle<int> textArea;
};
} // namespace vb
