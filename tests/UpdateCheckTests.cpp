#include "update/UpdateCheck.h"
#include "update/Installer.h"
#include "system/AppCache.h"
#include <juce_core/juce_core.h>

namespace vb::update
{
/*  更新の確認（DESIGN 11.7）：バージョンの比べ方・リリースの選び方・本文の平文化と、キャッシュの大きさの表示。
    通信はしない（GitHub の /releases の形をまねた JSON で確かめる） */
class UpdateCheckTests : public juce::UnitTest
{
public:
    UpdateCheckTests() : juce::UnitTest ("UpdateCheck", "VoiceBooth") {}

    static int cmp (const char* a, const char* b) { return compareVersions (parseVersion (a), parseVersion (b)); }

    /** GitHub の /releases の 1 件（使う所だけ） */
    static juce::String release (const char* tag, bool prerelease, bool draft = false, const char* extraAssets = "")
    {
        juce::String v = juce::String (tag).trimCharactersAtStart ("v").upToFirstOccurrenceOf ("-", false, false);
        return juce::String (R"({ "tag_name": ")") + tag + R"(", "draft": )" + (draft ? "true" : "false")
             + R"(, "prerelease": )" + (prerelease ? "true" : "false")
             + R"(, "html_url": "https://github.com/kajisho5/voicebooth/releases/tag/)" + tag
             + R"(", "published_at": "2026-10-15T09:30:00Z", "body": "## What's new\r\n- Faster **export**\r\n",)"
             + R"( "assets": [)" + extraAssets
             + R"({ "name": "VoiceBooth-)" + v + R"(-win-x64-setup.exe", "size": 8493465,)"
             + R"(  "browser_download_url": "https://github.com/kajisho5/voicebooth/releases/download/)" + tag + "/VoiceBooth-" + v + R"(-win-x64-setup.exe" },)"
             + R"({ "name": "VoiceBooth-)" + v + R"(-mac-universal.dmg", "size": 12000000,)"
             + R"(  "browser_download_url": "https://github.com/kajisho5/voicebooth/releases/download/)" + tag + "/VoiceBooth-" + v + R"(-mac-universal.dmg" } ] })";
    }

    static juce::String list (std::initializer_list<juce::String> items)
    {
        juce::StringArray a;
        for (auto& i : items) a.add (i);
        return "[" + a.joinIntoString (",") + "]";
    }

