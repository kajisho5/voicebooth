#include "Icons.h"

namespace vb
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float halfPi = juce::MathConstants<float>::halfPi;
    constexpr float strokeW = 2.0f;   // 24 グリッドでの線幅

    juce::Path outline (const juce::Path& p, float w = strokeW)
    {
        juce::Path out;
        juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (out, p);
        return out;
    }

    juce::Path polyline (std::initializer_list<juce::Point<float>> pts)
    {
        juce::Path p;
        bool first = true;
        for (auto pt : pts)
        {
            if (first) p.startNewSubPath (pt); else p.lineTo (pt);
            first = false;
        }
        return p;
    }

    juce::Path rounded (juce::Path p, float r) { return p.createPathWithRoundedCorners (r); }

    /** 24x24 グリッド上のアイコン */
    juce::Path design (Icon icon)
    {
        juce::Path p;

        switch (icon)
        {
            case Icon::play:
            {
                juce::Path t;
                t.addTriangle (8.0f, 5.0f, 8.0f, 19.0f, 19.5f, 12.0f);
                return rounded (t, 1.6f);
            }

            case Icon::pause:
                p.addRoundedRectangle (6.5f, 5.0f, 4.0f, 14.0f, 1.0f);
                p.addRoundedRectangle (13.5f, 5.0f, 4.0f, 14.0f, 1.0f);
                return p;

            case Icon::stop:
                p.addRoundedRectangle (6.0f, 6.0f, 12.0f, 12.0f, 1.6f);
                return p;

            case Icon::toStart:
            {
                p.addRoundedRectangle (5.0f, 5.5f, 2.6f, 13.0f, 0.8f);
                juce::Path t;
                t.addTriangle (19.0f, 5.5f, 19.0f, 18.5f, 9.0f, 12.0f);
                p.addPath (rounded (t, 1.4f));
                return p;
            }

            case Icon::rec:
                p.addEllipse (5.5f, 5.5f, 13.0f, 13.0f);
                return p;

            case Icon::loop:
            {
                juce::Path l = polyline ({ { 17.0f, 4.0f }, { 20.0f, 7.0f }, { 17.0f, 10.0f } });
                l.startNewSubPath (4.0f, 11.0f);
                l.lineTo (4.0f, 10.0f);
                l.addCentredArc (7.0f, 10.0f, 3.0f, 3.0f, 0.0f, pi * 1.5f, pi * 2.0f, false);
                l.lineTo (20.0f, 7.0f);
                l.startNewSubPath (7.0f, 20.0f);
                l.lineTo (4.0f, 17.0f);
                l.lineTo (7.0f, 14.0f);
                l.startNewSubPath (20.0f, 13.0f);
                l.lineTo (20.0f, 14.0f);
                l.addCentredArc (17.0f, 14.0f, 3.0f, 3.0f, 0.0f, halfPi, pi, false);
                l.lineTo (4.0f, 17.0f);
                return outline (l);
            }

            case Icon::rangeIn:
            {
                auto b = polyline ({ { 10.0f, 4.5f }, { 6.0f, 4.5f }, { 6.0f, 19.5f }, { 10.0f, 19.5f } });
                b.startNewSubPath (11.0f, 12.0f);
                b.lineTo (18.5f, 12.0f);
                auto o = outline (b);
                juce::Path t;
                t.addTriangle (18.0f, 8.5f, 18.0f, 15.5f, 21.0f, 12.0f);
                o.addPath (t);
                return o;
            }

            case Icon::rangeOut:
            {
                auto b = polyline ({ { 14.0f, 4.5f }, { 18.0f, 4.5f }, { 18.0f, 19.5f }, { 14.0f, 19.5f } });
                b.startNewSubPath (13.0f, 12.0f);
                b.lineTo (5.5f, 12.0f);
                auto o = outline (b);
                juce::Path t;
                t.addTriangle (6.0f, 8.5f, 6.0f, 15.5f, 3.0f, 12.0f);
                o.addPath (t);
                return o;
            }

            case Icon::close:
            {
                auto x = polyline ({ { 6.5f, 6.5f }, { 17.5f, 17.5f } });
                x.startNewSubPath (17.5f, 6.5f);
                x.lineTo (6.5f, 17.5f);
                return outline (x);
            }

            case Icon::metronome:
            {
                juce::Path m = polyline ({ { 9.0f, 3.5f }, { 15.0f, 3.5f }, { 19.0f, 20.5f }, { 5.0f, 20.5f } });
                m.closeSubPath();
                m.startNewSubPath (12.0f, 16.0f);
                m.lineTo (17.5f, 6.0f);
                m.startNewSubPath (7.5f, 16.5f);
                m.lineTo (16.5f, 16.5f);
                return outline (m, 1.8f);
            }

            case Icon::gear:
            {
                juce::Path ring;
                ring.addEllipse (6.0f, 6.0f, 12.0f, 12.0f);
                p = outline (ring, 3.4f);
                for (int i = 0; i < 8; ++i)
                {
                    juce::Path tooth;
                    tooth.addRoundedRectangle (10.5f, 1.6f, 3.0f, 4.6f, 0.8f);
                    tooth.applyTransform (juce::AffineTransform::rotation ((float) i * pi / 4.0f, 12.0f, 12.0f));
                    p.addPath (tooth);
                }
                return p;
            }

            case Icon::mic:
            {
                p.addRoundedRectangle (9.0f, 2.5f, 6.0f, 12.0f, 3.0f);
                juce::Path u;
                u.startNewSubPath (5.5f, 11.0f);
                u.addCentredArc (12.0f, 11.0f, 6.5f, 6.5f, 0.0f, pi * 1.5f, halfPi, false);
                u.startNewSubPath (12.0f, 17.5f);
                u.lineTo (12.0f, 21.0f);
                u.startNewSubPath (8.5f, 21.0f);
                u.lineTo (15.5f, 21.0f);
                p.addPath (outline (u, 1.8f));
                return p;
            }

            case Icon::headphones:
            {
                juce::Path band;
                band.startNewSubPath (4.0f, 15.0f);
                band.lineTo (4.0f, 12.0f);
                band.addCentredArc (12.0f, 12.0f, 8.0f, 8.0f, 0.0f, -halfPi, halfPi, false);
                band.lineTo (20.0f, 15.0f);
                p = outline (band, 1.8f);
                p.addRoundedRectangle (2.8f, 13.5f, 4.6f, 7.0f, 1.6f);
                p.addRoundedRectangle (16.6f, 13.5f, 4.6f, 7.0f, 1.6f);
                return p;
            }

            case Icon::edit:
            {
                juce::Path e = polyline ({ { 15.5f, 4.5f }, { 19.5f, 8.5f }, { 8.5f, 19.5f }, { 4.5f, 19.5f }, { 4.5f, 15.5f } });
                e.closeSubPath();
                e.startNewSubPath (13.0f, 7.0f);
                e.lineTo (17.0f, 11.0f);
                return outline (e, 1.8f);
            }

            case Icon::compare:
                p.addRoundedRectangle (3.0f, 5.5f, 13.0f, 4.5f, 2.0f);
                p.addRoundedRectangle (8.0f, 14.0f, 13.0f, 4.5f, 2.0f);
                return p;

            case Icon::lock:
            {
                p.addRoundedRectangle (5.0f, 10.5f, 14.0f, 10.0f, 2.0f);
                juce::Path s;
                s.startNewSubPath (8.0f, 10.5f);
                s.lineTo (8.0f, 8.0f);
                s.addCentredArc (12.0f, 8.0f, 4.0f, 4.0f, 0.0f, -halfPi, halfPi, false);
                s.lineTo (16.0f, 10.5f);
                p.addPath (outline (s, 2.0f));
                return p;
            }

            case Icon::chevronDown:
                return outline (polyline ({ { 6.5f, 9.5f }, { 12.0f, 15.0f }, { 17.5f, 9.5f } }));

            case Icon::chevronRight:
                return outline (polyline ({ { 9.5f, 6.5f }, { 15.0f, 12.0f }, { 9.5f, 17.5f } }));

            case Icon::exportFile:
            {
                auto e = polyline ({ { 4.5f, 14.0f }, { 4.5f, 19.5f }, { 19.5f, 19.5f }, { 19.5f, 14.0f } });
                e.startNewSubPath (12.0f, 15.0f);
                e.lineTo (12.0f, 4.5f);
                e.startNewSubPath (7.5f, 9.0f);
                e.lineTo (12.0f, 4.5f);
                e.lineTo (16.5f, 9.0f);
                return outline (e, 1.9f);
            }

            case Icon::download:   // exportFile の向きを逆に（受け皿へ下向き）
            {
                auto d = polyline ({ { 4.5f, 14.0f }, { 4.5f, 19.5f }, { 19.5f, 19.5f }, { 19.5f, 14.0f } });
                d.startNewSubPath (12.0f, 4.5f);
                d.lineTo (12.0f, 15.0f);
                d.startNewSubPath (7.5f, 10.5f);
                d.lineTo (12.0f, 15.0f);
                d.lineTo (16.5f, 10.5f);
                return outline (d, 1.9f);
            }

            case Icon::shield:     // 署名の確認
            {
                juce::Path sh;
                sh.startNewSubPath (12.0f, 3.5f);
                sh.lineTo (19.5f, 6.5f);
                sh.lineTo (19.0f, 12.5f);
                sh.quadraticTo (18.0f, 17.5f, 12.0f, 20.5f);
                sh.quadraticTo (6.0f, 17.5f, 5.0f, 12.5f);
                sh.lineTo (4.5f, 6.5f);
                sh.closeSubPath();
                p = outline (sh, 1.8f);
                p.addPath (outline (polyline ({ { 8.8f, 12.0f }, { 11.2f, 14.4f }, { 15.4f, 9.6f } }), 1.8f));
                return p;
            }

            case Icon::folder:
            {
                auto f = polyline ({ { 3.5f, 6.5f }, { 9.5f, 6.5f }, { 11.5f, 8.5f }, { 20.5f, 8.5f }, { 20.5f, 18.5f }, { 3.5f, 18.5f } });
                f.closeSubPath();
                return outline (f, 1.8f);
            }

            case Icon::note:
            {
                p.addEllipse (5.0f, 14.5f, 6.5f, 5.0f);
                p.addEllipse (13.5f, 12.5f, 6.5f, 5.0f);
                auto st = polyline ({ { 10.6f, 16.5f }, { 10.6f, 6.0f }, { 19.1f, 4.0f }, { 19.1f, 14.5f } });
                p.addPath (outline (st, 1.8f));
                return p;
            }

            case Icon::check:
                return outline (polyline ({ { 5.0f, 12.5f }, { 10.0f, 17.5f }, { 19.0f, 7.0f } }), 2.2f);

            case Icon::warning:
            {
                juce::Path t;
                t.addTriangle (12.0f, 3.5f, 21.0f, 19.5f, 3.0f, 19.5f);
                p = outline (rounded (t, 1.5f), 1.8f);
                p.addRoundedRectangle (11.0f, 9.0f, 2.0f, 5.5f, 1.0f);
                p.addEllipse (11.0f, 15.5f, 2.0f, 2.0f);
                return p;
            }

            case Icon::help:       // 困ったときのヘルプ：丸の中に ?
            {
                juce::Path q;
                q.addEllipse (3.5f, 3.5f, 17.0f, 17.0f);
                p = outline (q, 1.6f);
                juce::Path hook;
                hook.startNewSubPath (9.3f, 9.6f);
                hook.cubicTo (9.3f, 7.6f, 10.6f, 6.8f, 12.0f, 6.8f);
                hook.cubicTo (13.6f, 6.8f, 14.8f, 7.9f, 14.8f, 9.4f);
                hook.cubicTo (14.8f, 11.4f, 12.0f, 11.6f, 12.0f, 13.8f);
                p.addPath (outline (hook, 1.7f));
                p.addEllipse (11.0f, 15.6f, 2.0f, 2.0f);
                return p;
            }

            case Icon::globe:
            {
                juce::Path gl;
                gl.addEllipse (3.5f, 3.5f, 17.0f, 17.0f);
                gl.addEllipse (8.0f, 3.5f, 8.0f, 17.0f);
                gl.startNewSubPath (3.5f, 12.0f);
                gl.lineTo (20.5f, 12.0f);
                return outline (gl, 1.6f);
            }

            case Icon::minus:
                return outline (polyline ({ { 6.0f, 12.0f }, { 18.0f, 12.0f } }));

            case Icon::plus:
            {
                auto pl = polyline ({ { 6.0f, 12.0f }, { 18.0f, 12.0f } });
                pl.startNewSubPath (12.0f, 6.0f);
                pl.lineTo (12.0f, 18.0f);
                return outline (pl);
            }
        }

        return p;
    }
}

juce::Path makeIcon (Icon icon, juce::Rectangle<float> area)
{
    const auto s = juce::jmin (area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre (s, s);
    auto p = design (icon);
    p.applyTransform (juce::AffineTransform::scale (s / 24.0f).translated (box.getX(), box.getY()));
    return p;
}

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour c)
{
    g.setColour (c);
    g.fillPath (makeIcon (icon, area));
}
} // namespace vb
