#include "ShareVideo.h"
#include "export/VideoEncoder.h"

namespace vb::share
{
namespace
{
    using T = skin::Token;

    juce::Colour col (const Scene& s, T t) { return juce::Colour (s.colours[(size_t) t]); }

    /** 形ごとの置き場所と字の大きさ（px。コマの大きさは決まっているので実寸で組む） */
    struct Layout
    {
        juce::Rectangle<float> title, pitch, lyrics, wave, clock;
        float titleSize = 60.0f, lyricSize = 60.0f, nextSize = 40.0f, clockSize = 32.0f, radius = 24.0f;
        int titleLines = 2;
        juce::Justification titleJust = juce::Justification::centred;
        juce::Justification clockJust = juce::Justification::centred;
        double windowSeconds = 4.0;   // 音程の欄に見せる長さ
        float playheadAt = 0.38f;     // 再生位置の線（欄の左からの割合）
    };

    /** 真ん中の欄（音程・歌詞）を top〜bottom に分ける。片方しかなければ全部をその欄に */
    void splitMiddle (Layout& l, const Scene& s, float x, float w, float top, float bottom, float pitchShare)
    {
        const auto pitch = s.hasPitch(), lyrics = s.hasLyrics();
        const auto h = bottom - top;
        if (pitch && lyrics)
        {
            const auto ph = std::round (h * pitchShare);
            l.pitch = { x, top, w, ph };
            l.lyrics = { x, top + ph + 24.0f, w, h - ph - 24.0f };
        }
        else if (pitch)
            l.pitch = { x, top, w, h };
        else if (lyrics)
        {
            l.lyrics = { x, top, w, h };
            l.lyricSize *= 1.15f;
            l.nextSize *= 1.15f;
        }
    }

    Layout layoutFor (const Scene& s)
    {
        const auto size = frameSize (s.shape);
        const auto W = (float) size.x;
        Layout l;
        switch (s.shape)
        {
            case Shape::portrait:   // 1080×1920
            {
                const float m = 72.0f, w = W - 2.0f * m;
                l.title = { m, 150.0f, w, 190.0f };
                l.titleSize = 64.0f;
                l.lyricSize = 64.0f;
                l.nextSize = 42.0f;
                l.wave = { m, 1650.0f, w, 96.0f };
                l.clock = { m, 1766.0f, w, 48.0f };
                l.clockSize = 34.0f;
                l.radius = 28.0f;
                splitMiddle (l, s, m, w, 430.0f, 1580.0f, 0.6f);
                l.windowSeconds = 4.0;
                break;
            }
            case Shape::square:     // 1080×1080
            {
                const float m = 64.0f, w = W - 2.0f * m;
                l.title = { m, 52.0f, w, 116.0f };
                l.titleSize = 50.0f;
                l.lyricSize = 50.0f;
                l.nextSize = 34.0f;
                l.wave = { m, 930.0f, w, 60.0f };
                l.clock = { m, 1000.0f, w, 40.0f };
                l.clockSize = 28.0f;
                l.radius = 22.0f;
                splitMiddle (l, s, m, w, 214.0f, 900.0f, 0.56f);
                l.windowSeconds = 5.0;
                break;
            }
            case Shape::landscape:  // 1920×1080
            {
                const float m = 96.0f, w = W - 2.0f * m;
                l.title = { m, 56.0f, w, 84.0f };
                l.titleSize = 52.0f;
                l.titleLines = 1;
                l.titleJust = juce::Justification::centredLeft;
                l.lyricSize = 58.0f;
                l.nextSize = 38.0f;
                l.wave = { m, 950.0f, w - 240.0f, 56.0f };
                l.clock = { W - m - 220.0f, 950.0f, 220.0f, 56.0f };
                l.clockSize = 34.0f;
                l.clockJust = juce::Justification::centredRight;
                l.radius = 24.0f;
                splitMiddle (l, s, m, w, 184.0f, 920.0f, 0.62f);
                l.windowSeconds = 8.0;
                l.playheadAt = 0.35f;
                break;
            }
        }
        return l;
    }

