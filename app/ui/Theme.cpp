#include "Theme.h"
#include "VoiceBoothFonts.h"

namespace vb
{
namespace
{
    /** 埋め込みフォントの保持。JUCE 終了時に破棄される。 */
    class FontCache : private juce::DeletedAtShutdown
    {
    public:
        FontCache()
        {
            regular = juce::Typeface::createSystemTypefaceFor (VoiceBoothFonts::NotoSansJPRegular_otf,
                                                               (size_t) VoiceBoothFonts::NotoSansJPRegular_otfSize);
            bold    = juce::Typeface::createSystemTypefaceFor (VoiceBoothFonts::NotoSansJPBold_otf,
                                                               (size_t) VoiceBoothFonts::NotoSansJPBold_otfSize);
        }

        ~FontCache() override { clearSingletonInstance(); }

        juce::Typeface::Ptr regular, bold;

        JUCE_DECLARE_SINGLETON_INLINE (FontCache, false)
    };
}

juce::Typeface::Ptr typeface (FontWeight w)
{
    auto* cache = FontCache::getInstance();
    return w == FontWeight::bold ? cache->bold : cache->regular;
}

juce::Font font (float height, FontWeight w)
{
    return juce::Font (juce::FontOptions (typeface (w)).withHeight (height));
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}

void drawCard (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, metrics::radius);
    g.setColour (colours::border);
    g.drawRoundedRectangle (r.reduced (0.5f), metrics::radius, 1.0f);
}

void drawCardTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, FontWeight::bold));
    g.drawText (title, area, juce::Justification::centredLeft, true);
}
} // namespace vb