    void runTest() override
    {
        beginTest ("versions parse with or without v, pre-release and build parts");
        {
            const auto v = parseVersion ("v0.1.0-beta.1");
            expect (v.valid);
            expectEquals (v.minor, 1);
            expect (v.isPrerelease());
            expectEquals (v.toString(), juce::String ("0.1.0-beta.1"));
            expectEquals (parseVersion ("1.2").toString(), juce::String ("1.2.0"));
            expectEquals (parseVersion ("1.2.3+build.7").toString(), juce::String ("1.2.3"));
            for (auto bad : { "", "v", "latest", "1", "1.2.3.4", "1.x.0", "1.2.3-", "1.2.3-beta..1", "nightly-2026" })
                expect (! parseVersion (bad).valid, bad);
        }

        beginTest ("SemVer order: numbers by value, final after its betas, longer pre-release later");
        {
            expect (cmp ("0.1.0", "0.2.0") < 0);
            expect (cmp ("0.10.0", "0.9.0") > 0);          // 文字順ではなく数で
            expect (cmp ("1.0.0", "0.99.99") > 0);
            expect (cmp ("0.1.0-beta.2", "0.1.0") < 0);    // 正式版はそのベータより新しい
            expect (cmp ("0.1.0-beta.11", "0.1.0-beta.2") > 0);
            expect (cmp ("0.1.0-beta", "0.1.0-beta.1") < 0);
            expect (cmp ("0.1.0-alpha.9", "0.1.0-beta.1") < 0);
            expect (cmp ("0.1.0-rc.1", "0.1.0-beta.9") > 0);
            expect (cmp ("0.1.0-1", "0.1.0-alpha") < 0);   // 数字だけの所は文字より前
            expect (cmp ("v0.1.0", "0.1.0") == 0);
            expect (cmp ("0.1.0+a", "0.1.0+b") == 0);      // ビルド情報は比べない
        }

        beginTest ("pre-releases are offered to beta users or when opted in");
        {
            expect (includePrereleases ("0.1.0-beta.1", false));
            expect (! includePrereleases ("0.1.0", false));
            expect (includePrereleases ("0.1.0", true));
        }

        beginTest ("pick the newest non-draft release newer than ours");
        {
            // 並び順は信じない（古い順でも、いちばん新しいバージョンを選ぶ）。下書き・壊れたタグは見ない
            const auto json = list ({ release ("v0.1.1", false), release ("v0.3.0", false, true), release ("v0.2.0", false),
                                      release ("nightly", false), release ("v0.2.1-beta.1", true) });
            auto r = pickRelease (json, "0.1.0", false, Platform::windows);
            expect (r.found);
            expectEquals (r.version, juce::String ("0.2.0"));
            expectEquals (r.assetName, juce::String ("VoiceBooth-0.2.0-win-x64-setup.exe"));
            expectEquals (r.assetSize, (juce::int64) 8493465);
            expectEquals (r.published, juce::String ("2026-10-15"));
            expect (r.pageUrl.endsWith ("/releases/tag/v0.2.0"));
            expect (! r.prerelease);

            // ベータの人（またはベータも選んだ人）にはベータも
            r = pickRelease (json, "0.1.0-beta.1", true, Platform::mac);
            expectEquals (r.version, juce::String ("0.2.1-beta.1"));
            expect (r.prerelease);
            expectEquals (r.assetName, juce::String ("VoiceBooth-0.2.1-mac-universal.dmg"));

            // Linux（開発用）はインストーラーが無い：ページを開く
            r = pickRelease (json, "0.1.0", false, Platform::other);
            expect (r.found && r.assetUrl.isEmpty() && r.pageUrl.isNotEmpty());
        }

        beginTest ("nothing to offer: same or older, skipped, empty, or broken");
        {
            const auto json = list ({ release ("v0.2.0", false), release ("v0.1.0", false) });
            expect (! pickRelease (json, "0.2.0", false, Platform::windows).found);
            expect (! pickRelease (json, "0.3.0-beta.1", true, Platform::windows).found);
            expect (! pickRelease (json, "0.1.0", false, Platform::windows, "0.2.0").found);   // 飛ばしたバージョン
            expect (pickRelease (json, "0.1.0", false, Platform::windows, "0.1.5").found);     // 別のバージョンを飛ばしていても新しい物は出す
            expect (! pickRelease ("[]", "0.1.0", false, Platform::windows).found);
            expect (! pickRelease ("{\"message\":\"Not Found\"}", "0.1.0", false, Platform::windows).found);
            expect (! pickRelease ("<html>", "0.1.0", false, Platform::windows).found);

            // 正式版の人には、印（prerelease）の付け忘れた「-beta」タグも出さない
            expect (! pickRelease (list ({ release ("v0.2.0-beta.1", false) }), "0.1.0", false, Platform::windows).found);
            // ベータの人には、同じバージョンの正式版が出たら知らせる
            expectEquals (pickRelease (list ({ release ("v0.1.0", false) }), "0.1.0-beta.3", true, Platform::windows).version,
                          juce::String ("0.1.0"));
        }

        beginTest ("CI test builds (0.2.0-0.ci) are older than every beta, so the released one is offered");
        {
            expect (parseVersion ("0.2.0-0.ci").valid);
            expect (cmp ("0.2.0-0.ci", "0.2.0-beta.2") < 0);
            expect (cmp ("0.2.0-0.ci", "0.1.0") > 0);
            const auto r = pickRelease (list ({ release ("v0.2.0-beta.2", true), release ("v0.2.0-beta.1", true) }),
                                        "0.2.0-0.ci", includePrereleases ("0.2.0-0.ci", false), Platform::windows);
            expectEquals (r.version, juce::String ("0.2.0-beta.2"));
        }

        beginTest ("fixture: a GitHub /releases response (draft, beta, out of order, extra assets)");
        {
            const auto json = juce::File (VOICEBOOTH_TEST_DATA_DIR).getChildFile ("github-releases.json").loadFileAsString();
            expect (json.isNotEmpty());

            // 正式版の人：下書きの 0.3.0 とベータの 0.2.1-beta.1 は見ず、0.2.0。.sha256 ではなくインストーラー本体
            auto r = pickRelease (json, "0.1.0", false, Platform::windows);
            expectEquals (r.version, juce::String ("0.2.0"));
            expectEquals (r.assetName, juce::String ("VoiceBooth-0.2.0-win-x64-setup.exe"));
            expectEquals (r.assetSize, (juce::int64) 30000000);
            expectEquals (r.pageUrl, juce::String ("https://github.com/kajisho5/voicebooth/releases/tag/v0.2.0"));
            expect (plainNotes (r.notes).contains ("Clearer input meter"));

            // ベータの人：ベータの 0.2.1-beta.1（Mac の dmg）。本文は日本語の画面なら前半、ほかは英語の後半
            r = pickRelease (json, "0.1.0-beta.2", true, Platform::mac);
            expectEquals (r.version, juce::String ("0.2.1-beta.1"));
            expect (r.prerelease);
            expectEquals (r.assetName, juce::String ("VoiceBooth-0.2.1-beta.1-mac-universal.dmg"));
            expectEquals (r.published, juce::String ("2026-11-01"));
            expect (plainNotes (notesForLanguage (r.notes, false)).contains ("Faster export"));
            expect (! notesForLanguage (r.notes, false).contains ("English follows"));

            // 最新を使っている・新しいバージョンを飛ばした
            expect (! pickRelease (json, "0.2.0", false, Platform::windows).found);
            expect (! pickRelease (json, "0.1.0", false, Platform::windows, "v0.2.0").found);
            const auto ok = interpret (200, json, "0.2.0", true, Platform::windows, {});
            expect (ok.status == CheckResult::Status::ok && ok.release.version == "0.2.1-beta.1");
        }

        beginTest ("only GitHub addresses are opened");
        {
            const auto evil = release ("v0.2.0", false).replace ("https://github.com/kajisho5/voicebooth/releases/download", "https://example.com/x");
            const auto r = pickRelease (list ({ evil }), "0.1.0", false, Platform::windows);
            expect (r.found && r.assetUrl.isEmpty());

            // 覚えておいた物を戻す時も
            auto saved = r;
            saved.pageUrl = "https://example.com/";
            expect (! Release::fromJson (saved.toJson()).found);
            const auto back = Release::fromJson (pickRelease (list ({ release ("v0.2.0", false) }), "0.1.0", false, Platform::mac).toJson());
            expect (back.found);
            expectEquals (back.version, juce::String ("0.2.0"));
            expectEquals (back.assetSize, (juce::int64) 12000000);
            expect (back.notes.contains ("Faster"));
        }

        beginTest ("in-app update: SHA-256 from the asset digest, kept across restarts");
        {
            const auto hex = juce::String::repeatedString ("ab", 32);
            expectEquals (sha256FromDigest ("sha256:" + hex), hex);
            expectEquals (sha256FromDigest ("SHA256:" + hex.toUpperCase()), hex);
            expect (sha256FromDigest ("sha512:" + hex).isEmpty());
            expect (sha256FromDigest ("sha256:" + hex.substring (2)).isEmpty());
            expect (sha256FromDigest ("sha256:" + hex.replace ("a", "z")).isEmpty());
            expect (sha256FromDigest ({}).isEmpty());

            // GitHub の asset の digest を読む。無いバージョン（古いリリース）は空 = アプリ内では入れ替えない
            const auto withDigest = release ("v0.2.0", false).replace (R"("size": 8493465,)", R"("size": 8493465, "digest": "sha256:)" + hex + "\",");
            auto r = pickRelease (list ({ withDigest }), "0.1.0", false, Platform::windows);
            expectEquals (r.assetSha256, hex);
            expectEquals (Release::fromJson (r.toJson()).assetSha256, hex);
            expect (pickRelease (list ({ release ("v0.2.0", false) }), "0.1.0", false, Platform::windows).assetSha256.isEmpty());
        }

