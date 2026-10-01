#include "SkinTemplates.h"

namespace vb
{
namespace
{
    using skin::Token;

    constexpr int columns = 5, tileW = 164, thumbH = 100, tileH = thumbH + 44, gap = 12, sectionH = 24;
    constexpr int gridW = columns * tileW + (columns - 1) * gap;
    constexpr int maxViewH = 500, margin = 5;   // margin：ホバーの縁が切れないように

    juce::String subtitleOf (const skin::Skin& s)
    {
        if (s.builtIn)
            return tr (skin::subtitleKey (s.id).toRawUTF8());
        return s.author.isNotEmpty() ? s.author : tr ("settings.skin.mine");
    }
}

//==============================================================================
void paintSkinThumbnail (juce::Graphics& g, juce::Rectangle<float> r, const skin::Skin& s)
{
    auto c = [&s] (Token t) { return juce::Colour (s.get (t)); };

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, 4.0f);
    g.reduceClipRegion (clip);

    g.fillAll (c (Token::bg0));
    auto a = r;
    const auto unit = r.getHeight() / 100.0f;

    // トップバー（ロゴのタリー＝REC の点、曲名）
    auto top = a.removeFromTop (12.0f * unit);
    g.setColour (c (Token::panel));
    g.fillRect (top);
    g.setColour (c (Token::line));
    g.fillRect (top.removeFromBottom (1.0f));
    g.setColour (c (Token::rec));
    g.fillEllipse (juce::Rectangle<float> (5.0f * unit, 5.0f * unit).withCentre ({ top.getX() + 7.0f * unit, top.getCentreY() }));
    g.setColour (c (Token::text));
    g.fillRoundedRectangle ({ top.getX() + 13.0f * unit, top.getCentreY() - 1.5f * unit, 22.0f * unit, 3.0f * unit }, 1.0f);
    g.setColour (c (Token::textDim));
    g.fillRoundedRectangle ({ top.getX() + 38.0f * unit, top.getCentreY() - 1.5f * unit, 14.0f * unit, 3.0f * unit }, 1.0f);

    // ステータスバー
    auto status = a.removeFromBottom (7.0f * unit);
    g.setColour (c (Token::panel));
    g.fillRect (status);
    g.setColour (c (Token::textMute));
    g.fillRoundedRectangle ({ status.getX() + 5.0f * unit, status.getCentreY() - 1.0f * unit, 20.0f * unit, 2.0f * unit }, 1.0f);

    // 右のラック：キー・メーター
    auto rack = a.removeFromRight (r.getWidth() * 0.27f);
    g.setColour (c (Token::panel));
    g.fillRect (rack);
    g.setColour (c (Token::line));
    g.fillRect (rack.removeFromLeft (1.0f));
    rack = rack.reduced (5.0f * unit, 5.0f * unit);

    auto key = rack.removeFromTop (9.0f * unit);
    g.setColour (c (Token::raised));
    g.fillRoundedRectangle (key, 2.0f);
    g.setColour (c (Token::signal));
    g.fillEllipse (juce::Rectangle<float> (3.0f * unit, 3.0f * unit).withCentre ({ key.getX() + 4.5f * unit, key.getCentreY() }));
    g.setColour (c (Token::text));
    g.fillRoundedRectangle ({ key.getX() + 9.0f * unit, key.getCentreY() - 1.0f * unit, key.getWidth() * 0.45f, 2.0f * unit }, 1.0f);

