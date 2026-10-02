#pragma once

#include "../Overlay.h"

namespace vb
{
/** スキンの縮図（地・バー・ピッチレーンの帯と線・メーター・REC）を、そのスキンの色で描く。
    いまの画面の色（colours::）は使わない */
void paintSkinThumbnail (juce::Graphics&, juce::Rectangle<float>, const skin::Skin&);

/** DESIGN 4.11「テンプレートから作る」。内蔵 10 種と自作スキンを縮図で並べ、
    選んだものの 16 色を出発点にしてスキンエディタを開く */
class SkinTemplates : public DialogPanel
{
public:
    SkinTemplates (std::vector<skin::Skin> templates, const juce::String& activeId);
    ~SkinTemplates() override;

    std::function<void (const skin::Skin&)> onChosen;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    class Grid;
    juce::Viewport viewport;
    std::unique_ptr<Grid> grid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkinTemplates)
};
} // namespace vb
