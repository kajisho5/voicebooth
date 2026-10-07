#include "Skin.h"

#include <cmath>

namespace vb::skin
{
namespace
{
    constexpr const char* keys[numTokens] = {
        "bg0", "bgDeep", "panel", "raised", "raisedHi", "grid", "line", "lineHi",
        "text", "textDim", "textMute",
        "signal", "ref", "warn", "bad", "rec"
    };

    /** 並びは Token と同じ：bg0 bgDeep panel raised raisedHi grid line lineHi / text textDim textMute / signal ref warn bad rec */
    Skin make (const char* id, const char* name, Colours c)
    {
        Skin s;
        s.id = id;
        s.name = name;
        s.author = "VoiceBooth";
        s.base = id;
        s.colours = c;
        s.builtIn = true;
        return s;
    }

    std::vector<Skin> createBuiltIns()
    {
        return {
            // 既定。DESIGN 4.9 そのもの（夜の録音ブース：暖色グラファイト＋LED＋タリー）
            make ("booth", "Booth", {
                0xff141311, 0xff0d0c0b, 0xff1b1a17, 0xff262420, 0xff302d28, 0xff24221e, 0xff34312b, 0xff4a463e,
                0xfff2ede3, 0xffa9a295, 0xff878278,
                0xffc6ee6a, 0xff8cc1ee, 0xfff4b942, 0xffff6b5e, 0xffff3b30 }),

            // 明るい生成り。表示窓（bgDeep）は地より明るい紙の白にして、深い色の線を読みやすくする。
            // 段差は「パネル・キーほど白い」で表す（暗いスキンの「浮くほど明るい」と同じ向き）
            make ("studio-day", "Studio Day", {
                0xffece8e0, 0xfff8f6f1, 0xfff4f1eb, 0xfffbfaf7, 0xffffffff, 0xffdcd7cd, 0xffcec8bc, 0xffb2ab9d,
                0xff1e1c18, 0xff5a544a, 0xff6e685e,
                0xff5e8f00, 0xff2f7cc0, 0xffb97800, 0xffd23b2e, 0xffe0261c }),

            // 白とピンク。明るいスキン（Studio Day と同じく表示窓は地より白い）。
            // 合っている＝ピンク、bad はピンクに紛れないよう紫、rec は赤のまま
            make ("sweet", "Sweet", {
                0xfffbf4f7, 0xffffffff, 0xfff6eaf0, 0xffffffff, 0xfffff7fb, 0xfff0dee7, 0xffe6ccd9, 0xffd3afc1,
                0xff3a2530, 0xff6a4d5b, 0xff7e6470,
                0xffd63f86, 0xff3e8fd6, 0xffb07a00, 0xff7b3fd1, 0xffe0263a }),

            // 青みの黒。中間色も同じ青みでそろえる（ミント＋ラベンダーは使わない）
            make ("midnight", "Midnight", {
                0xff0f1218, 0xff0a0c11, 0xff151922, 0xff1f2430, 0xff282e3b, 0xff1b202a, 0xff2a303d, 0xff3e4657,
                0xffe8edf5, 0xff9aa3b4, 0xff7a8290,
                0xffc6ee6a, 0xff5fd3e8, 0xffffb547, 0xffff5c6c, 0xffff3b30 }),

            // 焦げ茶の古いミキサー卓。文字はクリーム色
            make ("analog", "Analog", {
                0xff1a1611, 0xff120f0b, 0xff221d16, 0xff2e271e, 0xff383026, 0xff2a241c, 0xff3b3328, 0xff54493a,
                0xfff3e6c8, 0xffb3a486, 0xff90846c,
                0xff9bd45a, 0xff7fb8c9, 0xfff28c38, 0xffe8524a, 0xffff3b2e }),

            // 黒紫に蛍光色。rec は bad（ピンク）より赤に寄せて分ける
            make ("neon", "Neon", {
                0xff0b0a10, 0xff07060b, 0xff121019, 0xff1c1926, 0xff252134, 0xff191623, 0xff2a2538, 0xff403955,
                0xfff2eeff, 0xffa8a0c0, 0xff7f7895,
                0xff39ff88, 0xff3fd0ff, 0xffffd23f, 0xffff4f8b, 0xffff2e4d }),

            // 黒に青白い線、シアンと紫の発光
            make ("gaming", "Gaming", {
                0xff0b0c10, 0xff050608, 0xff11131a, 0xff1a1d27, 0xff232736, 0xff161922, 0xff262b3a, 0xff3a4258,
                0xffeef2ff, 0xff9aa3bf, 0xff757d95,
                0xff00e5ff, 0xffb15cff, 0xffffb800, 0xffff3d6e, 0xffff1f1f }),

            // 夜の紫に桜色。bad は赤系にせず紫（桜色と見分ける）。rec は桜色と離れた朱寄りの赤
            make ("sakura", "Sakura", {
                0xff18121a, 0xff100c12, 0xff201824, 0xff2b2130, 0xff352939, 0xff261d2a, 0xff392d3e, 0xff524357,
                0xfff7ecf3, 0xffb8a5b5, 0xff8f7e8d,
                0xffff9ec7, 0xff9fd8f0, 0xffffc970, 0xffb06cff, 0xffff3b30 }),

            // 真っ黒に白い文字。線と控えめな文字も明るくして、小さい画面・明るい現場でも読める
            make ("contrast", "High Contrast", {
                0xff000000, 0xff000000, 0xff0a0a0a, 0xff1c1c1c, 0xff2c2c2c, 0xff262626, 0xff4d4d4d, 0xff808080,
                0xffffffff, 0xffd4d4d4, 0xff9e9e9e,
                0xffffe500, 0xff00d5ff, 0xffff9900, 0xffff3355, 0xffff2020 }),

            // Okabe–Ito の配色（空色 / 赤紫 / 黄 / 朱）。中間色は Booth と同じ
            make ("colorsafe", "Color Safe", {
                0xff141311, 0xff0d0c0b, 0xff1b1a17, 0xff262420, 0xff302d28, 0xff24221e, 0xff34312b, 0xff4a463e,
                0xfff2ede3, 0xffa9a295, 0xff878278,
                0xff56b4e9, 0xffcc79a7, 0xfff0e442, 0xffd55e00, 0xffff3b30 }),
        };
    }

