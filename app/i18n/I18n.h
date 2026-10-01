#pragma once

#include <juce_core/juce_core.h>

/*  多言語対応（DESIGN 10）

    - 画面に出す文字は必ず tr("key") で引く。ソースに表示文字列を直書きしない
    - 翻訳表は resources/i18n/<code>.json（キー → 文字列のフラットな表）
    - 引けない時は 英語 → キーそのもの の順に代替する（落ちない）
    - 差し込みは {0} {1} …（語順が言語で変わってもよいように番号で指定）
    - 追加言語は available() に 1 行足し、JSON を置くだけ
    - 日本語・英語は同梱フォント、韓国語・中国語は OS の標準フォントで描く（ui/Theme.cpp）

    tools/check_i18n.py が「全言語のキーが揃っているか」「使っているキーが表にあるか」
    「ソースに表示文字列の直書きが無いか」を検査する。 */

namespace vb::i18n
{
enum class Language { ja, en, ko, zhHans, zhHant };

struct LanguageInfo
{
    Language id;
    const char* code;          // "ja" / "zh-Hans" など（翻訳表のファイル名）
    const char* nativeName;    // その言語での名前（UTF-8）
    bool embeddedFont;         // true: 同梱の IBM Plex Sans JP / false: OS の標準フォント（DESIGN 10.1）
};

const std::vector<LanguageInfo>& available();

/** OS の表示言語から決める（対応外は英語） */
Language fromSystem();

/** コード（"ja" / "en" / "ko" / "zh-Hans" / "zh_TW" / "zh-HK" …）から。不明なら fallback */
Language fromCode (const juce::String& code, Language fallback);

const char* codeOf (Language);
const LanguageInfo& info (Language);

void setLanguage (Language);
Language current();

/** キーを引く */
juce::String tr (const char* key);

/** キーを引いて {0} {1} … を差し込む */
/** {index} を value で置き換える */
juce::String substitute (const juce::String& text, int index, const juce::String& value);

template <typename... Args>
juce::String tr (const char* key, Args&&... args)
{
    auto s = tr (key);
    int index = 0;
    ((s = substitute (s, index++, juce::String (std::forward<Args> (args)))), ...);
    return s;
}

/** 指定言語の表にキーがあるか（検査用） */
bool has (Language, const char* key);
} // namespace vb::i18n

namespace vb
{
using i18n::tr;
}
