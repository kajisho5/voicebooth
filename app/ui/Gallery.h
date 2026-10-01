#pragma once

#include "parts/KeyButton.h"
#include "parts/Encoder.h"
#include "parts/ConsoleFader.h"
#include "parts/LedMeter.h"
#include "parts/Readout.h"

namespace vb
{
/** 部品ギャラリー（開発用。`VoiceBooth --gallery` で表示）
    全部品を状態ごとに並べ、見た目の確認とスクリーンショットに使う */
class Gallery : public juce::Component
{
public:
    Gallery();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Labeled { juce::Component* comp; juce::String caption; };

    juce::OwnedArray<juce::Component> owned;
    std::vector<Labeled> items;
    juce::Rectangle<int> keysArea, segArea, encArea, faderArea, meterArea, readoutArea, iconArea, tokenArea, typeArea;

    template <typename T, typename... Args>
    T& make (const juce::String& caption, Args&&... args)
    {
        auto* c = new T (std::forward<Args> (args)...);
        owned.add (c);
        addAndMakeVisible (c);
        items.push_back ({ c, caption });
        return *c;
    }
};
} // namespace vb