    /** 文字列：前後の空白と制御文字を除き、64 文字で切る */
    juce::String cleanString (const juce::var& v)
    {
        if (! v.isString())
            return {};

        const auto in = v.toString();
        juce::String out;
        for (auto p = in.getCharPointer(); ! p.isEmpty(); ++p)
            if (const auto c = *p; c >= 0x20 && c != 0x7f)
                out += juce::String::charToString (c);

        return out.trim().substring (0, maxStringLength);
    }

    double linear (double c)
    {
        return c <= 0.04045 ? c / 12.92 : std::pow ((c + 0.055) / 1.055, 2.4);
    }

    struct Rgb { double r, g, b; };

    Rgb rgbOf (juce::uint32 argb)
    {
        return { (double) ((argb >> 16) & 0xff) / 255.0, (double) ((argb >> 8) & 0xff) / 255.0, (double) (argb & 0xff) / 255.0 };
    }

    double luminance (juce::uint32 argb)
    {
        const auto c = rgbOf (argb);
        return 0.2126 * linear (c.r) + 0.7152 * linear (c.g) + 0.0722 * linear (c.b);
    }

    struct Lab { double l, a, b; };

    Lab labOf (juce::uint32 argb)
    {
        const auto c = rgbOf (argb);
        const auto r = linear (c.r), g = linear (c.g), b = linear (c.b);

        // sRGB → XYZ（D65）
        const auto x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047;
        const auto y = (0.2126729 * r + 0.7151522 * g + 0.0721750 * b) / 1.0;
        const auto z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883;

        auto f = [] (double t) { return t > 216.0 / 24389.0 ? std::cbrt (t) : (24389.0 / 27.0 * t + 16.0) / 116.0; };
        const auto fx = f (x), fy = f (y), fz = f (z);
        return { 116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz) };
    }
}

//==============================================================================
const char* tokenKey (Token t)
{
    const auto i = (int) t;
    return juce::isPositiveAndBelow (i, numTokens) ? keys[i] : "";
}

