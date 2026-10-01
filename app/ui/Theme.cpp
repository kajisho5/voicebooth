#include "Theme.h"
#include "VoiceBoothFonts.h"

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

juce::Typeface::Ptr sansTypeface (Weight w)
{
    return FontCache::getInstance()->sans[(int) w];
}

juce::Font sans (float height, Weight w)
{
    return juce::Font (juce::FontOptions (sansTypeface (w)).withHeight (height));
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
