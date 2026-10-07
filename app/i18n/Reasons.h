#pragma once

#include "I18n.h"

/*  失敗の理由を、使う人の言語の文にする（#23）。
    録音・書き出し・分離・モデルの処理は、理由を短い英語の決まった文字列（"file exists"・"busy"・"can't reach <url>" など）で返す。
    UI に依存しない処理の側はそのままにして、知らせに入れる直前でここを通し、翻訳表の文にする。
    知らない理由（OS のエラーの文など）はそのまま返す */

namespace vb::i18n
{
struct ReasonKey
{
    const char* key = nullptr;      // 翻訳表のキー（nullptr = 知らない理由。text をそのまま使う）
    juce::String arg;               // {0} に入れる物（ファイル名・URL など。無ければ空）
};

/** 理由の文字列から翻訳表のキーを選ぶ（テストする） */
ReasonKey reasonKey (const juce::String& reason);

/** 理由を使う人の言語の文にする */
inline juce::String reasonText (const juce::String& reason)
{
    const auto r = reasonKey (reason);
    if (r.key == nullptr)
        return reason;
    if (r.arg.isEmpty())
        return tr (r.key);
    // 確認用ミックスのトラック名（書き出しの側は英語の名前で返す）：画面のトラック名にそろえる（バグチェック 2026-10-06）
    static const std::pair<const char*, const char*> tracks[] = {
        { "Main", "track.main" }, { "Double", "track.double" }, { "Harmony 1", "track.harm1" }, { "Harmony 2", "track.harm2" },
        { "Backing", "track.backing" }, { "Guide", "track.guide" } };
    if (juce::String (r.key) == "reason.cantRender")
        for (auto& [label, key] : tracks)
            if (r.arg == label)
                return tr (r.key, tr (key));
    return tr (r.key, r.arg);
}
} // namespace vb::i18n

namespace vb
{
using i18n::reasonText;
}