Group groupOf (Token t)
{
    switch (t)
    {
        case Token::bg0: case Token::bgDeep: case Token::panel: case Token::raised: case Token::raisedHi:
            return Group::background;
        case Token::grid: case Token::line: case Token::lineHi:
            return Group::lines;
        case Token::text: case Token::textDim: case Token::textMute:
            return Group::text;
        case Token::signal: case Token::ref: case Token::warn: case Token::bad: case Token::rec:
            break;
    }
    return Group::meaning;
}

const std::vector<Skin>& builtInSkins()
{
    static const std::vector<Skin> skins = createBuiltIns();
    return skins;
}

const Skin* findBuiltIn (const juce::String& id)
{
    for (auto& s : builtInSkins())
        if (s.id == id)
            return &s;
    return nullptr;
}

const Skin& defaultSkin()
{
    return builtInSkins().front();
}

juce::String subtitleKey (const juce::String& id)
{
    // tools/check_i18n.py がキーを見つけられるよう、組み立てずにそのまま書く
    static const std::pair<const char*, const char*> table[] = {
        { "booth",      "skin.booth.subtitle" },
        { "studio-day", "skin.studioDay.subtitle" },
        { "sweet",      "skin.sweet.subtitle" },
        { "midnight",   "skin.midnight.subtitle" },
        { "analog",     "skin.analog.subtitle" },
        { "neon",       "skin.neon.subtitle" },
        { "gaming",     "skin.gaming.subtitle" },
        { "sakura",     "skin.sakura.subtitle" },
        { "contrast",   "skin.contrast.subtitle" },
        { "colorsafe",  "skin.colorsafe.subtitle" },
    };
    for (auto& [skinId, key] : table)
        if (id == skinId)
            return key;
    return {};
}

//==============================================================================
bool parseHex (const juce::String& s, juce::uint32& out)
{
    const auto t = s.trim();
    if (t.length() != 7 || ! t.startsWithChar ('#'))
        return false;

    const auto digits = t.substring (1);
    if (! digits.containsOnly ("0123456789abcdefABCDEF"))
        return false;

    out = 0xff000000u | (juce::uint32) digits.getHexValue32();
    return true;
}

juce::String toHex (juce::uint32 argb)
{
    return "#" + juce::String::toHexString ((int) (argb & 0xffffffu)).paddedLeft ('0', 6).toUpperCase();
}

juce::String sanitiseId (const juce::String& s)
{
    const auto lower = s.toLowerCase();
    juce::String out;
    for (auto p = lower.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            out += juce::String::charToString (c);
        else if (out.isNotEmpty() && ! out.endsWithChar ('-'))
            out += "-";
    }

    out = out.substring (0, maxStringLength);
    while (out.endsWithChar ('-'))
        out = out.dropLastCharacters (1);
    return out.isEmpty() ? juce::String ("skin") : out;
}

ReadResult parse (const juce::String& json)
{
    ReadResult result;

    if ((juce::int64) json.getNumBytesAsUTF8() > maxFileBytes)
    {
        result.error = ReadError::tooLarge;
        return result;
    }

    juce::var root;
    if (juce::JSON::parse (json, root).failed() || root.isVoid())
    {
        result.error = ReadError::notJson;
        return result;
    }

    auto* obj = root.getDynamicObject();
    if (obj == nullptr || obj->getProperty ("format").toString() != formatName)
    {
        result.error = ReadError::notSkin;
        return result;
    }

    const auto& version = obj->getProperty ("version");
    if (! (version.isInt() || version.isInt64() || version.isDouble()) || (int) version < 1)
    {
        result.error = ReadError::notSkin;
        return result;
    }
    if ((int) version > formatVersion)
    {
        result.error = ReadError::newerVersion;
        return result;
    }

    // 書いていない色は base から（知らない base は booth）
    const auto baseId = cleanString (obj->getProperty ("base"));
    const auto* base = findBuiltIn (baseId);
    if (base == nullptr) base = &defaultSkin();

    auto& s = result.skin;
    s.colours = base->colours;
    s.base = base->id;
    s.name = cleanString (obj->getProperty ("name"));
    s.author = cleanString (obj->getProperty ("author"));
    s.id = sanitiseId (cleanString (obj->getProperty ("id")).isNotEmpty() ? cleanString (obj->getProperty ("id")) : s.name);
    if (s.name.isEmpty())
        s.name = s.id;

    if (auto* colours = obj->getProperty ("colours").getDynamicObject())
    {
        for (int i = 0; i < numTokens; ++i)
        {
            const auto& v = colours->getProperty (keys[i]);
            juce::uint32 argb = 0;
            if (v.isString() && parseHex (v.toString(), argb))
                s.colours[(size_t) i] = argb;
        }
    }

    return result;
}