    /** 1 行に収まる大きさまで字を小さくする（元の半分まで） */
    juce::Font fitted (const juce::String& text, float size, float maxWidth, Weight weight)
    {
        auto f = sansForExact (text, size, weight);
        const auto w = textWidth (f, text);
        if (w > maxWidth && w > 0.0f)
            f = sansForExact (text, size * juce::jmax (0.5f, maxWidth / w), weight);
        return f;
    }

    const Scene::Lyric* lyricAt (const Scene& s, int64 t)
    {
        const Scene::Lyric* found = nullptr;
        for (auto& l : s.lyrics)
        {
            if (l.start > t)
                break;
            if (t < l.end)
                found = &l;
        }
        return found;
    }

    const Scene::Lyric* lyricAfter (const Scene& s, int64 t)
    {
        for (auto& l : s.lyrics)
            if (l.start > t)
                return &l;
        return nullptr;
    }

    void paintPitch (juce::Graphics& g, const Scene& s, const Layout& l, int64 t)
    {
        const auto plot = l.pitch.reduced (28.0f, 32.0f);
        const auto span = juce::jmax (1.0f, s.highMidi - s.lowMidi);
        const auto win = l.windowSeconds * s.sampleRate;
        const auto t0 = (double) t - win * l.playheadAt;
        auto xOf = [&] (int64 x) { return plot.getX() + (float) (((double) x - t0) / win) * plot.getWidth(); };
        auto yOf = [&] (float midi) { return plot.getBottom() - (midi - s.lowMidi) / span * plot.getHeight(); };
        const auto semitone = plot.getHeight() / span;

        // 半音の罫線（C は少し強く）
        for (int m = (int) std::ceil (s.lowMidi); m <= (int) std::floor (s.highMidi); ++m)
        {
            g.setColour (col (s, T::line).withAlpha (m % 12 == 0 ? 0.55f : 0.18f));
            g.fillRect (juce::Rectangle<float> (plot.getX(), yOf ((float) m) - 0.5f, plot.getWidth(), m % 12 == 0 ? 1.5f : 1.0f));
        }

        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (l.pitch.reduced (6.0f).toNearestInt());

        // お手本の音符（いま鳴っている音符は明るく）
        const auto barH = juce::jlimit (8.0f, 22.0f, semitone * 0.62f);
        const auto end = (int64) (t0 + win);
        for (auto& n : s.guide)
        {
            if (n.end < (int64) t0 || n.start > end)
                continue;
            const auto active = n.start <= t && t < n.end;
            const auto x0 = xOf (n.start), x1 = juce::jmax (x0 + barH, xOf (n.end));
            g.setColour (col (s, T::ref).withAlpha (active ? 0.95f : (n.end <= t ? 0.4f : 0.62f)));
            g.fillRoundedRectangle (juce::Rectangle<float> (x0, yOf (n.midi) - barH * 0.5f, x1 - x0, barH), barH * 0.5f);
        }

        // 自分の線（いまの位置まで）
        const auto maxGap = (int64) (0.06 * s.sampleRate);
        juce::Path path;
        bool open = false;
        int64 last = 0;
        float lastMidi = 0.0f;
        const Scene::Voice* now = nullptr;
        auto it = std::lower_bound (s.voice.begin(), s.voice.end(), (int64) t0,
                                    [] (const Scene::Voice& v, int64 x) { return v.sample < x; });
        for (; it != s.voice.end() && it->sample <= t; ++it)
        {
            if (it->midi <= 0.0f)
            {
                open = false;
                continue;
            }
            const juce::Point<float> pt { xOf (it->sample), yOf (it->midi) };
            if (! open || it->sample - last > maxGap || std::abs (it->midi - lastMidi) > 4.0f)
                path.startNewSubPath (pt);
            else
                path.lineTo (pt);
            open = true;
            last = it->sample;
            lastMidi = it->midi;
            now = &*it;
        }
        const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
        g.setColour (col (s, T::signal).withAlpha (0.22f));
        g.strokePath (path, stroke (16.0f));
        g.setColour (col (s, T::signal));
        g.strokePath (path, stroke (5.5f));

        // 再生位置の線と、いまの声の点
        const auto x = xOf (t);
        g.setColour (col (s, T::text).withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (x - 1.0f, l.pitch.getY() + 12.0f, 2.0f, l.pitch.getHeight() - 24.0f));
        if (now != nullptr && t - now->sample < (int64) (0.08 * s.sampleRate))
        {
            const juce::Point<float> c { x, yOf (now->midi) };
            g.setColour (col (s, T::signal).withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (44.0f, 44.0f).withCentre (c));
            g.setColour (col (s, T::signal));
            g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (c));
        }
    }

