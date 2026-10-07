#include "HelpDialog.h"

namespace vb
{
namespace
{
    constexpr int headerRowH = 42, rowGap = 4, bodyPad = 14, actionKeyH = 32, preferredW = 760, preferredH = 720;

    juce::Font bodyFont() { return sans (13.0f); }

    /** 見出しの行（押すと開く・閉じる。Tab で移れて Enter / Space で押せる） */
    class Header : public juce::Button
    {
    public:
        Header (const juce::String& titleIn, bool relevantIn) : juce::Button (titleIn), title (titleIn), relevant (relevantIn)
        {
            focus::tabOnly (*this);
            setTitle (title);
        }

        bool open = false;

        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (open ? colours::raised : (over ? colours::raised.withAlpha (0.6f) : colours::panel));
            g.fillRoundedRectangle (r, 6.0f);
            auto t = r.reduced (12.0f, 0.0f);
            drawIcon (g, open ? Icon::chevronDown : Icon::chevronRight, t.removeFromLeft (16.0f).withSizeKeepingCentre (14.0f, 14.0f),
                      colours::textDim);
            t.removeFromLeft (8.0f);
            if (relevant)
            {
                // いまの状態に関係する印（色だけに頼らないよう、文字も添える）
                const auto label = tr ("help.relevant");
                g.setFont (sans (11.0f, Weight::semibold));
                const auto w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label) + 22.0f;
                auto badge = t.removeFromRight (w).withSizeKeepingCentre (w, 22.0f);
                g.setColour (colours::warn.withAlpha (0.16f));
                g.fillRoundedRectangle (badge, 11.0f);
                g.setColour (colours::warn);
                g.fillEllipse (badge.removeFromLeft (16.0f).withSizeKeepingCentre (6.0f, 6.0f).translated (4.0f, 0.0f));
                g.drawText (label, badge, juce::Justification::centredLeft, false);
                t.removeFromRight (8.0f);
            }
            g.setColour (colours::text);
            g.setFont (sans (13.5f, Weight::medium));
            g.drawText (title, t, juce::Justification::centredLeft, true);
            ring.paint (g, *this, r, 6.0f);
        }

        void focusGained (FocusChangeType c) override { ring.gained (*this, c); }
        void focusLost (FocusChangeType) override     { ring.lost (*this); }
        void mouseDown (const juce::MouseEvent& e) override { juce::Button::mouseDown (e); focus::handBack (*this); }
        bool keyPressed (const juce::KeyPress& k) override
        {
            // juce::Button は Enter だけ。Space でも開く（コメントどおりに。監査 2026-10-06）
            if (k == juce::KeyPress::spaceKey) { triggerClick(); return true; }
            return juce::Button::keyPressed (k);
        }

    private:
        juce::String title;
        bool relevant;
        focus::Ring ring;
    };
}

class HelpDialog::List : public juce::Component
{
public:
    explicit List (const UiSession& session) : items (help::items (session))
    {
        for (size_t i = 0; i < items.size(); ++i)
        {
            auto* h = headers.add (new Header (tr (help::titleKey (items[i].topic)), items[i].relevant));
            h->onClick = [this, i] { setOpen (open == (int) i ? -1 : (int) i); };
            addAndMakeVisible (h);

            auto* k = actionKeys.add (new KeyButton());
            if (const auto* key = help::actionKey (help::action (items[i].topic)))
            {
                k->setButtonText (tr (key));
                const auto act = help::action (items[i].topic);
                k->withIcon (act == help::Action::report ? Icon::globe
                           : act == help::Action::downloadModels ? Icon::download : Icon::mic);
                k->onClick = [this, act] { if (onAction) onAction (act); };
            }
            addChildComponent (k);
        }
        // いまの状態に関係する項目があれば、最初の 1 つを開いておく
        if (! items.empty() && items.front().relevant)
            open = 0;
    }

    std::function<void (help::Action)> onAction;
    std::function<void()> onLayoutChanged;
    std::vector<help::Item> items;
    int open = -1;

    void setOpen (int index)
    {
        open = index;
        if (onLayoutChanged) onLayoutChanged();
    }

    /** 幅 w のときの高さ（直し方の文の折り返しを測る） */
    int heightFor (int w) const
    {
        int h = 0;
        for (size_t i = 0; i < items.size(); ++i)
        {
            h += headerRowH + rowGap;
            if ((int) i == open)
                h += bodyHeight ((int) i, w) + rowGap;
        }
        return h;
    }