ReadResult readFile (const juce::File& f)
{
    ReadResult result;

    if (! f.existsAsFile())
    {
        result.error = ReadError::unreadable;
        return result;
    }
    if (f.getSize() > maxFileBytes)
    {
        result.error = ReadError::tooLarge;
        return result;
    }

    juce::MemoryBlock data;
    if (! f.loadFileAsData (data))
    {
        result.error = ReadError::unreadable;
        return result;
    }

    // UTF-8（BOM があれば飛ばす）
    auto* bytes = static_cast<const char*> (data.getData());
    auto size = data.getSize();
    if (size >= 3 && (juce::uint8) bytes[0] == 0xef && (juce::uint8) bytes[1] == 0xbb && (juce::uint8) bytes[2] == 0xbf)
    {
        bytes += 3;
        size -= 3;
    }

    if (! juce::CharPointer_UTF8::isValidString (bytes, (int) size))
    {
        result.error = ReadError::notJson;
        return result;
    }

    return parse (juce::String::fromUTF8 (bytes, (int) size));
}

juce::String toJson (const Skin& s)
{
    auto colours = std::make_unique<juce::DynamicObject>();
    for (int i = 0; i < numTokens; ++i)
        colours->setProperty (keys[i], toHex (s.colours[(size_t) i]));

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("format", formatName);
    root->setProperty ("version", formatVersion);
    root->setProperty ("id", s.id);
    root->setProperty ("name", s.name);
    root->setProperty ("author", s.author);
    root->setProperty ("base", s.base);
    root->setProperty ("colours", juce::var (colours.release()));

    return juce::JSON::toString (juce::var (root.release()), false) + "\n";
}

bool writeFile (const Skin& s, const juce::File& target)
{
    if (! target.getParentDirectory().createDirectory())
        return false;

    // 途中で落ちても前のファイルを壊さない
    juce::TemporaryFile temp (target);
    if (! temp.getFile().replaceWithText (toJson (s), false, false, "\n"))
        return false;
    return temp.overwriteTargetFileWithTemporary();
}

//==============================================================================
double contrastRatio (juce::uint32 a, juce::uint32 b)
{
    auto la = luminance (a), lb = luminance (b);
    if (la < lb) std::swap (la, lb);
    return (la + 0.05) / (lb + 0.05);
}

double deltaE76 (juce::uint32 a, juce::uint32 b)
{
    const auto p = labOf (a), q = labOf (b);
    return std::sqrt ((p.l - q.l) * (p.l - q.l) + (p.a - q.a) * (p.a - q.a) + (p.b - q.b) * (p.b - q.b));
}

double hueDegrees (juce::uint32 argb)
{
    const auto c = rgbOf (argb);
    const auto mx = juce::jmax (c.r, c.g, c.b), mn = juce::jmin (c.r, c.g, c.b);
    const auto d = mx - mn;
    if (d <= 0.0)
        return 0.0;

    double h;
    if (c.r >= c.g && c.r >= c.b) h = std::fmod ((c.g - c.b) / d, 6.0);
    else if (c.g >= c.b)          h = (c.b - c.r) / d + 2.0;
    else                          h = (c.r - c.g) / d + 4.0;

    h *= 60.0;
    return h < 0.0 ? h + 360.0 : h;
}

double saturation (juce::uint32 argb)
{
    const auto c = rgbOf (argb);
    const auto mx = juce::jmax (c.r, c.g, c.b), mn = juce::jmin (c.r, c.g, c.b);
    return mx <= 0.0 ? 0.0 : (mx - mn) / mx;
}

