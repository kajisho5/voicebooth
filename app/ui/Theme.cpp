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
// 韓国語・中国語・ベトナム語・トルコ語は OS の標準フォント（DESIGN 10.1）
//   同梱の Plex Sans JP では漢字が日本の字形になり、ハングルも無いため。
//   ベトナム語（ă ơ ư đ と声調の合成字）とトルコ語（ğ ş İ）の字も Plex Sans JP に無く、
//   1 語の中で書体が混ざらないよう、その字を持つ OS のフォントで全体を描く。
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
            case i18n::Language::vi:
            case i18n::Language::tr:     return { "Helvetica Neue", "Segoe UI", "Arial", "Noto Sans", "DejaVu Sans", "Liberation Sans" };
            case i18n::Language::ja:
            case i18n::Language::en:
            case i18n::Language::es:
            case i18n::Language::ptBR:
            case i18n::Language::id:
            case i18n::Language::de:
            case i18n::Language::fr:     break;
        }
        return {};
    }

    /** その言語に欠かせない字（候補のフォントが本当に持っているか確かめる。空なら確かめない） */
    const char* probeFor (i18n::Language l)
    {
        if (l == i18n::Language::vi) return "\xc4\x83\xc6\xa1\xc6\xb0\xc4\x91\xe1\xba\xa1\xe1\xbb\x87\xe1\xbb\xb1";   // ă ơ ư đ ạ ệ ự
        if (l == i18n::Language::tr) return "\xc4\x9f\xc5\x9f\xc4\xb0\xc4\xb1";                                           // ğ ş İ ı
        return "";
    }

    bool hasGlyphs (const juce::String& family, const juce::String& style, const char* probe)
    {
        if (*probe == 0)
            return true;

        // getTypefacePtr() は LookAndFeel を通って再帰するので、OS から直接引く（systemTypeface と同じ）
        const auto face = juce::Typeface::createSystemTypefaceFor (juce::Font (juce::FontOptions (family, style, 14.0f)));
        if (face == nullptr)
            return false;

        for (auto p = juce::String::fromUTF8 (probe).getCharPointer(); ! p.isEmpty(); ++p)
            if (! face->getNominalGlyphForCodepoint (*p).has_value())
                return false;
        return true;
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
            const auto regular = pickStyle (styles, { "Regular", "Normal", "Book" });
            if (! hasGlyphs (name, regular, probeFor (l)))
                continue;

            face.family = name;
            face.style[0] = regular;
            face.style[1] = pickStyle (styles, { "Medium", "Regular", "Normal", "Book" });
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

namespace
{
    /** OS のフォントの書体（言語・太さごとに一度だけ作る。無ければ nullptr）
        名前だけの juce::Font から getTypefacePtr() すると LookAndFeel::getTypefaceForFont（→ sansTypeface）を
        通って自分自身を呼び続けるので、書体そのものを OS から直接作って持つ */
    juce::Typeface::Ptr systemTypeface (i18n::Language lang, Weight w)
    {
        static std::map<int, juce::Typeface::Ptr> cache;
        const auto key = (int) lang * 3 + (int) w;
        if (auto it = cache.find (key); it != cache.end())
            return it->second;

        juce::Typeface::Ptr face;
        const auto& sys = systemFace (lang);
        if (sys.family.isNotEmpty())
            face = juce::Typeface::createSystemTypefaceFor (juce::Font (juce::FontOptions (sys.family, sys.style[(int) w], 14.0f)));

        return cache[key] = face;
    }

    /** OS のフォントで描くラテン文字の言語（ベトナム語・トルコ語）は、同じ高さだと同梱の Plex Sans JP より字が大きく出る
        （JUCE の高さは ascent + descent で、和文フォントは上下が広い。Linux の DejaVu Sans で約 29 %。#19）。
        同じ英数字の文の幅が同梱フォントとそろうよう、高さに掛ける比を求める（言語ごとに一度）。
        韓国語・中国語は和文と同じ作りのフォントなのでそのまま */
    float systemScale (i18n::Language lang)
    {
        if (lang != i18n::Language::vi && lang != i18n::Language::tr)
            return 1.0f;
        static std::map<int, float> cache;
        if (auto it = cache.find ((int) lang); it != cache.end())
            return it->second;

        auto k = 1.0f;
        if (auto face = systemTypeface (lang, Weight::regular))
        {
            const juce::String probe ("The quick brown fox jumps over the lazy dog 0123456789");
            const auto sys = textWidth (juce::Font (juce::FontOptions (face).withHeight (100.0f)), probe);
            const auto emb = textWidth (embeddedSans (100.0f, Weight::regular), probe);
            if (sys > 0.0f && emb > 0.0f)
                k = juce::jlimit (0.7f, 1.0f, emb / sys);
        }
        return cache[(int) lang] = k;
    }
}

juce::Typeface::Ptr sansTypeface (Weight w)
{
    const auto lang = i18n::current();
    if (! i18n::info (lang).embeddedFont)
        if (auto face = systemTypeface (lang, w))
            return face;
    return FontCache::getInstance()->sans[(int) w];
}

namespace
{
    float textBoost = 1.0f;
}

void setTextBoost (float amount)
{
    textBoost = juce::jlimit (0.0f, 1.0f, amount);
}

float textBoostAmount() { return textBoost; }

float textBoostFor (int userAreaHeight)
{
    // 作業領域の高さ 800 以下 → 0、1000 以上（1920x1080 の 100 %）→ 1。その間はなめらかに
    return juce::jlimit (0.0f, 1.0f, (float) (userAreaHeight - 800) / 200.0f);
}

float uiTextHeight (float h)
{
    const auto full = juce::jlimit (1.15f, 1.6f, 1.6f - juce::jmax (0.0f, h - 12.0f) * 0.035f);
    return h * (1.0f + (full - 1.0f) * textBoost);
}

namespace
{
    /** 日本語の本文は字の実寸 11 px（JUCE の高さ 16.5）を下回らない（注記・小さな説明が読めないため） */
    float sansHeight (float h) { return juce::jmax (uiTextHeight (h), juce::jmin (16.5f, h * 2.0f) * textBoost); }
}

namespace
{
    juce::Font sansInExact (i18n::Language lang, float height, Weight w)
    {
        if (! i18n::info (lang).embeddedFont)
            if (auto face = systemTypeface (lang, w))
                return juce::Font (juce::FontOptions (face).withHeight (height * systemScale (lang)));
        return embeddedSans (height, w);
    }
}

juce::Font sansExact (float height, Weight w)
{
    return sansInExact (i18n::current(), height, w);
}

juce::Font monoExact (float height, Weight w, float tracking)
{
    return juce::Font (juce::FontOptions (FontCache::getInstance()->mono[(int) w])
                           .withHeight (height)
                           .withKerningFactor (tracking));
}

juce::Font sansIn (i18n::Language lang, float height, Weight w)
{
    height = sansHeight (height);
    if (! i18n::info (lang).embeddedFont)
        if (auto face = systemTypeface (lang, w))
            return juce::Font (juce::FontOptions (face).withHeight (height * systemScale (lang)));
    return embeddedSans (height, w);
}

juce::Font sans (float height, Weight w)
{
    return sansIn (i18n::current(), height, w);
}

juce::Font sansForLanguageName (const juce::String& text, float height, Weight w)
{
    // 言語名（"简体中文" など）はその言語の字形で描く
    for (auto& info : i18n::available())
        if (text == juce::String::fromUTF8 (info.nativeName))
            return sansIn (info.id, height, w);
    return sans (height, w);
}

juce::Font sansFor (const juce::String& text, float height, Weight w)
{
    // 日本語の歌詞・曲名は UI の言語に関係なく日本語の字形で描く
    return containsKana (text) ? embeddedSans (sansHeight (height), w) : sans (height, w);
}

juce::Font mono (float height, Weight w, float tracking)
{
    return monoExact (uiTextHeight (height), w, tracking);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}

//==============================================================================
// スキン（DESIGN 4.11）
namespace colours
{
namespace
{
    Token* const tokens[skin::numTokens] = { &bg0, &bgDeep, &panel, &raised, &raisedHi, &grid, &line, &lineHi,
                                             &text, &textDim, &textMute, &signal, &ref, &warn, &bad, &rec };
    bool light = false;
}

juce::Colour current (int index)
{
    return juce::isPositiveAndBelow (index, skin::numTokens) ? juce::Colour (*tokens[index]) : juce::Colour();
}

bool isLight() { return light; }

juce::Colour shadow (float alpha)
{
    // 明るい地に黒い影をそのまま落とすと汚れて見えるので弱める
    return juce::Colours::black.withAlpha (light ? alpha * 0.4f : alpha);
}

juce::Colour highlight (float alpha)
{
    return juce::Colours::white.withAlpha (light ? juce::jmin (1.0f, alpha * 4.0f) : alpha);
}

juce::Colour onFill (juce::Colour fill)
{
    const auto argb = fill.getARGB();
    return skin::contrastRatio (text.getARGB(), argb) > skin::contrastRatio (bgDeep.getARGB(), argb) ? juce::Colour (text) : juce::Colour (bgDeep);
}

juce::Colour onRec()
{
    return text.getPerceivedBrightness() >= bgDeep.getPerceivedBrightness() ? juce::Colour (text) : juce::Colour (bgDeep);
}
}

void applySkin (const skin::Skin& s)
{
    for (int i = 0; i < skin::numTokens; ++i)
        *colours::tokens[i] = juce::Colour (s.colours[(size_t) i]);
    colours::light = colours::bg0.getPerceivedBrightness() > 0.5f;
}

skin::Colours currentSkinColours()
{
    skin::Colours c {};
    for (int i = 0; i < skin::numTokens; ++i)
        c[(size_t) i] = colours::tokens[i]->getARGB();
    return c;
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
        g.setColour (colours::shadow (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), radius);
    }

    auto top = s.over ? colours::raisedHi : colours::raised;
    auto bottom = top.darker (0.18f);
    if (s.down) { top = colours::bgDeep.brighter (0.06f); bottom = colours::bgDeep.brighter (0.1f); }
    if (! s.enabled) { top = top.withMultipliedAlpha (0.5f); bottom = bottom.withMultipliedAlpha (0.5f); }

    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    // 縁と上辺のハイライト
    g.setColour (colours::shadow (0.55f));
    g.drawRoundedRectangle (r, radius, 1.0f);

    if (! s.down && s.enabled)
    {
        g.setColour (colours::highlight (s.over ? 0.09f : 0.06f));
        g.fillRect (juce::Rectangle<float> (r.getX() + radius, r.getY() + 1.0f, r.getWidth() - radius * 2.0f, 1.0f));
    }
}

