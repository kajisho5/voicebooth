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
        const auto file = juce::String (codeOf (l)) + "_json";
        int size = 0;
        const char* data = nullptr;

        for (int i = 0; i < VoiceBoothI18n::namedResourceListSize; ++i)
            if (file == VoiceBoothI18n::namedResourceList[i])
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
        { Language::ja, "ja", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e" },   // 日本語
        { Language::en, "en", "English" },
    };
    return list;
}

const char* codeOf (Language l)
{
    for (auto& info : available())
        if (info.id == l)
            return info.code;
    return "en";
}

Language fromCode (const juce::String& code, Language fallback)
{
    const auto c = code.toLowerCase().substring (0, 2);
    for (auto& info : available())
        if (c == info.code)
            return info.id;
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