    void resized() override
    {
        int y = 0;
        const auto w = getWidth();
        for (size_t i = 0; i < items.size(); ++i)
        {
            auto* h = headers[(int) i];
            h->open = (int) i == open;
            h->setBounds (0, y, w, headerRowH);
            y += headerRowH + rowGap;
            auto* k = actionKeys[(int) i];
            if ((int) i == open)
            {
                const auto bh = bodyHeight ((int) i, w);
                bodyAreas[(size_t) i] = { 0, y, w, bh };
                const bool hasAction = help::action (items[i].topic) != help::Action::none;
                k->setVisible (hasAction);
                if (hasAction)
                {
                    k->setSize (10, actionKeyH);
                    k->setBounds (bodyPad + 18, y + bh - bodyPad - actionKeyH, juce::jmax (180, k->idealWidth()), actionKeyH);
                }
                y += bh + rowGap;
            }
            else
            {
                bodyAreas[(size_t) i] = {};
                k->setVisible (false);
            }
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto area = bodyAreas[(size_t) i];
            if (area.isEmpty())
                continue;
            g.setColour (colours::bgDeep);
            g.fillRoundedRectangle (area.toFloat(), 6.0f);
            g.setColour (colours::text.withAlpha (0.92f));
            layoutFor ((int) i, area.getWidth()).draw (g, textArea (area).toFloat());
        }
    }

private:
    juce::OwnedArray<Header> headers;
    juce::OwnedArray<KeyButton> actionKeys;
    std::array<juce::Rectangle<int>, help::numTopics> bodyAreas {};

    static juce::Rectangle<int> textArea (juce::Rectangle<int> area)
    {
        return area.reduced (bodyPad).withTrimmedLeft (18);
    }

    juce::TextLayout layoutFor (int index, int w) const
    {
        juce::AttributedString a;
        a.setLineSpacing (5.0f);
        a.append (tr (help::bodyKey (items[(size_t) index].topic)), bodyFont(), colours::text.withAlpha (0.92f));
        juce::TextLayout l;
        l.createLayout (a, (float) juce::jmax (100, w - 2 * bodyPad - 18));
        return l;
    }

    int bodyHeight (int index, int w) const
    {
        const bool hasAction = help::action (items[(size_t) index].topic) != help::Action::none;
        return juce::roundToInt (layoutFor (index, w).getHeight()) + 2 * bodyPad + (hasAction ? actionKeyH + 12 : 0);
    }
};

HelpDialog::HelpDialog (UiSession& session)
    : DialogPanel (tr ("help.title"), tr ("help.micro")), list (std::make_unique<List> (session))
{
    list->onAction = [this] (help::Action a) { if (onAction) onAction (a); };
    list->onLayoutChanged = [this] { resized(); list->resized(); };   // 大きさが変わらなくても、開いた項目を並べ直す
    viewport.setViewedComponent (list.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    addFooterKey (tr ("common.close"), KeyRole::primary, [this] { if (onCloseRequest) onCloseRequest(); });
    setWantsKeyboardFocus (true);
    setSize (preferredW, preferredH);
}

HelpDialog::~HelpDialog() = default;

const std::vector<help::Item>& HelpDialog::shownItems() const { return list->items; }
int HelpDialog::openIndex() const { return list->open; }
void HelpDialog::openItem (int index) { list->setOpen (index); }

void HelpDialog::fitToParent()
{
    // 窓が小さいときは、ダイアログを窓に収めて一覧をスクロールする
    if (auto* p = getParentComponent())
        setSize (juce::jmin (preferredW, p->getWidth() - 40), juce::jmin (preferredH, p->getHeight() - 40));
}

void HelpDialog::layoutBody (juce::Rectangle<int> r)
{
    introArea = r.removeFromTop (40);
    r.removeFromTop (6);
    viewport.setBounds (r);
    const auto w = r.getWidth() - (list->heightFor (r.getWidth()) > r.getHeight() ? viewport.getScrollBarThickness() + 4 : 0);
    list->setSize (w, list->heightFor (w));
}

void HelpDialog::paintBody (juce::Graphics& g, juce::Rectangle<int>)
{
    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawFittedText (tr ("help.intro"), introArea, juce::Justification::topLeft, 2, 1.0f);
}
} // namespace vb
