#include "Theme.h"
#include "VoiceBoothFonts.h"

#include <map>

namespace vb
{
namespace
{
    /** 埋め込みフォントの保持。JUCE 終了時に破棄される */
    class FontCache : private juce::DeletedAtShutdown
    {
    public:
        FontCache()
        {
            using namespace VoiceBoothFonts;
            sans[0] = load (IBMPlexSansJPRegular_ttf,  IBMPlexSansJPRegular_ttfSize);
            sans[1] = load (IBMPlexSansJPMedium_ttf,   IBMPlexSansJPMedium_ttfSize);
            sans[2] = load (IBMPlexSansJPSemiBold_ttf, IBMPlexSansJPSemiBold_ttfSize);
            mono[0] = load (IBMPlexMonoRegular_ttf,    IBMPlexMonoRegular_ttfSize);
            mono[1] = load (IBMPlexMonoMedium_ttf,     IBMPlexMonoMedium_ttfSize);
            mono[2] = load (IBMPlexMonoSemiBold_ttf,   IBMPlexMonoSemiBold_ttfSize);
        }

        ~FontCache() override { clearSingletonInstance(); }

        juce::Typeface::Ptr sans[3], mono[3];

        JUCE_DECLARE_SINGLETON_INLINE (FontCache, false)

    private:
        static juce::Typeface::Ptr load (const char* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }
    };
}

//==============================================================================
// 韓国語・中国語は OS の標準フォント（DESIGN 10.1）
//   同梱の Plex Sans JP では漢字が日本の字形になり、ハングルも無いため。
namespace
{
    struct SystemFace { juce::String family, style[3]; };

    juce::StringArray candidatesFor (i18n::Language l)
    {
        switch (l)
        {
            case i18n::Language::ko:     return { "Apple SD Gothic Neo", "Malgun Gothic", "Noto Sans CJK KR", "Noto Sans KR", "NanumGothic" };
            case i18n::Language::zhHans: return { "PingFang SC", "Microsoft YaHei UI", "Microsoft YaHei", "Noto Sans CJK SC", "Noto Sans SC", "Source Han Sans SC" };
            case i18n::Language::zhHant: return { "PingFang TC", "Microsoft JhengHei UI", "Microsoft JhengHei", "Noto Sans CJK TC", "Noto Sans TC", "Source Han Sans TC" };
            case i18n::Language::ja:
            case i18n::Language::en:     break;
        }
        return {};
    }

    juce::String pickStyle (const juce::StringArray& available, std::initializer_list<const char*> wanted)
    {
        for (auto* w : wanted)
            for (auto& a : available)
                if (a.equalsIgnoreCase (w))
                    return a;
        return available.isEmpty() ? juce::String ("Regular") : available[0];
    }

    /** 言語ごとに一度だけ探す。見つからなければ family は空（同梱フォントで描き、足りない字は OS が代替） */
    const SystemFace& systemFace (i18n::Language l)
    {
        static std::map<int, SystemFace> cache;
        if (auto it = cache.find ((int) l); it != cache.end())
            return it->second;

        static const auto installed = juce::Font::findAllTypefaceNames();
        SystemFace face;

        for (auto& name : candidatesFor (l))
        {
            if (! installed.contains (name, true))
                continue;

            const auto styles = juce::Font::findAllTypefaceStyles (name);
            face.family = name;
            face.style[0] = pickStyle (styles, { "Regular", "Normal", "Book" });
            face.style[1] = pickStyle (styles, { "Medium", "Regular", "Normal" });
            face.style[2] = pickStyle (styles, { "SemiBold", "Semibold", "DemiBold", "Bold" });
            break;
        }

        return cache[(int) l] = face;
    }

    bool containsKana (const juce::String& text)
    {
        for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
            if (const auto c = *p; c >= 0x3040 && c <= 0x30ff)
                return true;
        return false;
    }

