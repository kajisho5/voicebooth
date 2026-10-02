#include "WelcomeScreen.h"
#include "../TopBar.h"

namespace vb
{
namespace
{
    juce::StringArray languageNames()
    {
        juce::StringArray a;
        for (auto& l : i18n::available())
            a.add (juce::String::fromUTF8 (l.nativeName));
        return a;
    }

    int indexOf (i18n::Language lang)
    {
        const auto& list = i18n::available();
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].id == lang)
                return (int) i;
        return 0;
    }

    // 各言語での「ようこそ」（その言語の字形で描く。データなので翻訳表には入れない）
    struct Greeting { i18n::Language lang; const char* text; };
    const Greeting greetings[] = {
        { i18n::Language::ja,     "\xe3\x82\x88\xe3\x81\x86\xe3\x81\x93\xe3\x81\x9d" },               // ようこそ
        { i18n::Language::en,     "Welcome" },
        { i18n::Language::ko,     "\xed\x99\x98\xec\x98\x81\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4" },   // 환영합니다
        { i18n::Language::zhHans, "\xe6\xac\xa2\xe8\xbf\x8e" },                                       // 欢迎
        { i18n::Language::zhHant, "\xe6\xad\xa1\xe8\xbf\x8e" },                                       // 歡迎
    };
}

WelcomeScreen::WelcomeScreen()
    : language (languageNames(), indexOf (i18n::current())),
      continueKey (tr ("welcome.continue"))
{
    language.setFont (sans (15.0f, Weight::medium));
    language.onChange = [this] (int i) { if (onLanguage) onLanguage (i18n::available()[(size_t) i].id); };
    addAndMakeVisible (language);

    continueKey.withLed (colours::signal).withToggle (false).withFont (sans (14.0f, Weight::semibold));
    continueKey.setToggleState (true, juce::dontSendNotification);
    continueKey.onClick = [this] { if (onContinue) onContinue(); };
    addAndMakeVisible (continueKey);
}

void WelcomeScreen::resized()
{
    column = getLocalBounds().withSizeKeepingCentre (juce::jmin (560, getWidth() - 80), 460);

    auto r = column.withTrimmedTop (250);
    languageLabelArea = r.removeFromTop (24);
    r.removeFromTop (8);
    language.setBounds (r.removeFromTop (48));
    r.removeFromTop (60);

    continueKey.setSize (10, 44);
    const auto w = juce::jmax (180, continueKey.idealWidth());
    continueKey.setBounds (r.removeFromTop (44).removeFromRight (w));
}

void WelcomeScreen::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg0);

    auto r = column;
    drawBoothMark (g, r.removeFromTop (72).toFloat().withSizeKeepingCentre (64.0f, 64.0f).withX ((float) r.getX()), false);
    r.removeFromTop (24);

    // 各言語の「ようこそ」を、それぞれの字形で
    {
        auto line = r.removeFromTop (30).toFloat();
        const auto sep = juce::String::fromUTF8 (" \xc2\xb7 ");   // ·
        for (size_t i = 0; i < std::size (greetings); ++i)
        {
            const auto f = sansIn (greetings[i].lang, 17.0f, Weight::medium);
            const auto text = juce::String::fromUTF8 (greetings[i].text);
            const auto w = textWidth (f, text);
            g.setColour (greetings[i].lang == i18n::current() ? colours::signal : colours::textDim);
            g.setFont (f);
            g.drawText (text, line.removeFromLeft (w + 1.0f), juce::Justification::centredLeft, false);

            if (i + 1 < std::size (greetings))
            {
                g.setColour (colours::textMute);
                g.setFont (sans (17.0f));
                g.drawText (sep, line.removeFromLeft (textWidth (sans (17.0f), sep) + 1.0f), juce::Justification::centredLeft, false);
            }
        }
    }
    r.removeFromTop (14);

    g.setColour (colours::text);
    g.setFont (sans (26.0f, Weight::semibold));
    g.drawText (tr ("welcome.title"), r.removeFromTop (40), juce::Justification::centredLeft, true);
    g.setColour (colours::textDim);
    g.setFont (sans (14.0f));
    g.drawText (tr ("welcome.sub"), r.removeFromTop (26), juce::Justification::centredLeft, true);

    paint::microLabel (g, languageLabelArea.toFloat(), tr ("welcome.language"), colours::textMute);

    g.setColour (colours::textMute);
    g.setFont (sans (12.0f));
    g.drawText (tr ("welcome.note"), language.getBounds().translated (0, 56).withHeight (20), juce::Justification::centredLeft, true);
}
} // namespace vb