        beginTest ("in-app update: only plain installer names are saved");
        {
            expect (safeAssetName ("VoiceBooth-0.2.0-win-x64-setup.exe", Platform::windows));
            expect (safeAssetName ("VoiceBooth-0.2.0-mac-universal.dmg", Platform::mac));
            expect (! safeAssetName ("VoiceBooth-0.2.0-mac-universal.dmg", Platform::windows));
            expect (! safeAssetName ("../VoiceBooth-0.2.0-win-x64-setup.exe", Platform::windows));
            expect (! safeAssetName ("x/VoiceBooth-0.2.0-win-x64-setup.exe", Platform::windows));
            expect (! safeAssetName ("x\\VoiceBooth-0.2.0-win-x64-setup.exe", Platform::windows));
            expect (! safeAssetName ("C:VoiceBooth-0.2.0-win-x64-setup.exe", Platform::windows));
            expect (! safeAssetName ({}, Platform::other));
        }

        beginTest ("in-app update: installer arguments and the Mac swap script");
        {
            const auto args = windowsInstallerArguments();
            for (auto* a : { "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS", "/relaunch=1" })
                expect (args.contains (a), a);

            // 入れ替えは「新しい物を横に置く → 古い物をよける → 入れる」。失敗したら元に戻して DMG を開く
            const auto script = macSwapScript();
            expect (script.startsWith ("#!/bin/sh"));
            expect (script.contains ("hdiutil attach") && script.contains ("ditto"));
            expect (script.indexOf ("mv \"$app\" \"$old\"") < script.indexOf ("mv \"$staged\" \"$app\""));
            expect (script.contains ("mv \"$old\" \"$app\""));   // 戻す
            expect (script.contains ("open \"$dmg\""));            // 手で入れられるように
           #if ! JUCE_WINDOWS
            // sh として文法が正しい（実際に入れ替えるのは Mac だけ）
            const auto tmp = juce::File::createTempFile (".sh");
            expect (tmp.replaceWithText (script, false, false, "\n"));
            juce::ChildProcess sh;
            expect (sh.start (juce::StringArray { "/bin/sh", "-n", tmp.getFullPathName() }));
            sh.waitForProcessToFinish (10000);
            expectEquals ((int) sh.getExitCode(), 0);
            tmp.deleteFile();
           #endif
        }

