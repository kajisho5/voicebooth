#include "Shortcuts.h"

namespace vb::shortcuts
{
namespace
{
    struct Info { Action action; const char* id; const char* nameKey; juce::juce_wchar key; };
    constexpr Info infos[numActions] = {
        { Action::record,     "record",     "shortcuts.record",     'r' },
        { Action::loop,       "loop",       "shortcuts.loop",       'l' },
        { Action::rangeIn,    "rangeIn",    "shortcuts.rangeIn",    '[' },
        { Action::rangeOut,   "rangeOut",   "shortcuts.rangeOut",   ']' },
        { Action::tapTempo,   "tapTempo",   "shortcuts.tapTempo",   't' },
        { Action::addSection, "addSection", "shortcuts.addSection", 'm' },
        { Action::track1,     "track1",     "shortcuts.track1",     '1' },
        { Action::track2,     "track2",     "shortcuts.track2",     '2' },
        { Action::track3,     "track3",     "shortcuts.track3",     '3' },
        { Action::track4,     "track4",     "shortcuts.track4",     '4' },
    };

    juce::juce_wchar normalise (juce::juce_wchar c) { return juce::CharacterFunctions::toLowerCase (c); }
}

const char* nameKey (Action a) { return infos[(size_t) a].nameKey; }
const char* id (Action a)      { return infos[(size_t) a].id; }

bool assignable (juce::juce_wchar c)
{
    return c > 0x20 && c < 0x7f;   // 表示できる ASCII の 1 文字（空白・制御文字・ASCII 以外は外す）
}

juce::String keyName (juce::juce_wchar c)
{
    if (c == 0)
        return {};
    return juce::String::charToString (juce::CharacterFunctions::toUpperCase (c));
}

Map Map::defaults()
{
    Map m;
    for (auto& i : infos)
        m.keys[(size_t) i.action] = i.key;
    return m;
}

std::optional<Action> Map::actionFor (juce::juce_wchar typed) const
{
    const auto c = normalise (typed);
    if (! assignable (c))
        return std::nullopt;
    for (int i = 0; i < numActions; ++i)
        if (keys[(size_t) i] == c)
            return (Action) i;
    return std::nullopt;
}

std::optional<Action> Map::assign (Action a, juce::juce_wchar typed)
{
    const auto c = normalise (typed);
    if (! assignable (c))
        return std::nullopt;
    std::optional<Action> displaced;
    for (int i = 0; i < numActions; ++i)
        if ((Action) i != a && keys[(size_t) i] == c)
        {
            keys[(size_t) i] = 0;
            displaced = (Action) i;
        }
    keys[(size_t) a] = c;
    return displaced;
}

juce::String Map::toString() const
{
    // キーは文字コードの番号で書く（";" や "=" もキーにできるので、文字のままだと区切りと取り違える）
    juce::StringArray parts;
    for (auto& i : infos)
        parts.add (juce::String (i.id) + "=" + juce::String ((int) keys[(size_t) i.action]));
    return parts.joinIntoString (";");
}

Map Map::fromString (const juce::String& text)
{
    auto m = defaults();
    for (auto& part : juce::StringArray::fromTokens (text, ";", {}))
    {
        const auto name = part.upToFirstOccurrenceOf ("=", false, false).trim();
        const auto value = part.fromFirstOccurrenceOf ("=", false, false).trim();
        if (! value.containsOnly ("0123456789") || value.isEmpty())
            continue;   // 読めない値は既定のまま
        for (auto& i : infos)
            if (name == i.id)
            {
                const auto c = (juce::juce_wchar) value.getIntValue();
                if (c == 0)               m.keys[(size_t) i.action] = 0;
                else if (assignable (c))  m.keys[(size_t) i.action] = normalise (c);
            }
    }
    // 同じキーが 2 つあれば、後の方をなしにする（手で書き換えたファイルなど）
    for (int i = 0; i < numActions; ++i)
        for (int j = i + 1; j < numActions; ++j)
            if (m.keys[(size_t) i] != 0 && m.keys[(size_t) i] == m.keys[(size_t) j])
                m.keys[(size_t) j] = 0;
    return m;
}
} // namespace vb::shortcuts
