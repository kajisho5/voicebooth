#pragma once

#include "UiSession.h"
#include "Actions.h"
#include "Timeline.h"
#include "parts/KeyButton.h"
#include "parts/Encoder.h"
#include "parts/ConsoleFader.h"
#include "parts/LedMeter.h"

/*  右ラック（DESIGN 4 / 4.7 / 4.8）
    INPUT    入力メーター・デバイス・レイテンシ
    PRACTICE テンポ・キー（練習用。納品 REC 中はロック）
    MONITOR  モニターミックス（耳だけ。録音バスには入らない）
    RECORD   録音モードと録音先 */

namespace vb
{
class RackModule : public juce::Component
{
public:
    RackModule (juce::String english, juce::String japanese) : en (std::move (english)), ja (std::move (japanese)) {}

    void paint (juce::Graphics&) override;

protected:
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (metrics::pad, 0).withTrimmedTop (headerH).withTrimmedBottom (12); }
    static constexpr int headerH = 36;

private:
    juce::String en, ja;
};

//==============================================================================
class InputModule : public RackModule, private SessionView
{
public:
    InputModule (UiSession&, Actions&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void onSessionChanged (juce::uint32 c) override;

    Actions& actions;
    LedMeter meter;
    KeyButton buffer { {}, KeyButton::Kind::ghost };
    juce::Rectangle<int> deviceArea, readoutArea;
};

class PracticeModule : public RackModule, private SessionView
{
public:
    PracticeModule (UiSession&, Actions&);
    void resized() override;

private:
    void onSessionChanged (juce::uint32) override;

    Actions& actions;
    EncoderBlock tempo, key;
    bool syncing = false;
};

class MonitorModule : public RackModule, private SessionView
{
public:
    explicit MonitorModule (UiSession&);
    void resized() override;

private:
    void onSessionChanged (juce::uint32 c) override;

    juce::OwnedArray<ChannelStrip> strips;
    ChannelStrip* backingStrip = nullptr;   // B2：これだけ音が出る
    ChannelStrip* harmStrip = nullptr;
};

class RecordModule : public RackModule, private SessionView
{
public:
    explicit RecordModule (UiSession&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void onSessionChanged (juce::uint32) override;

    SegmentedKeys recMode;
    juce::Rectangle<int> targetArea, lockArea;
};

//==============================================================================
class Rack : public juce::Component, private SessionView
{
public:
    Rack (UiSession&, Actions&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void onSessionChanged (juce::uint32 c) override { if (c & change::transport) repaint(); }

    InputModule input;
    PracticeModule practice;
    MonitorModule monitor;
    RecordModule record;
};
} // namespace vb