        beginTest ("in-app update: only a writable app bundle outside the disk image is replaced");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vb-bundle-test");
            root.deleteRecursively();
            const auto app = root.getChildFile ("VoiceBooth.app");
            app.createDirectory();
            expect (macBundleReplaceable (app));
            expect (! macBundleReplaceable (root));                              // .app でない
            expect (! macBundleReplaceable (root.getChildFile ("Gone.app")));    // 無い
            juce::String why;
            expect (! macBundleReplaceable (juce::File ("/Volumes/VoiceBooth/VoiceBooth.app"), &why));
            expect (why.isNotEmpty());
            root.deleteRecursively();
        }

        beginTest ("HTTP results: offline, rate limited, failed, ok");
        {
            const auto json = list ({ release ("v0.2.0", false) });
            using S = CheckResult::Status;
            expect (interpret (0, {}, "0.1.0", false, Platform::windows, {}).status == S::offline);
            expect (interpret (403, R"({"message":"API rate limit exceeded"})", "0.1.0", false, Platform::windows, {}).status == S::rateLimited);
            expect (interpret (429, {}, "0.1.0", false, Platform::windows, {}).status == S::rateLimited);
            expect (interpret (500, json, "0.1.0", false, Platform::windows, {}).status == S::failed);
            expect (interpret (200, "oops", "0.1.0", false, Platform::windows, {}).status == S::failed);
            const auto ok = interpret (200, json, "0.1.0", false, Platform::windows, {});
            expect (ok.status == S::ok && ok.release.found);
            const auto latest = interpret (200, json, "0.2.0", false, Platform::windows, {});
            expect (latest.status == S::ok && ! latest.release.found);   // 最新
        }