    rack.removeFromTop (5.0f * unit);
    auto meter = rack.removeFromTop (rack.getHeight() * 0.8f).withWidth (7.0f * unit);
    g.setColour (c (Token::bgDeep));
    g.fillRoundedRectangle (meter, 1.5f);
    const int segs = 10;
    const auto segH = meter.getHeight() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const auto col = i >= 9 ? c (Token::bad) : (i >= 7 ? c (Token::warn) : c (Token::signal));
        const auto lit = i < 7;
        g.setColour (lit ? col : col.interpolatedWith (c (Token::bgDeep), 0.8f));
        g.fillRect (juce::Rectangle<float> (meter.getX() + 1.0f, meter.getBottom() - (float) (i + 1) * segH + 0.5f,
                                            meter.getWidth() - 2.0f, segH - 1.0f));
    }

    // つまみ
    const auto knob = juce::Rectangle<float> (13.0f * unit, 13.0f * unit).withCentre ({ meter.getRight() + (rack.getRight() - meter.getRight()) * 0.5f,
                                                                                        meter.getY() + 9.0f * unit });
    g.setColour (c (Token::raisedHi));
    g.fillEllipse (knob);
    g.setColour (c (Token::signal));
    g.drawEllipse (knob.expanded (2.0f * unit), 1.0f);

    // キャンバス：ピッチレーン（お手本の帯＋自分の線）と歌詞
    auto canvas = a.reduced (4.0f * unit, 4.0f * unit);
    auto lyrics = canvas.removeFromBottom (9.0f * unit);
    auto lane = canvas.withTrimmedBottom (2.0f * unit);
    g.setColour (c (Token::bgDeep));
    g.fillRoundedRectangle (lane, 2.0f);

    g.setColour (c (Token::grid));
    for (int i = 1; i < 4; ++i)
        g.fillRect (juce::Rectangle<float> (lane.getX(), lane.getY() + lane.getHeight() * (float) i / 4.0f, lane.getWidth(), 1.0f));

    // お手本の帯（階段状）
    const float steps[] = { 0.62f, 0.62f, 0.45f, 0.45f, 0.30f, 0.38f, 0.38f, 0.55f };
    const auto stepW = lane.getWidth() / (float) std::size (steps);
    const auto bandH = lane.getHeight() * 0.12f;
    for (size_t i = 0; i < std::size (steps); ++i)
    {
        const auto band = juce::Rectangle<float> (lane.getX() + stepW * (float) i, lane.getY() + lane.getHeight() * steps[i] - bandH * 0.5f,
                                                  stepW - 1.0f, bandH);
        g.setColour (c (Token::ref).withAlpha (0.28f));
        g.fillRect (band);
        g.setColour (c (Token::ref).withAlpha (0.8f));
        g.fillRect (band.withHeight (1.0f));
        g.fillRect (band.withY (band.getBottom() - 1.0f).withHeight (1.0f));
    }

    // 自分の線：合っている → 少しずれ → 大きくずれ → 合っている
    auto yAt = [&] (float x) { return lane.getY() + lane.getHeight() * steps[juce::jlimit (0, (int) std::size (steps) - 1, (int) ((x - lane.getX()) / stepW))]; };
    auto segment = [&] (float from, float to, float offset, juce::Colour col)
    {
        juce::Path p;
        bool first = true;
        for (float x = lane.getX() + lane.getWidth() * from; x <= lane.getX() + lane.getWidth() * to; x += 1.0f)
        {
            const auto y = yAt (x) + offset * lane.getHeight() + std::sin (x * 0.35f) * 0.8f;
            if (first) p.startNewSubPath (x, y); else p.lineTo (x, y);
            first = false;
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    segment (0.02f, 0.30f, 0.0f, c (Token::signal));
    segment (0.33f, 0.48f, -0.08f, c (Token::warn));
    segment (0.51f, 0.62f, 0.16f, c (Token::bad));
    segment (0.66f, 0.80f, 0.0f, c (Token::signal));

    // 再生ヘッド
    g.setColour (c (Token::signal));
    g.fillRect (juce::Rectangle<float> (lane.getX() + lane.getWidth() * 0.80f, lane.getY(), 1.0f, lane.getHeight()));

    // 歌詞（歌ったところは文字色、先は控えめ）
    lyrics.removeFromTop (2.0f * unit);
    const auto ly = lyrics.getCentreY() - 1.5f * unit;
    g.setColour (c (Token::text));
    g.fillRoundedRectangle ({ lyrics.getX(), ly, lyrics.getWidth() * 0.38f, 3.0f * unit }, 1.0f);
    g.setColour (c (Token::textDim));
    g.fillRoundedRectangle ({ lyrics.getX() + lyrics.getWidth() * 0.42f, ly, lyrics.getWidth() * 0.30f, 3.0f * unit }, 1.0f);

    g.setColour (c (Token::lineHi));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

//==============================================================================
/** 縮図のタイルを並べる（多ければスクロール） */
class SkinTemplates::Grid : public juce::Component
{
public:
    Grid (std::vector<skin::Skin> t, juce::String active) : templates (std::move (t)), activeId (std::move (active))
    {
        int builtIns = 0;
        for (auto& s : templates)
            if (s.builtIn) ++builtIns;

        auto y = margin;
        auto place = [&] (int count)
        {
            sections.push_back (y);
            y += sectionH;
            for (int i = 0; i < count; ++i)
            {
                const auto col = i % columns, row = i / columns;
                tiles.push_back ({ margin + col * (tileW + gap), y + row * (tileH + gap), tileW, tileH });
            }
            y += ((count + columns - 1) / columns) * (tileH + gap);
        };
        place (builtIns);                                   // 内蔵
        if ((int) templates.size() > builtIns)
            place ((int) templates.size() - builtIns);      // 自作

        setSize (gridW + 2 * margin, y - gap + margin);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (const skin::Skin&)> onChosen;

    void paint (juce::Graphics& g) override
    {
        paint::sectionHeader (g, { margin, sections[0], gridW, sectionH - 6 }, tr ("skinTemplates.builtIn.micro"), tr ("skinTemplates.builtIn"));
        if (sections.size() > 1)
            paint::sectionHeader (g, { margin, sections[1], gridW, sectionH - 6 }, tr ("skinTemplates.mine.micro"), tr ("skinTemplates.mine"));

        for (size_t i = 0; i < templates.size(); ++i)
        {
            const auto& s = templates[i];
            auto r = tiles[i].toFloat();

            if ((int) i == hover)
            {
                g.setColour (colours::raisedHi);
                g.fillRoundedRectangle (r.expanded (4.0f), 6.0f);
                g.setColour (colours::lineHi);
                g.drawRoundedRectangle (r.expanded (3.5f), 6.0f, 1.0f);
            }

            paintSkinThumbnail (g, r.removeFromTop ((float) thumbH), s);
            r.removeFromTop (6.0f);

            auto nameRow = r.removeFromTop (19.0f);
            g.setColour (colours::text);
            g.setFont (sans (13.0f, Weight::medium));
            g.drawText (s.name, nameRow, juce::Justification::centredLeft, true);
            if (s.id == activeId)
            {
                // いま使っているスキン：名前のすぐ後ろに LED と「使用中」
                auto rest = nameRow.withTrimmedLeft (textWidth (g.getCurrentFont(), s.name) + 10.0f);
                paint::led (g, { rest.getX() + 3.0f, rest.getCentreY() }, 3.0f, colours::signal, true);
                g.setColour (colours::textDim);
                g.setFont (sans (11.0f));
                g.drawText (tr ("skinTemplates.current"), rest.withTrimmedLeft (11.0f), juce::Justification::centredLeft, true);
            }

            g.setColour (colours::textDim);
            g.setFont (sans (11.0f));
            g.drawText (subtitleOf (s), r.removeFromTop (16.0f), juce::Justification::centredLeft, true);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto h = tileAt (e.getPosition());
        if (h != hover) { hover = h; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (const auto i = tileAt (e.getPosition()); i >= 0 && e.mouseWasClicked() && onChosen)
            onChosen (templates[(size_t) i]);
    }

private:
    int tileAt (juce::Point<int> p) const
    {
        for (size_t i = 0; i < tiles.size(); ++i)
            if (tiles[i].expanded (4).contains (p))
                return (int) i;
        return -1;
    }

    std::vector<skin::Skin> templates;
    juce::String activeId;
    std::vector<juce::Rectangle<int>> tiles;
    std::vector<int> sections;
    int hover = -1;
};

//==============================================================================
SkinTemplates::SkinTemplates (std::vector<skin::Skin> templates, const juce::String& activeId)
    : DialogPanel (tr ("skinTemplates.title"), tr ("skinTemplates.micro"))
{
    grid = std::make_unique<Grid> (std::move (templates), activeId);
    grid->onChosen = [this] (const skin::Skin& s) { if (onChosen) onChosen (s); };

    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    addFooterKey (tr ("common.cancel"), KeyRole::normal, [this] { if (onCloseRequest) onCloseRequest(); });

    const auto viewH = juce::jmin (maxViewH, grid->getHeight());
    setSize (grid->getWidth() + 2 * padding + 10, headerH + 14 + 26 + viewH + footerH + 6);
}

SkinTemplates::~SkinTemplates() = default;

void SkinTemplates::layoutBody (juce::Rectangle<int> r)
{
    r.removeFromTop (26);
    viewport.setBounds (r.withX (r.getX() - margin).withWidth (r.getWidth() + margin + 10));
}

void SkinTemplates::paintBody (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (colours::textDim);
    g.setFont (sans (12.5f));
    g.drawText (tr ("skinTemplates.hint"), r.removeFromTop (18), juce::Justification::centredLeft, true);
}
} // namespace vb
