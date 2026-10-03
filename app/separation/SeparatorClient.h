#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>

/*  分離プロセス VoiceBoothSeparator を本体から動かす（DESIGN 11.3 / B16）
    子プロセスで起動し、標準出力の ready / chunk / progress / done / error を読む。結果は WAV（44.1 kHz ステレオ float）。
    中止は子プロセスを止める。本体が落ちた時は、子が --parent-pid で気づいて止まる。
    コールバックはメッセージスレッドで呼ぶ。録音・再生とは関係なく動く（別プロセス・別スレッド）。 */

namespace vb::separation
{
class SeparatorClient : private juce::Thread
{
public:
    SeparatorClient();
    ~SeparatorClient() override;

    /** 本体の隣の VoiceBoothSeparator（無ければ存在しないファイル） */
    static juce::File executable();
    /** モデルのフォルダ（環境変数 VB_SEPARATION_MODEL があればそこ、無ければアプリのデータの Models/separation/<id>） */
    static juce::File modelFolder();
    /** 使うモデルの名前と版（キャッシュの鍵に入れる） */
    static juce::String modelId();
    /** 分離に要る物（実行ファイルとモデルの部品）がそろっている */
    static bool available();
    static bool modelInstalled();

    /** リードボーカルのモデル（BS-RoFormer karaoke、anvuew、GPL-3.0。ハモリのお手本 = 声 − リード。2026-10-02）。
        環境変数 VB_KARAOKE_MODEL があればそのフォルダ、無ければ Models/karaoke/<id> */
    static juce::String karaokeModelId();
    static juce::File karaokeModelFolder();
    static bool karaokeInstalled();

    /** お手本の音程のモデル（RMVPE、analysis/Rmvpe.h）。環境変数 VB_PITCH_MODEL があればそのファイル、
        無ければアプリのデータの Models/pitch/<id>/rmvpe.onnx */
    static juce::String pitchModelId();
    static juce::File pitchModelFile();
    static bool pitchAvailable();
    /** 音程を取る（分離プロセスの --pitch。終わるまで待つので裏のスレッドから呼ぶ）。
        wav はモノラル、frames は 10 ms ごとの（セント、強さ）。失敗・モデルが無ければ false */
    static bool runPitch (const juce::File& wav, std::vector<std::pair<float, float>>& frames);

    struct Callbacks
    {
        std::function<void (float progress, double etaSeconds)> progress;   // etaSeconds < 0 = まだ分からない
        std::function<void (bool ok, const juce::String& error)> done;      // error は英語の短い文（"stopped" = 止めた）
    };

    /** 始める（動いていれば false）。lead を渡し、リードのモデルが入っていれば、続けてリードボーカルも書く */
    bool start (const juce::File& input, const juce::File& vocals, const juce::File& backing, Callbacks,
                const juce::File& lead = {}, const juce::File& modelOverride = {});
    void stop();
    bool isBusy() const { return isThreadRunning(); }

private:
    void run() override;
    void finish (bool ok, const juce::String& error);

    juce::File input, vocals, backing, lead, model;   // model：空なら分離のモデル（リードだけを取る時はリードのモデル）
    Callbacks callbacks;
    std::unique_ptr<juce::ChildProcess> child;
    juce::CriticalSection childLock;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
} // namespace vb::separation