std::vector<Warning> check (const Skin& s, const Limits& lim)
{
    std::vector<Warning> out;

    auto contrast = [&] (Token fg, Token bg, double required)
    {
        const auto v = contrastRatio (s.get (fg), s.get (bg));
        if (v < required)
            out.push_back ({ Warning::Kind::contrast, fg, bg, v, required });
    };

    auto distinct = [&] (Token a, Token b)
    {
        const auto v = deltaE76 (s.get (a), s.get (b));
        if (v < lim.deltaE)
            out.push_back ({ Warning::Kind::deltaE, a, b, v, lim.deltaE });
    };

    // 文字と地（WCAG）
    contrast (Token::text, Token::bg0, lim.text);
    contrast (Token::text, Token::panel, lim.text);
    contrast (Token::textDim, Token::bg0, lim.textDim);
    contrast (Token::textDim, Token::panel, lim.textDim);
    contrast (Token::textMute, Token::bg0, lim.textMute);       // 小さい見出し・目盛りの数字
    contrast (Token::textMute, Token::bgDeep, lim.textMute);    // レーンの中の字（歌詞の次の行など）
    contrast (Token::textMute, Token::panel, lim.textMute);

    // ピッチの線と帯が見える
    contrast (Token::signal, Token::bgDeep, lim.lines);
    contrast (Token::ref, Token::bgDeep, lim.lines);

    // 合っている / ずれ / 大きくずれ を見分けられる
    distinct (Token::signal, Token::warn);
    distinct (Token::signal, Token::bad);
    distinct (Token::warn, Token::bad);

    // rec は赤系（色相 330〜20°、くすみすぎない）
    const auto hue = hueDegrees (s.get (Token::rec));
    const bool redHue = hue >= lim.recHueFrom || hue <= lim.recHueTo;
    if (! redHue || saturation (s.get (Token::rec)) < lim.recMinSaturation)
        out.push_back ({ Warning::Kind::recHue, Token::rec, Token::rec, hue, lim.recHueFrom });

    return out;
}

//==============================================================================
Library::Library (juce::File folder) : dir (std::move (folder))
{
    reload();
}

void Library::reload()
{
    user.clear();
    files.clear();
    if (! dir.isDirectory())
        return;

    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension))
    {
        auto r = readFile (f);
        if (! r.ok())
            continue;

        // ファイル名が id（fileFor() と対にする）。内蔵と重なる・同じ id のものは飛ばす
        r.skin.id = sanitiseId (f.getFileNameWithoutExtension());
        r.skin.builtIn = false;
        if (find (r.skin.id) != nullptr)
            continue;
        files[r.skin.id] = f;
        user.push_back (r.skin);
    }

    std::sort (user.begin(), user.end(), [] (const Skin& a, const Skin& b)
    {
        return a.name.compareNatural (b.name) < 0;
    });
}

std::vector<Skin> Library::all() const
{
    auto out = builtInSkins();
    out.insert (out.end(), user.begin(), user.end());
    return out;
}

const Skin* Library::find (const juce::String& id) const
{
    if (auto* s = findBuiltIn (id))
        return s;
    for (auto& s : user)
        if (s.id == id)
            return &s;
    return nullptr;
}

juce::String Library::uniqueId (const juce::String& wanted, const juce::String& except) const
{
    const auto base = sanitiseId (wanted);
    auto id = base;
    for (int n = 2; (find (id) != nullptr && id != except) || findBuiltIn (id) != nullptr; ++n)
    {
        const auto suffix = "-" + juce::String (n);
        id = base.substring (0, maxStringLength - suffix.length()) + suffix;
    }
    return id;
}

juce::File Library::fileFor (const juce::String& id) const
{
    // 読んだファイルがあればそれを使う（ファイル名と id が違うと、削除しても残り、保存すると同じスキンが 2 つになっていた。監査 2026-10-06）
    if (auto it = files.find (id); it != files.end())
        return it->second;
    return dir.getChildFile (sanitiseId (id) + fileExtension);
}

bool Library::save (const Skin& s)
{
    if (findBuiltIn (s.id) != nullptr || s.id != sanitiseId (s.id))
        return false;

    auto copy = s;
    copy.builtIn = false;
    if (! writeFile (copy, fileFor (copy.id)))
        return false;

    reload();
    return true;
}

bool Library::remove (const juce::String& id)
{
    if (findBuiltIn (id) != nullptr)
        return false;

    const auto f = fileFor (id);
    if (f.existsAsFile() && ! f.deleteFile())
        return false;

    reload();
    return true;
}
} // namespace vb::skin
