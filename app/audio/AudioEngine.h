#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>

/*  音声エンジンの境界（DESIGN 7）
    UI はこのインターフェース越しにだけ音声へ触る。B2 で再生（オフボ）、B3 でデバイス列挙と入力メーター、
    B4 で自分の声のモニター（入力 → 出力。リバーブはモニターだけ）を結線。録音は B5。

    ルール（DESIGN 17）
      - オーディオスレッドでメモリ確保・ファイル I/O・長いロック待ちをしない
      - サンプル位置は int64
      - モニターバスと録音バスは分離（モニターリバーブは録音に入れない） */

namespace vb::audio
{
using int64 = juce::int64;
struct SongAudio;

/** 入力メーターの値（dBFS。UI が 30 Hz で読む） */
struct InputLevel
{
    float peakDb = -100.0f;
    float rmsDb  = -100.0f;
    float holdDb = -100.0f;     // ピークホールド（1.5 秒）
    bool  clipped = false;      // -0.1 dBFS 以上が来た（消すまで残る）
};

/** 出力デバイスの状態（ステータスバー用） */
struct OutputStatus
{
    bool open = false;
    juce::String deviceName, typeName, error;
    double sampleRate = 0.0;
    int bufferSize = 0;
    bool converting = false;    // 曲と SR が違い、試聴用に変換している
    bool stalled = false;       // 開いているのに音の処理が止まった（ドライバ・サウンドサーバーが応答しない）
};

/** マイクの使用許可（Mac だけ OS に聞く。Win / Linux は notNeeded） */
enum class MicPermission { notNeeded, granted, asking, denied };

/** 入力が使えない理由（UI で翻訳して出す） */
enum class InputProblem
{
    none,
    noDevice,           // 入力デバイスが無い
    openFailed,         // 開けない（error に OS / ドライバの文言）
    noChannels,         // 選んだ機器は開けたが、入力のチャンネルが来ない
    permissionAsking,   // Mac：許可のダイアログに答え待ち
    permissionDenied,   // Mac：許可されていない（セットアップで止める。DESIGN 13）
    stalled             // デバイスが応答しない（音の処理が止まった）
};

/** 入力デバイスの状態 */
struct InputStatus
{
    bool open = false;
    InputProblem problem = InputProblem::noDevice;
    juce::String deviceName, typeName, error;
    int channel = 0;              // 開いているチャンネル（0 = L）
    int numChannels = 0;          // デバイスの入力チャンネル数
    double sampleRate = 0.0;
    int bufferSize = 0;
    int inputLatency = 0, outputLatency = 0;   // デバイスが申告した値（サンプル）。実測は B6
    MicPermission permission = MicPermission::notNeeded;
    bool silent = false;          // 開いているのに 0 ちょうどが続く（許可・ミュートの疑い）
    bool bluetooth = false;       // 名前から見た当て推量（DeviceRules）
};

/** 選べるデバイス（入力セットアップ Step 1） */
struct DeviceList
{
    juce::StringArray types;            // ドライバ（WASAPI / DirectSound / ASIO / Core Audio / ALSA …）
    juce::String currentType;
    juce::StringArray inputs, outputs;  // いまのドライバの機器名
    juce::String currentInput, currentOutput;
    juce::Array<double> sampleRates;    // いま開いている機器が対応する SR
    juce::Array<int> bufferSizes;
    double sampleRate = 0.0;
    int bufferSize = 0;
};

class AudioEngine
{
public:
    virtual ~AudioEngine() = default;

    // 曲（B2）
    virtual void setSong (std::shared_ptr<const SongAudio>) = 0;
    virtual bool hasSong() const = 0;

    // 輸送
    virtual void  play() = 0;
    virtual void  stop() = 0;
    virtual bool  isPlaying() const = 0;
    virtual void  seek (int64 sample) = 0;
    virtual int64 getPlayheadSample() const = 0;
    virtual bool  consumeReachedEnd() = 0;
    virtual void  setLoop (int64 startSample, int64 endSample, bool enabled) = 0;

    // モニター（オフボ）。fader は 0..1（0.75 = 0 dB）
    virtual void setBackingLevel (float fader, bool muted) = 0;

    // 自分の声のモニター（B4）。fader は 0..1（0.75 = 0 dB）。止まっていても入力があれば鳴る
    virtual void setSelfMonitor (float /*fader*/, bool /*muted*/) {}
    // モニターリバーブの返り（0..1、0.75 = 0 dB の送り）。耳だけで、録音には入らない（DESIGN 4.7 / 14）
    virtual void setMonitorReverb (float /*fader*/) {}

    virtual OutputStatus getOutputStatus() const = 0;

    // デバイス（B3）。切り替えの戻り値は失敗の理由（空なら成功。OS / ドライバの文言そのまま）
    virtual DeviceList getDeviceList() const { return {}; }
    virtual void rescanDevices() {}
    virtual juce::String setDeviceType (const juce::String&)   { return {}; }
    virtual juce::String setInputDevice (const juce::String&)  { return {}; }
    virtual juce::String setOutputDevice (const juce::String&) { return {}; }
    virtual juce::String setInputChannel (int)                 { return {}; }
    virtual juce::String setBufferSize (int)                   { return {}; }

    /** デバイスが外から変わった（抜けた・OS で切り替えた）。メッセージスレッドで呼ばれる。lost = 使っていた機器が外れた */
    virtual void setDeviceChangeCallback (std::function<void (bool lost)>) {}

    // 入力（B3：メーターだけ）
    virtual InputStatus getInputStatus() const { return {}; }
    virtual InputLevel getInputLevel() const { return {}; }
    virtual void resetInputClip() {}
    virtual int64 getLatencyCompensationSamples() const { return 0; }
};
} // namespace vb::audio