    void paintLyrics (juce::Graphics& g, const Scene& s, const Layout& l, int64 t)
    {
        const auto area = l.lyrics;
        const auto* cur = lyricAt (s, t);
        const auto* next = lyricAfter (s, t);
        const auto lineH = l.lyricSize * 1.35f, nextH = l.nextSize * 1.35f, gap = l.lyricSize * 0.55f;
        auto block = juce::Rectangle<float> (area.getX(), 0.0f, area.getWidth(), lineH + gap + nextH).withCentre (area.getCentre());
        auto top = block.removeFromTop (lineH);
        block.removeFromTop (gap);
        auto bottom = block;

        if (cur != nullptr)
        {
            const auto f = fitted (cur->text, l.lyricSize, area.getWidth(), Weight::semibold);
            const auto w = juce::jmin (area.getWidth(), textWidth (f, cur->text) + 4.0f);
            const auto line = top.withSizeKeepingCentre (w, lineH);
            const auto progress = juce::jlimit (0.0f, 1.0f, (float) (t - cur->start) / (float) juce::jmax ((int64) 1, cur->end - cur->start));
            const auto split = line.getX() + line.getWidth() * progress;

            g.setFont (f);
            g.setColour (col (s, T::text));
            g.drawText (cur->text, line, juce::Justification::centred, true);
            {
                juce::Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (line.withRight (split).getSmallestIntegerContainer());
                g.setColour (col (s, T::signal));
                g.drawText (cur->text, line, juce::Justification::centred, true);
            }
            const auto uy = line.getBottom() + 6.0f;
            g.setColour (col (s, T::line).withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (line.getX(), uy, line.getWidth(), 3.0f));
            g.setColour (col (s, T::signal));
            g.fillRect (juce::Rectangle<float> (line.getX(), uy, split - line.getX(), 3.0f));
        }
        else if (next != nullptr)
        {
            // フレーズの合間：次の行を大きめに待たせる（歌詞レーンと同じ）
            g.setFont (fitted (next->text, l.lyricSize * 0.85f, area.getWidth(), Weight::medium));
            g.setColour (col (s, T::textDim));
            g.drawText (next->text, top, juce::Justification::centred, true);
            next = lyricAfter (s, next->start);
        }

        if (next != nullptr)
        {
            // 次の行の 1.5 秒前から少しずつ明るく
            const auto lead = (double) (next->start - t) / juce::jmax (1.0, s.sampleRate);
            const auto soon = cur != nullptr ? juce::jlimit (0.0f, 1.0f, (float) (1.0 - lead / 1.5)) : 0.0f;
            g.setFont (fitted (next->text, l.nextSize, area.getWidth(), Weight::regular));
            g.setColour (col (s, T::textDim).interpolatedWith (col (s, T::text), soon * 0.7f));
            g.drawText (next->text, bottom, juce::Justification::centred, true);
        }
    }

    void paintWave (juce::Graphics& g, const Scene& s, const Layout& l, int64 t)
    {
        const auto total = juce::jmax<int64> (1, s.to - s.from);
        const auto progress = juce::jlimit (0.0f, 1.0f, (float) (t - s.from) / (float) total);
        const auto n = (int) s.wave.size();
        if (n > 0)
        {
            const auto bw = l.wave.getWidth() / (float) n;
            const auto barW = juce::jmax (2.0f, bw * 0.62f);
            for (int i = 0; i < n; ++i)
            {
                const auto h = juce::jmax (5.0f, s.wave[(size_t) i] * l.wave.getHeight());
                const auto x = l.wave.getX() + bw * (float) i + (bw - barW) * 0.5f;
                const auto played = ((float) i + 0.5f) / (float) n <= progress;
                g.setColour (played ? col (s, T::signal) : col (s, T::textMute).withAlpha (0.55f));
                g.fillRoundedRectangle (juce::Rectangle<float> (x, l.wave.getCentreY() - h * 0.5f, barW, h), barW * 0.5f);
            }
        }
        g.setFont (monoExact (l.clockSize, Weight::medium));
        g.setColour (col (s, T::textDim));
        g.drawText (clockText ((double) (t - s.from) / s.sampleRate) + " / " + clockText ((double) total / s.sampleRate),
                    l.clock, l.clockJust, false);
    }
} // namespace