        beginTest ("once a day at most");
        {
            constexpr juce::int64 hour = 3600 * 1000, t = 1790000000000;
            expect (isDue (0, t));
            expect (! isDue (t, t + 23 * hour));
            expect (isDue (t, t + 24 * hour));
            expect (isDue (t, t - hour));   // 時計が戻った
        }

        beginTest ("release notes become plain text");
        {
            const juce::String md = "<!-- hidden -->\r\n**This is a beta.** See [Issues](https://github.com/x/y/issues).\r\n\r\n\r\n"
                                    "## Download\r\n| OS | File |\r\n|---|---|\r\n| Windows | `VoiceBooth-*-win-x64-setup.exe` |\r\n\r\n"
                                    "- Faster __export__\r\n* Clearer meter\r\n---\r\n> quoted\r\n\r\n";
            const auto lines = juce::StringArray::fromLines (plainNotes (md));
            expectEquals (lines[0], juce::String ("This is a beta. See Issues."));
            expectEquals (lines[1], juce::String());              // 空行は 1 つに
            expectEquals (lines[2], juce::String ("Download"));
            expectEquals (lines[3], juce::String ("OS  File"));
            expectEquals (lines[4], juce::String ("Windows  VoiceBooth-*-win-x64-setup.exe"));
            expectEquals (lines[6], juce::String::fromUTF8 ("\xe2\x80\xa2 Faster export"));
            expectEquals (lines[7], juce::String::fromUTF8 ("\xe2\x80\xa2 Clearer meter"));
            expectEquals (lines[8], juce::String ("quoted"));
            expectEquals (lines.size(), 9);                      // 終わりの空行は出さない
            expectEquals (juce::StringArray::fromLines (plainNotes (md, 3)).size(), 3);
        }

        beginTest ("bilingual notes: Japanese half for Japanese, English half for the rest");
        {
            const juce::String md = "**Beta.**\r\n\r\nEnglish follows Japanese.\r\n\r\n## A\r\n---\r\n\r\n**This is a beta.**\r\n## Download\r\n";
            expect (notesForLanguage (md, true).contains ("English follows"));
            expect (! notesForLanguage (md, true).contains ("Download"));
            expect (notesForLanguage (md, false).contains ("Download"));
            expect (! notesForLanguage (md, false).contains ("English follows"));
            // 区切り線があっても 2 段の形でなければそのまま
            const juce::String plain = "One\n---\nTwo";
            expectEquals (notesForLanguage (plain, false), plain);
            expectEquals (notesForLanguage ("Only one part", true), juce::String ("Only one part"));
        }

        beginTest ("cache size text and only our own folders are cleared");
        {
            expectEquals (system::formatSize (0), juce::String ("0 KB"));
            expectEquals (system::formatSize (1), juce::String ("1 KB"));
            expectEquals (system::formatSize (820 * 1024), juce::String ("820 KB"));
            expectEquals (system::formatSize ((juce::int64) (4.2 * 1024 * 1024)), juce::String ("4.2 MB"));
            expectEquals (system::formatSize ((juce::int64) 350 * 1024 * 1024), juce::String ("350 MB"));
            expectEquals (system::formatSize ((juce::int64) 1288490189), juce::String ("1.2 GB"));

            // 使う人が選んだフォルダ：アプリの offvocal/ だけを数えて消し、ほかのファイルは残す
            const auto dir = juce::File::createTempFile ("vb-cache");
            dir.createDirectory();
            const auto mine = dir.getChildFile ("offvocal/abc/song (off vocal).wav");
            mine.getParentDirectory().createDirectory();
            mine.replaceWithData (juce::MemoryBlock (3000).getData(), 3000);
            const auto theirs = dir.getChildFile ("notes.txt");
            theirs.replaceWithText ("keep me");
            expectEquals (system::cacheSize (dir), (juce::int64) 3000);
            expect (system::clearCache (dir));
            expect (! mine.exists());
            expect (theirs.existsAsFile());
            expectEquals (system::cacheSize (dir), (juce::int64) 0);
            expect (system::clearCache (dir.getChildFile ("missing")));   // 無いフォルダは何もしない
            dir.deleteRecursively();
        }
    }
};

static UpdateCheckTests updateCheckTests;
} // namespace vb::update
