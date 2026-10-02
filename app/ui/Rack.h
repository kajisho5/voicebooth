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

    void updateKeyHelp();

    Actions& actions;
    EncoderBlock tempo, key;
    KeyButton rangeKey, suggestKey;   // 声域（測る）と、声域に合うキー（2026-10-02）
    bool syncing = false;
};

class MonitorModule : public RackModule, private SessionView
{
public:
    explicit MonitorModule (UiSession&);
    void paint (juce::Graphics&) override;
    void resized() override;

    /** モジュールの下に出す知らせ（スピーカー出力・モニターの遅れ）。無ければ空 */
    struct Notice { juce::String text; colours::Tone tone = colours::textDim; };
    static Notice noticeFor (const dummy::Session&);

    /** 遅れの目安（DESIGN 7.3：往復 20–40 ms）。これを超えたら知らせる */
    static constexpr double lateMonitorMs = 40.0;

private:
    void onSessionChanged (juce::uint32 c) override;
    void updateMeters();

    juce::OwnedArray<ChannelStrip> strips;
    ChannelStrip* backingStrip = nullptr;   // B2：オフボ
    ChannelStrip* mainStrip = nullptr;
    ChannelStrip* harmStrip = nullptr;
    ChannelStrip* selfStrip = nullptr;      // B4：自分の声
    ChannelStrip* reverbStrip = nullptr;    // B4：モニターリバーブ（耳だけ）
    ChannelStrip* clickStrip = nullptr;     // クリック・カウントインの音量（耳だけ。2026-10-02）
    juce::Rectangle<int> noticeArea;
    bool hadNotice = false;
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