juce::Point<int> frameSize (Shape shape)
{
    switch (shape)
    {
        case Shape::portrait:  return { 1080, 1920 };
        case Shape::square:    return { 1080, 1080 };
        case Shape::landscape: return { 1920, 1080 };
    }
    return { 1080, 1920 };
}

int waveBinCount (Shape shape)
{
    switch (shape)
    {
        case Shape::portrait:  return 72;
        case Shape::square:    return 72;
        case Shape::landscape: return 120;
    }
    return 72;
}

void Scene::fitPitchRange()
{
    float lo = 1000.0f, hi = -1000.0f;
    for (auto& n : guide)
        if (n.end > from && n.start < to && n.midi > 0.0f)
        {
            lo = juce::jmin (lo, n.midi);
            hi = juce::jmax (hi, n.midi);
        }
    for (auto& v : voice)
        if (v.sample >= from && v.sample < to && v.midi > 0.0f)
        {
            lo = juce::jmin (lo, v.midi);
            hi = juce::jmax (hi, v.midi);
        }
    if (lo > hi)
    {
        lowMidi = 48.0f;
        highMidi = 72.0f;
        return;
    }
    lowMidi = std::floor (lo) - 2.0f;
    highMidi = std::ceil (hi) + 2.0f;
    if (highMidi - lowMidi < 12.0f)
    {
        const auto c = (lowMidi + highMidi) * 0.5f;
        lowMidi = std::floor (c - 6.0f);
        highMidi = lowMidi + 12.0f;
    }
}

juce::Image paintStatic (const Scene& s)
{
    const auto size = frameSize (s.shape);
    juce::Image img (juce::Image::ARGB, size.x, size.y, false, juce::SoftwareImageType());
    juce::Graphics g (img);
    const auto all = img.getBounds().toFloat();
    const auto l = layoutFor (s);

    // 背景：画像があれば全面に敷いて暗くする（文字を読めるように）。なければスキンの地の色を上から下へ
    g.setColour (col (s, T::bgDeep));
    g.fillAll();
    if (s.background.isValid())
    {
        const auto iw = (float) s.background.getWidth(), ih = (float) s.background.getHeight();
        const auto scale = juce::jmax (all.getWidth() / iw, all.getHeight() / ih);
        const auto dest = juce::Rectangle<float> (iw * scale, ih * scale).withCentre (all.getCentre());
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (s.background, dest);
        g.setColour (col (s, T::bgDeep).withAlpha (0.62f));
        g.fillAll();
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (col (s, T::bg0), 0.0f, 0.0f, col (s, T::bgDeep), 0.0f, all.getHeight(), false));
        g.fillAll();
        // 左上にお手本の色をうっすら（平らに見えないように）
        g.setGradientFill (juce::ColourGradient (col (s, T::ref).withAlpha (0.08f), 0.0f, 0.0f,
                                                 col (s, T::ref).withAlpha (0.0f), all.getWidth() * 0.7f, all.getHeight() * 0.45f, true));
        g.fillAll();
    }

    // 曲名と、その下の短い線
    g.setColour (col (s, T::text));
    g.setFont (sansForExact (s.title, l.titleSize, Weight::semibold));
    g.drawFittedText (s.title, l.title.toNearestInt(), l.titleJust, l.titleLines, 0.8f);
    const auto accentW = 88.0f;
    const auto ax = l.titleJust == juce::Justification::centred ? l.title.getCentreX() - accentW * 0.5f : l.title.getX();
    g.setColour (col (s, T::signal));
    g.fillRoundedRectangle (juce::Rectangle<float> (ax, l.title.getBottom() + 14.0f, accentW, 5.0f), 2.5f);

    // 音程の欄の枠
    if (! l.pitch.isEmpty())
    {
        g.setColour (col (s, T::bgDeep).withAlpha (0.6f));
        g.fillRoundedRectangle (l.pitch, l.radius);
        g.setColour (col (s, T::line).withAlpha (0.7f));
        g.drawRoundedRectangle (l.pitch.reduced (0.75f), l.radius, 1.5f);
    }
    return img;
}

