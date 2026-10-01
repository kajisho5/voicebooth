#pragma once

#include <juce_core/juce_core.h>

/*  デバイスまわりの決まりごと（Phase B3）。デバイスを開かずに試せる部分だけ
    PlaybackEngine（デバイス依存）はこれを呼ぶだけにする。 */

namespace vb::audio
{
/** Bluetooth らしい名前か（遅延警告用。DESIGN 5）
    名前だけで見る当て推量：AirPods / Bluetooth / BT（単語）/ Hands-Free / A2DP / bluez（Linux）。
    OS から接続方式は取らない（取れない OS がある）。外れても警告が出ないだけで、動作は変えない */
bool looksLikeBluetooth (const juce::String& deviceName);

/** 入力チャンネル（DESIGN 13：モノラル化。既定は L = 0）
    wanted が範囲外なら 0（L）。デバイスに入力が無ければ -1 */
int resolveInputChannel (int wanted, int available);

/** そのチャンネルだけを開く指定（1 ch・モノラル） */
juce::BigInteger inputChannelMask (int channel);

/** デバイスが申告した往復の遅れ（実測は B6）
    JUCE の値は各ドライバでバッファ分を含む（WASAPI / Core Audio / DirectSound はバッファ込み、ALSA は周期×(数-1)）ので、
    入力 + 出力をそのまま足す。両方 0（申告なし）の時だけバッファ 2 つ分で推定する */
struct ReportedLatency
{
    int samples = 0;
    bool estimated = false;
};
ReportedLatency reportedLatency (int inputLatency, int outputLatency, int bufferSize);

/** いま開いているデバイスの要点（変化の判定用） */
struct DeviceSnapshot
{
    juce::String type, input, output;   // 入力 / 出力の機器名（空 = なし）
    double sampleRate = 0.0;
    int bufferSize = 0;
    bool open = false;

    bool operator== (const DeviceSnapshot& o) const
    {
        return type == o.type && input == o.input && output == o.output
            && std::abs (sampleRate - o.sampleRate) < 0.5 && bufferSize == o.bufferSize && open == o.open;
    }
    bool operator!= (const DeviceSnapshot& o) const { return ! (*this == o); }
};

/** 使っていた機器が外れた（別の機器に替わった・閉じた）か。SR やバッファだけの変化は外れではない */
bool deviceLost (const DeviceSnapshot& before, const DeviceSnapshot& after);
} // namespace vb::audio
