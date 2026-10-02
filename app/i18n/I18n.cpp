#include "I18n.h"
#include "VoiceBoothI18n.h"

#include <set>
#include <string>
#include <unordered_map>

namespace vb::i18n
{
namespace
{
    using Table = std::unordered_map<std::string, juce::String>;

    struct Store
    {
        Language language = Language::ja;
        Table active, english;
        std::set<std::string> reportedMissing;
    };

    Store& store()
    {
        static Store s;
        return s;
    }

    Table load (Language l)
    {
        // 埋め込み名は記号が変換されるので（zh-Hans.json → zhHans_json 等）元のファイル名で探す
        const auto file = juce::String (codeOf (l)) + ".json";
        int size = 0;
        const char* data = nullptr;

        for (int i = 0; i < VoiceBoothI18n::namedResourceListSize; ++i)
            if (file == VoiceBoothI18n::getNamedResourceOriginalFilename (VoiceBoothI18n::namedResourceList[i]))
                data = VoiceBoothI18n::getNamedResource (VoiceBoothI18n::namedResourceList[i], size);

        Table t;
        if (data == nullptr)
        {
            jassertfalse;   // 翻訳表が埋め込まれていない
            return t;
        }

        const auto json = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        if (auto* obj = json.getDynamicObject())
            for (auto& p : obj->getProperties())
                t[p.name.toString().toStdString()] = p.value.toString();

        jassert (! t.empty());   // JSON の書式誤り
        return t;
    }

    bool loaded = false;

    void ensureLoaded()
    {
        if (loaded) return;
        auto& s = store();
        s.english = load (Language::en);
        s.active = s.language == Language::en ? s.english : load (s.language);
        loaded = true;
    }
}

const std::vector<LanguageInfo>& available()
{
    static const std::vector<LanguageInfo> list {
        { Language::ja,     "ja",      "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",                 true  },   // 日本語
        { Language::en,     "en",      "English",                                                   true  },
        { Language::ko,     "ko",      "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4",                 false },   // 한국어
        { Language::zhHans, "zh-Hans", "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87",   false },   // 简体中文
        { Language::zhHant, "zh-Hant", "\xe7\xb9\x81\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87",   false },   // 繁體中文
        { Language::es,     "es",      "Espa\xc3\xb1ol",                                       true  },   // Español（ラテン文字は同梱の Plex で足りる）
        { Language::ptBR,   "pt-BR",   "Portugu\xc3\xaas (Brasil)",                             true  },   // Português (Brasil)
        { Language::id,     "id",      "Bahasa Indonesia",                                          true  },
        { Language::vi,     "vi",      "Ti\xe1\xba\xbfng Vi\xe1\xbb\x87t",                      false },   // Tiếng Việt（ă ơ ư đ と声調の合成字が Plex Sans JP に無い）
        { Language::tr,     "tr",      "T\xc3\xbcrk\xc3\xa7" "e",                                false },   // Türkçe（ğ ş İ が Plex Sans JP に無い）
        { Language::de,     "de",      "Deutsch",                                                   true  },
        { Language::fr,     "fr",      "Fran\xc3\xa7" "ais",                                      true  },   // Français
    };
    return list;
}

const LanguageInfo& info (Language l)
{
    for (auto& i : available())
        if (i.id == l)
            return i;
    return available()[1];
}

const char* codeOf (Language l)
{
    return info (l).code;
}

Language fromCode (const juce::String& raw, Language fallback)
{
    const auto code = raw.trim().toLowerCase().replaceCharacter ('_', '-');
    if (code.isEmpty())
        return fallback;

    // 中国語：繁体（Hant / 台湾 / 香港 / マカオ）と簡体（それ以外）
    if (code.startsWith ("zh"))
    {
        if (code.contains ("hant") || code.contains ("-tw") || code.contains ("-hk") || code.contains ("-mo"))
            return Language::zhHant;
        return Language::zhHans;
    }

    // ポルトガル語はブラジル以外（pt-PT など）も pt-BR で表示する。インドネシア語は旧コード "in" も
    if (code == "pt" || code.startsWith ("pt-"))
        return Language::ptBR;
    if (code == "in" || code.startsWith ("in-"))
        return Language::id;

    for (auto& i : available())
        if (code == juce::String (i.code).toLowerCase() || code.startsWith (juce::String (i.code).toLowerCase() + "-"))
            return i.id;

    return fallback;
}

Language fromSystem()
{
    return fromCode (juce::SystemStats::getDisplayLanguage(), fromCode (juce::SystemStats::getUserLanguage(), Language::en));
}

void setLanguage (Language l)
{
    auto& s = store();
    s.language = l;
    loaded = false;
    ensureLoaded();
}

Language current()
{
    return store().language;
}

juce::String tr (const char* key)
{
    ensureLoaded();
    auto& s = store();
    const std::string k (key);

    if (auto it = s.active.find (k); it != s.active.end())
        return it->second;

    if (s.reportedMissing.insert (k).second)
    {
        DBG ("i18n: missing key '" << key << "' for " << codeOf (s.language));
    }

    if (auto it = s.english.find (k); it != s.english.end())
        return it->second;

    return juce::String (key);
}

juce::String substitute (const juce::String& text, int index, const juce::String& value)
{
    return text.replace ("{" + juce::String (index) + "}", value);
}

bool has (Language l, const char* key)
{
    return load (l).count (key) > 0;
}
} // namespace vb::i18n