void paintFrame (juce::Graphics& g, const Scene& s, const juce::Image& still, int64 t)
{
    g.drawImageAt (still, 0, 0);
    const auto l = layoutFor (s);
    if (! l.pitch.isEmpty())
        paintPitch (g, s, l, t);
    if (! l.lyrics.isEmpty())
        paintLyrics (g, s, l, t);
    paintWave (g, s, l, t);
}

juce::String write (const Scene& scene, std::shared_ptr<const juce::AudioBuffer<float>> audio, const juce::File& dest, int fps,
                    const std::function<bool (float)>& progress)
{
    const auto size = frameSize (scene.shape);
    video::Spec spec;
    spec.width = size.x;
    spec.height = size.y;
    spec.fps = fps;
    spec.videoBitrate = scene.shape == Shape::square ? 8000000 : 10000000;
    auto encoder = video::Encoder::create();
    if (auto e = encoder->open (dest, spec, audio); e.isNotEmpty())
        return e;

    const auto frames = video::frameCount (scene.seconds(), fps);
    const auto still = paintStatic (scene);
    juce::Image img (juce::Image::ARGB, size.x, size.y, false, juce::SoftwareImageType());
    for (int64 i = 0; i < frames; ++i)
    {
        {
            juce::Graphics g (img);
            paintFrame (g, scene, still, scene.from + (int64) std::llround ((double) i * scene.sampleRate / fps));
        }
        {
            const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);
            if (auto e = encoder->addFrame (data.data, data.lineStride); e.isNotEmpty())
                return e;
        }
        if (progress && ! progress ((float) (i + 1) / (float) juce::jmax<int64> (1, frames)))
            return "cancelled";   // encoder を捨てると書きかけのファイルも消える
    }
    return encoder->finish();
}

std::vector<float> waveBins (const juce::AudioBuffer<float>& audio, int bins)
{
    std::vector<float> out ((size_t) juce::jmax (0, bins), 0.0f);
    const auto n = audio.getNumSamples(), ch = audio.getNumChannels();
    if (bins <= 0 || n <= 0 || ch <= 0)
        return out;
    float top = 0.0f;
    for (int b = 0; b < bins; ++b)
    {
        const auto a = (int) ((juce::int64) n * b / bins), e = (int) ((juce::int64) n * (b + 1) / bins);
        double sum = 0.0;
        for (int c = 0; c < ch; ++c)
        {
            const auto* p = audio.getReadPointer (c);
            for (int i = a; i < e; ++i)
                sum += (double) p[i] * p[i];
        }
        const auto rms = e > a ? (float) std::sqrt (sum / ((double) (e - a) * ch)) : 0.0f;
        out[(size_t) b] = rms;
        top = juce::jmax (top, rms);
    }
    if (top > 0.0f)
        for (auto& v : out)
            v = std::pow (v / top, 0.8f);
    return out;
}

juce::String fileName (const juce::String& song, Shape shape)
{
    auto safe = juce::File::createLegalFileName (song).trim();
    if (safe.isEmpty() || safe.containsOnly (". "))
        safe = "Untitled";
    if (safe.length() > 60)
        safe = safe.substring (0, 60).trimEnd();
    switch (shape)
    {
        case Shape::portrait:  return safe + "_9x16.mp4";
        case Shape::square:    return safe + "_1x1.mp4";
        case Shape::landscape: return safe + "_16x9.mp4";
    }
    return safe + ".mp4";
}

juce::String clockText (double seconds)
{
    const auto total = (int) std::floor (juce::jmax (0.0, seconds) + 1.0e-6);
    const auto h = total / 3600, m = (total / 60) % 60, sec = total % 60;
    const auto ss = juce::String (sec).paddedLeft ('0', 2);
    return h > 0 ? juce::String (h) + ":" + juce::String (m).paddedLeft ('0', 2) + ":" + ss
                 : juce::String (m) + ":" + ss;
}
} // namespace vb::share
