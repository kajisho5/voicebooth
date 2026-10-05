#include "skin/Skin.h"

namespace vb::skin
{
class SkinTests : public juce::UnitTest
{
public:
    SkinTests() : juce::UnitTest ("Skin", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("built-in skins: 10, booth first and equal to DESIGN 4.9");
        {
            const auto& all = builtInSkins();
            expectEquals ((int) all.size(), 10);
            expectEquals (defaultSkin().id, juce::String ("booth"));

            const juce::uint32 booth[] = { 0xff141311, 0xff0d0c0b, 0xff1b1a17, 0xff262420, 0xff302d28, 0xff24221e, 0xff34312b, 0xff4a463e,
                                           0xfff2ede3, 0xffa9a295, 0xff878278, 0xffc6ee6a, 0xff8cc1ee, 0xfff4b942, 0xffff6b5e, 0xffff3b30 };
            for (int i = 0; i < numTokens; ++i)
                expectEquals ((juce::int64) defaultSkin().colours[(size_t) i], (juce::int64) booth[i], tokenKey (i));

            juce::StringArray ids;
            for (auto& s : all)
            {
                expect (s.builtIn);
                expect (! ids.contains (s.id), "duplicate id " + s.id);
                expect (sanitiseId (s.id) == s.id, "id must be file-safe: " + s.id);
                expect (subtitleKey (s.id).isNotEmpty(), "subtitle key for " + s.id);
                ids.add (s.id);
            }
            // 一覧の順（DESIGN 4.11 の表）
            expectEquals (ids.joinIntoString (","), juce::String ("booth,studio-day,sweet,midnight,analog,neon,gaming,sakura,contrast,colorsafe"));

            // DESIGN 4.11 の表の鍵になる色
            auto colour = [] (const char* id, Token t) { return (juce::int64) findBuiltIn (id)->get (t); };
            expectEquals (colour ("studio-day", Token::bg0), (juce::int64) 0xffece8e0);
            expectEquals (colour ("studio-day", Token::signal), (juce::int64) 0xff5e8f00);
            expectEquals (colour ("sweet", Token::signal), (juce::int64) 0xffd63f86);
            expectEquals (colour ("sweet", Token::bad), (juce::int64) 0xff7b3fd1);
            expectEquals (colour ("gaming", Token::signal), (juce::int64) 0xff00e5ff);
            expectEquals (colour ("midnight", Token::ref), (juce::int64) 0xff5fd3e8);
            expectEquals (colour ("sakura", Token::bad), (juce::int64) 0xffb06cff);
            expectEquals (colour ("contrast", Token::bg0), (juce::int64) 0xff000000);
            expectEquals (colour ("colorsafe", Token::signal), (juce::int64) 0xff56b4e9);
            expectEquals (colour ("colorsafe", Token::bg0), colour ("booth", Token::bg0));
        }

        beginTest ("every built-in skin passes its own checks");
        {
            for (auto& s : builtInSkins())
            {
                const auto w = check (s);
                for (auto& x : w)
                    logMessage ("  " + s.id + ": " + tokenKey (x.a) + " / " + tokenKey (x.b) + " = " + juce::String (x.value, 2));
                expect (w.empty(), s.id + " has warnings");

                // テストでも直接（判定のしくみが壊れても気づくように）
                expectGreaterOrEqual (contrastRatio (s.get (Token::text), s.get (Token::bg0)), 4.5, s.id);
                expectGreaterOrEqual (contrastRatio (s.get (Token::text), s.get (Token::panel)), 4.5, s.id);
                for (auto bg : { Token::bg0, Token::panel })
                    expectGreaterOrEqual (contrastRatio (s.get (Token::textDim), s.get (bg)), 4.5, s.id + " textDim");
                for (auto bg : { Token::bg0, Token::bgDeep, Token::panel })
                    expectGreaterOrEqual (contrastRatio (s.get (Token::textMute), s.get (bg)), 4.5, s.id + " textMute (small text, #28)");
                expectGreaterOrEqual (contrastRatio (s.get (Token::signal), s.get (Token::bgDeep)), 3.0, s.id);
                expectGreaterOrEqual (contrastRatio (s.get (Token::ref), s.get (Token::bgDeep)), 3.0, s.id);
                expectGreaterOrEqual (deltaE76 (s.get (Token::signal), s.get (Token::warn)), 25.0, s.id);
                expectGreaterOrEqual (deltaE76 (s.get (Token::signal), s.get (Token::bad)), 25.0, s.id);
                expectGreaterOrEqual (deltaE76 (s.get (Token::warn), s.get (Token::bad)), 25.0, s.id);
            }
        }

        beginTest ("contrast ratio: known values");
        {
            expectWithinAbsoluteError (contrastRatio (0xff000000, 0xffffffff), 21.0, 1.0e-9);
            expectWithinAbsoluteError (contrastRatio (0xffffffff, 0xff000000), 21.0, 1.0e-9);
            expectWithinAbsoluteError (contrastRatio (0xff777777, 0xff777777), 1.0, 1.0e-9);
            expectWithinAbsoluteError (contrastRatio (0xff767676, 0xffffffff), 4.54, 0.01);   // よく知られた「白地で 4.5 ぎりぎりの灰色」
            expectWithinAbsoluteError (contrastRatio (0xffff0000, 0xffffffff), 4.0, 0.01);
        }

        beginTest ("CIE76 delta E: known values");
        {
            expectWithinAbsoluteError (deltaE76 (0xff000000, 0xffffffff), 100.0, 0.01);
            expectWithinAbsoluteError (deltaE76 (0xff123456, 0xff123456), 0.0, 1.0e-9);
            expectWithinAbsoluteError (deltaE76 (0xffff0000, 0xff00ff00), 170.6, 0.5);   // sRGB の赤と緑
            expectLessThan (deltaE76 (0xff808080, 0xff828282), 1.0);
        }

        beginTest ("rec hue: red family only");
        {
            expectWithinAbsoluteError (hueDegrees (0xffff0000), 0.0, 1.0e-9);
            expectWithinAbsoluteError (hueDegrees (0xff00ff00), 120.0, 1.0e-9);
            expectWithinAbsoluteError (hueDegrees (0xffff00ff), 300.0, 1.0e-9);

            auto recWarnings = [] (juce::uint32 rec)
            {
                auto s = defaultSkin();
                s.set (Token::rec, rec);
                int n = 0;
                for (auto& w : check (s))
                    if (w.kind == Warning::Kind::recHue) ++n;
                return n;
            };
            expectEquals (recWarnings (0xffff3b30), 0);
            expectEquals (recWarnings (0xffff2e6a), 0);   // 少し赤紫（約 343°）
            expectEquals (recWarnings (0xff3080ff), 1);   // 青
            expectEquals (recWarnings (0xffff9900), 1);   // 橙（36°）
            expectEquals (recWarnings (0xff806060), 1);   // くすんだ赤（彩度が低い）
        }

        beginTest ("checks report low contrast and similar colours");
        {
            auto s = defaultSkin();
            s.set (Token::text, 0xff2a2825);        // 地とほぼ同じ
            s.set (Token::warn, 0xffc8ee6c);        // signal とほぼ同じ
            bool textWarned = false, deltaWarned = false;
            for (auto& w : check (s))
            {
                if (w.kind == Warning::Kind::contrast && w.a == Token::text) textWarned = true;
                if (w.kind == Warning::Kind::deltaE && w.a == Token::signal && w.b == Token::warn) deltaWarned = true;
            }
            expect (textWarned);
            expect (deltaWarned);

            // 前の Booth の控えめな文字（#6F6A60）は小さい字に暗すぎる（パネルの上で 3.2:1）
            auto old = defaultSkin();
            old.set (Token::textMute, 0xff6f6a60);
            bool muteWarned = false;
            for (auto& w : check (old))
                if (w.kind == Warning::Kind::contrast && w.a == Token::textMute) muteWarned = true;
            expect (muteWarned, "the old textMute is reported");
        }

        beginTest ("hex colours");
        {
            juce::uint32 c = 0;
            expect (parseHex ("#C6EE6A", c));
            expectEquals ((juce::int64) c, (juce::int64) 0xffc6ee6a);
            expect (parseHex ("#c6ee6a", c));
            expectEquals ((juce::int64) c, (juce::int64) 0xffc6ee6a);
            expect (parseHex (" #000000 ", c));
            expectEquals ((juce::int64) c, (juce::int64) 0xff000000);
            for (auto bad : { "C6EE6A", "#C6EE6", "#C6EE6A00", "#GGGGGG", "rgb(1,2,3)", "", "#-12345", "red" })
                expect (! parseHex (bad, c), bad);
            expectEquals (toHex (0xff0d0c0b), juce::String ("#0D0C0B"));
        }

        beginTest ("ids are file-safe");
        {
            expectEquals (sanitiseId ("My Skin!"), juce::String ("my-skin"));
            expectEquals (sanitiseId ("  --a__b--  "), juce::String ("a-b"));
            expectEquals (sanitiseId (juce::String::fromUTF8 ("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97")), juce::String ("skin"));
            expectEquals (sanitiseId ("../../etc/passwd"), juce::String ("etc-passwd"));
            expectEquals (sanitiseId (juce::String::repeatedString ("a", 100)).length(), maxStringLength);
        }

        beginTest ("JSON round trip");
        {
            auto s = *findBuiltIn ("sakura");
            s.builtIn = false;
            s.id = "my-sakura";
            s.name = juce::String::fromUTF8 ("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae\xe3\x82\xb9\xe3\x82\xad\xe3\x83\xb3");   // わたしのスキン
            s.author = "Someone";
            s.set (Token::signal, 0xff123456);

            const auto json = toJson (s);
            expect (json.contains ("\"format\": \"voicebooth-skin\""));
            expect (json.contains ("\"bgDeep\": \"#100C12\""));

            const auto r = parse (json);
            expect (r.ok());
            expectEquals (r.skin.id, s.id);
            expectEquals (r.skin.name, s.name);
            expectEquals (r.skin.author, s.author);
            expectEquals (r.skin.base, juce::String ("sakura"));
            expect (! r.skin.builtIn);
            for (int i = 0; i < numTokens; ++i)
                expectEquals ((juce::int64) r.skin.colours[(size_t) i], (juce::int64) s.colours[(size_t) i], tokenKey (i));

            // ファイル経由も
            juce::TemporaryFile temp (fileExtension);
            expect (writeFile (s, temp.getFile()));
            const auto f = readFile (temp.getFile());
            expect (f.ok());
            expect (f.skin.colours == s.colours);
            expectEquals (f.skin.name, s.name);
        }

        beginTest ("missing colours come from base; unknown keys and bad values are ignored");
        {
            const auto r = parse (R"({ "format": "voicebooth-skin", "version": 1, "id": "x", "name": "X", "author": "",
                                       "base": "midnight", "extra": { "a": [1, 2, 3] },
                                       "colours": { "bg0": "#010203", "signal": "lime", "ref": 123, "warn": "#ABCDEF00",
                                                    "glow": "#FFFFFF", "text": "#fafafa" } })");
            expect (r.ok());
            const auto& m = *findBuiltIn ("midnight");
            expectEquals ((juce::int64) r.skin.get (Token::bg0), (juce::int64) 0xff010203);
            expectEquals ((juce::int64) r.skin.get (Token::text), (juce::int64) 0xfffafafa);
            for (auto t : { Token::signal, Token::ref, Token::warn, Token::bad, Token::panel, Token::rec })
                expectEquals ((juce::int64) r.skin.get (t), (juce::int64) m.get (t), tokenKey (t));
            expectEquals (r.skin.base, juce::String ("midnight"));

            // 知らない base・base なし → booth
            for (auto json : { R"({ "format": "voicebooth-skin", "version": 1, "base": "nope", "colours": {} })",
                               R"({ "format": "voicebooth-skin", "version": 1 })" })
            {
                const auto b = parse (json);
                expect (b.ok());
                expectEquals (b.skin.base, juce::String ("booth"));
                expect (b.skin.colours == defaultSkin().colours);
                expectEquals (b.skin.id, juce::String ("skin"));   // id も名前も無い
            }

            // colours がオブジェクトでなければ全部 base
            const auto c = parse (R"({ "format": "voicebooth-skin", "version": 1, "base": "neon", "colours": "#000000" })");
            expect (c.ok());
            expect (c.skin.colours == findBuiltIn ("neon")->colours);
        }

        beginTest ("strings are limited to 64 characters and control characters are dropped");
        {
            const auto longName = juce::String::repeatedString (juce::String::fromUTF8 ("\xe6\xa1\x9c"), 100);   // 桜 x 100
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("format", formatName);
            o->setProperty ("version", 1);
            o->setProperty ("id", juce::String::repeatedString ("ab", 50));
            o->setProperty ("name", longName);
            o->setProperty ("author", "a\nb\tc");
            const auto r = parse (juce::JSON::toString (juce::var (o.get())));
            expect (r.ok());
            expectEquals (r.skin.name.length(), maxStringLength);
            expectEquals (r.skin.id.length(), maxStringLength);
            expectEquals (r.skin.author, juce::String ("abc"));

            // 文字列でない名前は空 → id を名前にする
            const auto n = parse (R"({ "format": "voicebooth-skin", "version": 1, "id": "abc", "name": 5 })");
            expectEquals (n.skin.name, juce::String ("abc"));
        }

        beginTest ("invalid input is rejected");
        {
            expect (parse ("").error == ReadError::notJson);
            expect (parse ("{ not json").error == ReadError::notJson);
            expect (parse ("[1, 2]").error == ReadError::notSkin);
            expect (parse ("   ").error == ReadError::notJson);
            expect (parse ("\"voicebooth-skin\"").error == ReadError::notJson);
            expect (parse (R"({ "format": "something-else", "version": 1 })").error == ReadError::notSkin);
            expect (parse (R"({ "format": "voicebooth-skin" })").error == ReadError::notSkin);
            expect (parse (R"({ "format": "voicebooth-skin", "version": "1" })").error == ReadError::notSkin);
            expect (parse (R"({ "format": "voicebooth-skin", "version": 0 })").error == ReadError::notSkin);
            expect (parse (R"({ "format": "voicebooth-skin", "version": 2 })").error == ReadError::newerVersion);

            // 64 KB を超える（中身は正しくても）
            const auto padding = juce::String::repeatedString (" ", (int) maxFileBytes);
            const auto big = R"({ "format": "voicebooth-skin", "version": 1 })" + padding;
            expect (parse (big).error == ReadError::tooLarge);

            juce::TemporaryFile temp (fileExtension);
            expect (temp.getFile().replaceWithText (big));
            expect (readFile (temp.getFile()).error == ReadError::tooLarge);

            // ぎりぎり 64 KB 以内なら読める
            const auto fits = R"({ "format": "voicebooth-skin", "version": 1 })";
            const auto exact = fits + juce::String::repeatedString (" ", (int) maxFileBytes - (int) juce::String (fits).getNumBytesAsUTF8());
            expect (parse (exact).ok());

            // 存在しない・UTF-8 でない
            expect (readFile (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("no-such-skin.vbskin")).error == ReadError::unreadable);
            juce::TemporaryFile binary (fileExtension);
            const char junk[] = { '{', (char) 0xff, (char) 0xfe, '}' };
            expect (binary.getFile().replaceWithData (junk, sizeof (junk)));
            expect (readFile (binary.getFile()).error == ReadError::notJson);

            // BOM つきの UTF-8 は読める
            juce::TemporaryFile bom (fileExtension);
            juce::MemoryOutputStream mo;
            mo.writeByte ((char) 0xef); mo.writeByte ((char) 0xbb); mo.writeByte ((char) 0xbf);
            mo << R"({ "format": "voicebooth-skin", "version": 1, "base": "analog" })";
            expect (bom.getFile().replaceWithData (mo.getData(), mo.getDataSize()));
            const auto b = readFile (bom.getFile());
            expect (b.ok());
            expectEquals (b.skin.base, juce::String ("analog"));
        }

        beginTest ("library: save, list, unique ids, remove; built-ins are protected");
        {
            const auto dir = juce::File::createTempFile ("vbskins");
            {
                Library lib (dir);   // まだフォルダが無い
                expect (lib.userSkins().empty());
                expectEquals ((int) lib.all().size(), (int) builtInSkins().size());

                auto s = defaultSkin();
                s.builtIn = false;
                s.name = "Mine";
                s.id = lib.uniqueId ("booth");
                expectEquals (s.id, juce::String ("booth-2"));
                expect (lib.save (s));
                expect (dir.getChildFile ("booth-2.vbskin").existsAsFile());
                expectEquals ((int) lib.userSkins().size(), 1);
                expect (lib.find ("booth-2") != nullptr && ! lib.find ("booth-2")->builtIn);
                expectEquals (lib.uniqueId ("booth"), juce::String ("booth-3"));
                expectEquals (lib.uniqueId ("booth-2", "booth-2"), juce::String ("booth-2"));   // 自分自身の上書きは同じ id

                // 内蔵の id には保存しない・消せない
                auto clash = s;
                clash.id = "booth";
                expect (! lib.save (clash));
                expect (! lib.remove ("booth"));
                expect (lib.find ("booth") != nullptr);

                // 壊れたファイル・内蔵と同じ名前のファイルは一覧に出さない
                expect (dir.getChildFile ("broken.vbskin").replaceWithText ("{"));
                expect (dir.getChildFile ("neon.vbskin").replaceWithText (toJson (s)));
                lib.reload();
                expectEquals ((int) lib.userSkins().size(), 1);
                expectEquals (lib.find ("neon")->author, juce::String ("VoiceBooth"));

                expect (lib.remove ("booth-2"));
                expect (lib.userSkins().empty());
                expect (! dir.getChildFile ("booth-2.vbskin").exists());
            }
            dir.deleteRecursively();
        }
    }
};

static SkinTests skinTests;
} // namespace vb::skin