    juce::Font embeddedSans (float height, Weight w)
    {
        return juce::Font (juce::FontOptions (FontCache::getInstance()->sans[(int) w]).withHeight (height));
    }
}

juce::Typeface::Ptr sansTypeface (Weight w)
{
    const auto lang = i18n::current();
    if (! i18n::info (lang).embeddedFont)
    {
        const auto& face = systemFace (lang);
        if (face.family.isNotEmpty())
            return juce::Font (juce::FontOptions (face.family, face.style[(int) w], 14.0f)).getTypefacePtr();
    }
    return FontCache::getInstance()->sans[(int) w];
}

juce::Font sans (float height, Weight w)
{
    const auto lang = i18n::current();
    if (! i18n::info (lang).embeddedFont)
    {
        const auto& face = systemFace (lang);
        if (face.family.isNotEmpty())
            return juce::Font (juce::FontOptions (face.family, face.style[(int) w], height));
    }
    return embeddedSans (height, w);
}

juce::Font sansFor (const juce::String& text, float height, Weight w)
{
    // 日本語の歌詞・曲名は UI の言語に関係なく日本語の字形で描く
    return containsKana (text) ? embeddedSans (height, w) : sans (height, w);
}

juce::Font mono (float height, Weight w, float tracking)
{
    return juce::Font (juce::FontOptions (FontCache::getInstance()->mono[(int) w])
                           .withHeight (height)
                           .withKerningFactor (tracking));
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}

//==============================================================================
namespace paint
{
void keycap (juce::Graphics& g, juce::Rectangle<float> r, const KeyState& s, float radius)
{
    r = r.reduced (0.5f);

    // 外側の影（沈んでいない時だけ）
    if (! s.down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), radius);
    }

    auto top = s.over ? colours::raisedHi : colours::raised;
    auto bottom = top.darker (0.18f);
    if (s.down) { top = colours::bgDeep.brighter (0.06f); bottom = colours::bgDeep.brighter (0.1f); }
    if (! s.enabled) { top = top.withMultipliedAlpha (0.5f); bottom = bottom.withMultipliedAlpha (0.5f); }

    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    // 縁と上辺のハイライト
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (r, radius, 1.0f);

    if (! s.down && s.enabled)
    {
        g.setColour (juce::Colours::white.withAlpha (s.over ? 0.09f : 0.06f));
        g.fillRect (juce::Rectangle<float> (r.getX() + radius, r.getY() + 1.0f, r.getWidth() - radius * 2.0f, 1.0f));
    }
}

void inset (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (colours::bgDeep);
    g.fillRoundedRectangle (r, radius);

    // 内側の影（上辺が暗く、下辺がわずかに明るい）
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (juce::Rectangle<float> (r.getX() + radius, r.getY(), r.getWidth() - radius * 2.0f, 1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRect (juce::Rectangle<float> (r.getX() + radius, r.getBottom() - 1.0f, r.getWidth() - radius * 2.0f, 1.0f));

    g.setColour (colours::line.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void led (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, bool lit)
{
    const auto body = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);

    if (lit)
    {
        g.setColour (colour.withAlpha (0.22f));
        g.fillEllipse (body.expanded (radius * 1.1f));
        g.setColour (colour);
        g.fillEllipse (body);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillEllipse (body.reduced (radius * 0.55f).translated (-radius * 0.2f, -radius * 0.25f));
    }
    else
    {
        g.setColour (colour.interpolatedWith (colours::bgDeep, 0.78f));
        g.fillEllipse (body);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (body, 0.8f);
    }
}

void ledBar (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float level)
{
    if (level <= 0.0f)
    {
        g.setColour (colour.interpolatedWith (colours::bgDeep, 0.84f));
        g.fillRect (r);
        return;
    }

    g.setColour (colour.interpolatedWith (colours::bgDeep, 0.84f * (1.0f - level)));
    g.fillRect (r);
}

void sectionHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& en, const juce::String& ja)
{
    auto r = area.toFloat();
    const auto f = mono (10.5f, Weight::semibold, 0.14f);
    const auto w = textWidth (f, en);

    g.setColour (colours::text.withAlpha (0.88f));
    g.setFont (f);
    g.drawText (en, r.removeFromLeft (w + 2.0f), juce::Justification::centredLeft, false);

    r.removeFromLeft (8.0f);
    g.setColour (colours::textMute);
    g.setFont (sans (11.0f));
    g.drawText (ja, r, juce::Justification::centredLeft, true);
}

void microLabel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& s, juce::Colour c, juce::Justification j)
{
    g.setColour (c);
    g.setFont (mono (9.5f, Weight::medium, 0.12f));
    g.drawText (s, r, j, false);
}

void hline (juce::Graphics& g, float y, float x0, float x1, juce::Colour c)
{
    g.setColour (c);
    g.fillRect (juce::Rectangle<float> (x0, y, x1 - x0, 1.0f));
}

void vline (juce::Graphics& g, float x, float y0, float y1, juce::Colour c)
{
    g.setColour (c);
    g.fillRect (juce::Rectangle<float> (x, y0, 1.0f, y1 - y0));
}
} // namespace paint
} // namespace vb