void inset (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (colours::bgDeep);
    g.fillRoundedRectangle (r, radius);

    // 内側の影（上辺が暗く、下辺がわずかに明るい）
    g.setColour (colours::shadow (0.5f));
    g.fillRect (juce::Rectangle<float> (r.getX() + radius, r.getY(), r.getWidth() - radius * 2.0f, 1.0f));
    g.setColour (colours::highlight (0.04f));
    g.fillRect (juce::Rectangle<float> (r.getX() + radius, r.getBottom() - 1.0f, r.getWidth() - radius * 2.0f, 1.0f));

    g.setColour (colours::line.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

namespace
{
    /** 白い光の画像（中心が濃く、縁で消える）。一度だけ作る */
    class GlowSprite : public juce::DeletedAtShutdown
    {
    public:
        GlowSprite() : image (juce::Image::ARGB, size, size, true)
        {
            juce::Graphics g (image);
            const auto c = (float) size * 0.5f;
            juce::ColourGradient grad (juce::Colours::white, c, c, juce::Colours::white.withAlpha (0.0f), c, 0.0f, true);
            grad.addColour (0.35, juce::Colours::white.withAlpha (0.55f));
            grad.addColour (0.7, juce::Colours::white.withAlpha (0.12f));
            g.setGradientFill (grad);
            g.fillEllipse (0.0f, 0.0f, (float) size, (float) size);
        }
        ~GlowSprite() override { clearSingletonInstance(); }

        static constexpr int size = 64;
        juce::Image image;

        JUCE_DECLARE_SINGLETON_SINGLETHREADED_MINIMAL_INLINE (GlowSprite)
    };
}

void glow (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    if (colour.getAlpha() == 0 || area.isEmpty())
        return;

    const auto& img = GlowSprite::getInstance()->image;
    g.setColour (colour);
    g.drawImage (img, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                 0, 0, img.getWidth(), img.getHeight(), true);   // 画像のアルファを型にして、色で塗る
}

void led (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    if (level >= 1.0f) { led (g, c, radius, colour, true); return; }
    led (g, c, radius, colour, false);
    if (level <= 0.0f) return;

    // 消えかけ・点きかけ：点灯の絵を薄く重ねる
    const auto body = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
    g.setColour (colour.withAlpha (0.22f * level));
    g.fillEllipse (body.expanded (radius * 1.1f * level));
    g.setColour (colour.withAlpha (level));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.55f * level * level));
    g.fillEllipse (body.reduced (radius * 0.55f).translated (-radius * 0.2f, -radius * 0.25f));
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
        g.setColour (colours::shadow (0.5f));
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
