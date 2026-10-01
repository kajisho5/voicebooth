#pragma once

#include "DummySession.h"
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
class InputModule : public RackModule
{
public:
    explicit InputModule (const dummy::Session&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const dummy::Session& session;
    LedMeter meter;
    juce::Rectangle<int> deviceArea, readoutArea;
};

class PracticeModule : public RackModule
{
public:
    explicit PracticeModule (const dummy::Session&);
    void resized() override;

private:
    EncoderBlock tempo, key;
};

class MonitorModule : public RackModule
{
public:
    explicit MonitorModule (const dummy::Session&);
    void resized() override;

private:
    juce::OwnedArray<ChannelStrip> strips;
};

class RecordModule : public RackModule
{
public:
    explicit RecordModule (const dummy::Session&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const dummy::Session& session;
    SegmentedKeys recMode;
    juce::Rectangle<int> targetArea, lockArea;
};

//==============================================================================
class Rack : public juce::Component
{
public:
    explicit Rack (const dummy::Session&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    InputModule input;
    PracticeModule practice;
    MonitorModule monitor;
    RecordModule record;
};
} // namespace vb
