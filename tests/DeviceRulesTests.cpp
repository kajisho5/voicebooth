#include "audio/DeviceRules.h"

namespace vb::audio
{
class DeviceRulesTests : public juce::UnitTest
{
public:
    DeviceRulesTests() : juce::UnitTest ("DeviceRules", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("Bluetooth-looking names (heuristic)");
        {
            for (auto* deviceName : { "AirPods Pro", "Bluetooth Headset", "WH-1000XM4 Hands-Free AG Audio", "Headset (BT)",
                                      "Galaxy BT Speaker", "bluez_source.00_11_22.handsfree_head_unit", "Speaker A2DP",
                                      "BT-Mic", "airpods max" })
                expect (looksLikeBluetooth (deviceName), deviceName);

            for (auto* deviceName : { "USB Audio Interface", "Built-in Microphone", "MacBook Pro Microphone", "Subtle Mic",
                                      "BTX-200", "Realtek(R) Audio", "Scarlett 2i2 USB", "" })
                expect (! looksLikeBluetooth (deviceName), deviceName);
        }

        beginTest ("speaker-looking output names (monitor feedback guard)");
        {
            for (auto* deviceName : { "MacBook Pro Speakers", "Speakers (Realtek(R) Audio)", "Galaxy BT Speaker",
                                      "Built-in Speaker", "SPEAKERS (USB Audio)" })
                expect (looksLikeSpeakers (deviceName), deviceName);
            for (auto* utf8 : { "スピーカー (Realtek(R) Audio)", "扬声器 (Realtek)", "揚聲器 (USB)", "스피커 (Realtek)" })
                expect (looksLikeSpeakers (juce::String::fromUTF8 (utf8)), juce::String::fromUTF8 (utf8));

            // ヘッドホンの語があれば違う（差したヘッドホンにも同じ機器名を使うドライバがある）
            for (auto* deviceName : { "Speakers/Headphones (Realtek(R) Audio)", "External Headphones", "Headset Earphone",
                                      "AirPods Pro", "Scarlett 2i2 USB", "MacBook Pro Microphone", "Realtek(R) Audio", "" })
                expect (! looksLikeSpeakers (deviceName), deviceName);
            for (auto* utf8 : { "ヘッドホン (Realtek(R) Audio)", "スピーカー/ヘッドホン (Realtek)", "耳机 (USB)" })
                expect (! looksLikeSpeakers (juce::String::fromUTF8 (utf8)), juce::String::fromUTF8 (utf8));
        }

        beginTest ("input channel: default L, selectable, falls back to L");
        {
            expectEquals (resolveInputChannel (0, 2), 0);    // L
            expectEquals (resolveInputChannel (1, 2), 1);    // R
            expectEquals (resolveInputChannel (3, 8), 3);
            expectEquals (resolveInputChannel (1, 1), 0);    // モノラルの機器は L
            expectEquals (resolveInputChannel (5, 2), 0);
            expectEquals (resolveInputChannel (-1, 2), 0);
            expectEquals (resolveInputChannel (0, 0), -1);   // 入力なし
        }

        beginTest ("input channel mask opens exactly one channel");
        {
            const auto l = inputChannelMask (0), r = inputChannelMask (1);
            expectEquals (l.countNumberOfSetBits(), 1);
            expect (l[0] && ! l[1]);
            expectEquals (r.countNumberOfSetBits(), 1);
            expect (r[1] && ! r[0]);
            expect (inputChannelMask (-1).isZero());
        }

        beginTest ("reported latency = input + output (estimate only when nothing is reported)");
        {
            auto a = reportedLatency (300, 256, 256);
            expectEquals (a.samples, 556);
            expect (! a.estimated);
            auto b = reportedLatency (0, 0, 256);
            expectEquals (b.samples, 512);
            expect (b.estimated);
            auto c = reportedLatency (-5, 0, 128);
            expectEquals (c.samples, 256);
            expect (c.estimated);
            auto d = reportedLatency (0, 480, 0);
            expectEquals (d.samples, 480);
            expect (! d.estimated);
        }

        beginTest ("device lost vs. changed");
        {
            DeviceSnapshot a;
            a.type = "ALSA"; a.input = "Mic"; a.output = "Speakers"; a.sampleRate = 48000.0; a.bufferSize = 256; a.open = true;

            auto b = a;
            expect (a == b);
            b.sampleRate = 44100.0;
            expect (a != b);
            expect (! deviceLost (a, b));        // SR が変わっただけ
            b = a; b.bufferSize = 512;
            expect (! deviceLost (a, b));

            b = a; b.input = {};
            expect (deviceLost (a, b));          // 入力が外れた
            b = a; b.output = "Other";
            expect (deviceLost (a, b));          // 別の出力へ（既定に戻された）
            b = a; b.open = false;
            expect (deviceLost (a, b));          // 止まった
            b = a; b.type = "JACK";
            expect (deviceLost (a, b));

            auto closed = a; closed.open = false;
            expect (! deviceLost (closed, a));   // もともと開いていなければ「外れた」ではない
            auto noInput = a; noInput.input = {};
            b = a;
            expect (! deviceLost (noInput, b));  // 入力が足された
        }
    }
};

static DeviceRulesTests deviceRulesTests;
} // namespace vb::audio
