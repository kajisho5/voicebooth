#pragma once

#include "Icons.h"

namespace vb
{
/** プルダウン（選択肢が多いもの：表示言語など）。
    キーキャップに現在の値と ▼、押すと Booth 風のポップアップ（VoiceBoothLookAndFeel が描く） */
class Dropdown : public juce::Component
{
public:
    Dropdown (juce::StringArray items, int selected);

    void setSelected (int index, juce::NotificationType = juce::sendNotification);
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;

    void setFont (juce::Font f) { labelFont = f; repaint(); }
    int idealWidth() const;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray items;
    int selected = 0;
    bool open = false;
    juce::Font labelFont;
};
} // namespace vb
