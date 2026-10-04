#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>

/*  分離の入口（2026-10-04、UiSession を分ける 3 段目）。UiSession は分離プロセスを直接作らず、ここを通して使う。
    本物は SeparatorClient（別プロセス VoiceBoothSeparator）。テストは偽物に差し替える（tests/session/FakeSeparator.h） */

namespace vb::separation
{
struct Callbacks
{
    std::function<void (float progress, double etaSeconds)> progress;   // etaSeconds < 0 = まだ分からない
    std::function<void (bool ok, const juce::String& error)> done;      // error は英語の短い文（"stopped" = 止めた）
};

/** 分離を 1 本動かすもの。コールバックはメッセージスレッドで呼ぶ */
class Separator
{
public:
    virtual ~Separator() = default;
    /** 始める（動いていれば false）。lead を渡し、リードのモデルが入っていれば、続けてリードボーカルも書く。
        modelOverride：空なら分離のモデル（リードだけを取る時はリードのモデルのフォルダ） */
    virtual bool start (const juce::File& input, const juce::File& vocals, const juce::File& backing, Callbacks,
                        const juce::File& lead = {}, const juce::File& modelOverride = {}) = 0;
    virtual void stop() = 0;
    virtual bool isBusy() const = 0;
};

/** 分離が使えるか・分離を作る */
class Service
{
public:
    virtual ~Service() = default;
    /** 分離に要る物（実行ファイルとモデル）がそろっている */
    virtual bool available() const;
    /** リードボーカルのモデルが入っている（ハモリのお手本） */
    virtual bool karaokeInstalled() const;
    /** 分離の実行ファイルがある（リードだけを取る時。分離のモデルは要らない） */
    virtual bool executableExists() const;
    virtual std::unique_ptr<Separator> create();
};
} // namespace vb::separation
