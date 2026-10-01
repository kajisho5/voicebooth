#pragma once

#include "DummySession.h"
#include "Widgets.h"

namespace vb
{
/** DESIGN 4.4 歌詞。現在フレーズを強調。失敗しても空で進める */
class LyricsLane : public juce::Component
{
public:
    explicit LyricsLane (const dummy::Session&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 56;

private:
    const dummy::Session& session;
    ChipButton editButton { jp ("歌詞パッド"), colours::text };
    juce::Rectangle<int> textArea;
};
} // namespace vb
