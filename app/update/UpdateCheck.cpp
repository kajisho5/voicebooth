#include "UpdateCheck.h"
#include <juce_events/juce_events.h>

#ifndef VOICEBOOTH_VERSION_STRING
 #define VOICEBOOTH_VERSION_STRING "0.0.0"
#endif

namespace vb::update
{
namespace
{
    bool isNumeric (const juce::String& s)
    {
        return s.isNotEmpty() && s.containsOnly ("0123456789");
    }

    /** "0.1.0" のような数字の並びを読む（patch は省略してよい） */
    bool readCore (const juce::String& core, Version& v)
    {
        const auto parts = juce::StringArray::fromTokens (core, ".", "");
        if (parts.size() < 2 || parts.size() > 3)
            return false;
        for (auto& p : parts)
            if (! isNumeric (p) || p.length() > 6)
                return false;
        v.major = parts[0].getIntValue();
        v.minor = parts[1].getIntValue();
        v.patch = parts.size() > 2 ? parts[2].getIntValue() : 0;
        return true;
    }

    /** この OS のインストーラーか（CI が付ける名前。build.yml） */
    bool assetMatches (const juce::String& name, Platform p)
    {
        switch (p)
        {
            case Platform::windows: return name.endsWithIgnoreCase ("-win-x64-setup.exe");
            case Platform::mac:     return name.endsWithIgnoreCase ("-mac-universal.dmg");
            case Platform::other:   return false;
        }
        return false;
    }

    /** "[文字](URL)" → "文字"（画像の "![..](..)" も文字だけ） */
    juce::String stripLinks (juce::String s)
    {
        for (int from = 0;;)
        {
            const auto open = s.indexOfChar (from, '[');
            if (open < 0) break;
            const auto close = s.indexOfChar (open + 1, ']');
            if (close < 0 || close + 1 >= s.length() || s[close + 1] != '(') { from = open + 1; continue; }
            const auto end = s.indexOfChar (close + 2, ')');
            if (end < 0) { from = open + 1; continue; }
            const auto bang = open > 0 && s[open - 1] == '!' ? 1 : 0;
            const auto text = s.substring (open + 1, close);
            s = s.substring (0, open - bang) + text + s.substring (end + 1);
            from = open - bang + text.length();
        }
        return s;
    }

    juce::String stripInline (juce::String s)
    {
        s = stripLinks (s);
        return s.replace ("**", "").replace ("__", "").replace ("`", "").replace ("<br>", " ").trim();
    }

    /** 表の罫線（|---|:--:|）・区切りの線（--- / ***） */
    bool isRule (const juce::String& t)
    {
        return t.length() >= 3 && t.containsOnly ("-|: *=") && (t.contains ("---") || t.contains ("***") || t.contains ("==="));
    }
}

//==============================================================================
juce::String Version::toString() const
{
    juce::String s;
    s << major << "." << minor << "." << patch;
    if (isPrerelease())
        s << "-" << pre.joinIntoString (".");
    return s;
}

Version parseVersion (const juce::String& text)
{
    Version v;
    auto t = text.trim();
    if (t.startsWithIgnoreCase ("v"))
        t = t.substring (1);
    t = t.upToFirstOccurrenceOf ("+", false, false);   // ビルド情報は順に関係しない（SemVer 10）

    const auto core = t.upToFirstOccurrenceOf ("-", false, false);
    if (! readCore (core, v))
        return {};

    if (t.containsChar ('-'))
    {
        v.pre = juce::StringArray::fromTokens (t.fromFirstOccurrenceOf ("-", false, false), ".", "");
        if (v.pre.isEmpty())
            return {};
        for (auto& id : v.pre)
            if (id.isEmpty() || ! id.containsOnly ("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-"))
                return {};
    }
    v.valid = true;
    return v;
}

int compareVersions (const Version& a, const Version& b)
{
    const int ax[] = { a.major, a.minor, a.patch }, bx[] = { b.major, b.minor, b.patch };
    for (int i = 0; i < 3; ++i)
        if (ax[i] != bx[i])
            return ax[i] < bx[i] ? -1 : 1;

    // 正式版はプレリリースより新しい
    if (a.isPrerelease() != b.isPrerelease())
        return a.isPrerelease() ? -1 : 1;

    // プレリリースは点で区切った所ごとに：数字どうしは数で、ほかは文字コード順、数字は文字より前。
    // 前が同じなら区切りの多い方が新しい（beta < beta.1、beta.2 < beta.11）
    for (int i = 0; i < juce::jmin (a.pre.size(), b.pre.size()); ++i)
    {
        const auto& x = a.pre[i];
        const auto& y = b.pre[i];
        const bool xn = isNumeric (x), yn = isNumeric (y);
        if (xn && yn)
        {
            const auto xi = x.getLargeIntValue(), yi = y.getLargeIntValue();
            if (xi != yi) return xi < yi ? -1 : 1;
        }
        else if (xn != yn)
        {
            return xn ? -1 : 1;
        }
        else if (const auto c = x.compare (y); c != 0)
        {
            return c < 0 ? -1 : 1;
        }
    }
    if (a.pre.size() != b.pre.size())
        return a.pre.size() < b.pre.size() ? -1 : 1;
    return 0;
}

juce::String currentVersion()
{
    return VOICEBOOTH_VERSION_STRING;
}

Platform currentPlatform()
{
   #if JUCE_WINDOWS
    return Platform::windows;
   #elif JUCE_MAC
    return Platform::mac;
   #else
    return Platform::other;   // Linux は開発用（インストーラーを配っていない）：リリースのページを開く
   #endif
}

bool includePrereleases (const juce::String& current, bool optIn)
{
    return optIn || parseVersion (current).isPrerelease();
}

bool isDue (juce::int64 lastCheckMs, juce::int64 nowMs)
{
    constexpr juce::int64 day = 24 * 60 * 60 * 1000;
    return lastCheckMs <= 0 || nowMs - lastCheckMs >= day || nowMs < lastCheckMs;
}

//==============================================================================
juce::String Release::toJson() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", version);
    o->setProperty ("pageUrl", pageUrl);
    o->setProperty ("assetUrl", assetUrl);
    o->setProperty ("assetName", assetName);
    o->setProperty ("assetSize", assetSize);
    o->setProperty ("assetSha256", assetSha256);
    o->setProperty ("published", published);
    o->setProperty ("notes", notes);
    o->setProperty ("prerelease", prerelease);
    return juce::JSON::toString (juce::var (o), true);
}

Release Release::fromJson (const juce::String& json)
{
    Release r;
    const auto v = juce::JSON::parse (json);
    if (! v.isObject())
        return r;
    r.version = v["version"].toString();
    r.pageUrl = v["pageUrl"].toString();
    r.assetUrl = v["assetUrl"].toString();
    r.assetName = v["assetName"].toString();
    r.assetSize = (juce::int64) v["assetSize"];
    r.assetSha256 = sha256FromDigest ("sha256:" + v["assetSha256"].toString());
    r.published = v["published"].toString();
    r.notes = v["notes"].toString();
    r.prerelease = (bool) v["prerelease"];
    // 開くのは GitHub のページだけ（設定ファイルが書き換えられていても、よそのアドレスは開かない）
    r.found = parseVersion (r.version).valid && r.pageUrl.startsWith ("https://github.com/");
    if (! r.assetUrl.startsWith ("https://github.com/") || ! safeAssetName (r.assetName, currentPlatform()))
        r.assetUrl = {};
    return r;
}

Release pickRelease (const juce::String& json, const juce::String& current, bool includePre, Platform platform,
                     const juce::String& skipped)
{
    const auto list = juce::JSON::parse (json);
    if (! list.isArray())
        return {};

    const juce::var* best = nullptr;
    Version bestVersion;
    for (auto& item : *list.getArray())
    {
        if (! item.isObject() || (bool) item["draft"])
            continue;
        const auto v = parseVersion (item["tag_name"].toString());
        if (! v.valid)
            continue;
        // GitHub の印とタグの「-」のどちらかがあればプレリリース扱い（片方だけ付け忘れても正式版の人に出さない）
        if (((bool) item["prerelease"] || v.isPrerelease()) && ! includePre)
            continue;
        if (best == nullptr || compareVersions (v, bestVersion) > 0)
        {
            best = &item;
            bestVersion = v;
        }
    }

    const auto cur = parseVersion (current);
    if (best == nullptr || ! cur.valid || compareVersions (bestVersion, cur) <= 0)
        return {};
    if (const auto skip = parseVersion (skipped); skip.valid && compareVersions (bestVersion, skip) == 0)
        return {};

    Release r;
    r.version = bestVersion.toString();
    r.pageUrl = (*best)["html_url"].toString();
    if (! r.pageUrl.startsWith ("https://github.com/"))
        r.pageUrl = "https://github.com/kajisho5/voicebooth/releases/tag/" + (*best)["tag_name"].toString();
    r.published = (*best)["published_at"].toString().substring (0, 10);
    r.notes = (*best)["body"].toString();
    r.prerelease = (bool) (*best)["prerelease"] || bestVersion.isPrerelease();
    if (auto* assets = (*best)["assets"].getArray())
        for (auto& a : *assets)
        {
            const auto name = a["name"].toString();
            const auto url = a["browser_download_url"].toString();
            if (assetMatches (name, platform) && safeAssetName (name, platform) && url.startsWith ("https://github.com/"))
            {
                r.assetName = name;
                r.assetUrl = url;
                r.assetSize = (juce::int64) a["size"];
                r.assetSha256 = sha256FromDigest (a["digest"].toString());
                break;
            }
        }
    r.found = true;
    return r;
}

juce::String sha256FromDigest (const juce::String& digest)
{
    if (! digest.startsWithIgnoreCase ("sha256:"))
        return {};
    const auto hex = digest.substring (7).trim().toLowerCase();
    return hex.length() == 64 && hex.containsOnly ("0123456789abcdef") ? hex : juce::String();
}

bool safeAssetName (const juce::String& name, Platform platform)
{
    if (name.isEmpty() || name.containsAnyOf ("/\\:") || name.contains ("..") || name.startsWithChar ('.'))
        return false;
    return platform == Platform::other || assetMatches (name, platform);
}

juce::String notesForLanguage (const juce::String& markdown, bool japanese)
{
    const auto lines = juce::StringArray::fromLines (markdown.replace ("\r\n", "\n"));
    for (int i = 0; i < lines.size(); ++i)
        if (lines[i].trim() == "---")
        {
            juce::StringArray before, after;
            for (int j = 0; j < lines.size(); ++j)
                (j < i ? before : after).add (lines[j]);
            after.remove (0);
            if (! before.joinIntoString ("\n").containsIgnoreCase ("English follows"))
                break;   // 2 段の形ではない（ただの区切り線）
            return (japanese ? before : after).joinIntoString ("\n");
        }
    return markdown;
}

juce::String plainNotes (const juce::String& markdown, int maxLines)
{
    // HTML のコメント（<!-- ... -->）は見せない
    juce::String text = markdown.replace ("\r\n", "\n");
    for (int start; (start = text.indexOf ("<!--")) >= 0;)
    {
        const auto end = text.indexOf (start, "-->");
        text = text.substring (0, start) + (end < 0 ? juce::String() : text.substring (end + 3));
    }

    juce::StringArray out;
    for (auto line : juce::StringArray::fromLines (text))
    {
        auto t = line.trim();
        if (isRule (t))
            continue;
        if (t.startsWith ("#"))
            t = t.trimCharactersAtStart ("#").trim();
        else if (t.startsWith ("|"))
        {
            // 表の行：セルを 2 つの空白でつなぐ
            juce::StringArray cells;
            for (auto& c : juce::StringArray::fromTokens (t, "|", ""))
                if (c.trim().isNotEmpty())
                    cells.add (stripInline (c));
            t = cells.joinIntoString ("  ");
        }
        else if (t.startsWith ("- ") || t.startsWith ("* ") || t.startsWith ("+ "))
            t = juce::String::fromUTF8 ("\xe2\x80\xa2 ") + t.substring (2).trim();
        else if (t.startsWith (">"))
            t = t.trimCharactersAtStart ("> ");

        t = stripInline (t);
        if (t.isEmpty() && (out.isEmpty() || out[out.size() - 1].isEmpty()))
            continue;   // 空行は 1 つにまとめる（頭の空行は出さない）
        out.add (t);
        if (out.size() >= maxLines)
            break;
    }
    while (! out.isEmpty() && out[out.size() - 1].isEmpty())
        out.remove (out.size() - 1);
    return out.joinIntoString ("\n");
}

//==============================================================================
CheckResult interpret (int httpStatus, const juce::String& body, const juce::String& current, bool includePre,
                       Platform platform, const juce::String& skipped)
{
    CheckResult r;
    if (httpStatus == 0)
        r.status = CheckResult::Status::offline;
    else if (httpStatus == 429 || httpStatus == 403)
        r.status = CheckResult::Status::rateLimited;   // 認証なしは 1 時間に 60 回まで（同じ回線の人と合算）。公開リポジトリの GET の 403 はほぼこれ
    else if (httpStatus != 200 || ! juce::JSON::parse (body).isArray())
        r.status = CheckResult::Status::failed;
    else
    {
        r.status = CheckResult::Status::ok;
        r.release = pickRelease (body, current, includePre, platform, skipped);
    }
    return r;
}

//==============================================================================
Checker::Checker() : juce::Thread ("VoiceBooth update check") {}

Checker::~Checker()
{
    // アプリの終了：通信の途中なら切って、スレッドが終わるのを待つ（終わった後に結果を返さない）
    signalThreadShouldExit();
    {
        const juce::ScopedLock sl (lock);
        if (stream != nullptr)
            stream->cancel();
    }
    stopThread (4000);
}

bool Checker::start (const juce::String& current, bool includePre, const juce::String& skipped, std::function<void (CheckResult)> done)
{
    if (busy)
        return false;
    stopThread (2000);   // 前の確認のスレッドが結果を渡して終わる所なら、待ってから使い直す
    busy = true;
    currentVersion = current;
    includePrerelease = includePre;
    skippedVersion = skipped;
    callback = std::move (done);
    startThread();
    return true;
}

void Checker::run()
{
    int status = 0;
    juce::String body;
    {
        // 送るのはこの GET だけ。短く待って、だめなら諦める（起動を遅らせない・次の起動でまた確かめる）。
        // WebInputStream は 403 / 429 でも本文を読めるので、回数の上限か別の失敗かを見分けられる
        juce::WebInputStream in (juce::URL (releasesUrl), false);
        in.withExtraHeaders ("User-Agent: VoiceBooth\r\nAccept: application/vnd.github+json")
          .withConnectionTimeout (8000)
          .withNumRedirectsToFollow (3);
        {
            const juce::ScopedLock sl (lock);
            if (threadShouldExit())
                return;
            stream = &in;
        }
        if (in.connect (nullptr) && ! threadShouldExit())
        {
            status = in.getStatusCode();
            juce::MemoryBlock data;
            in.readIntoMemoryBlock (data, 2 * 1024 * 1024);   // 10 件で数十 KB。おかしな応答で膨らませない
            body = data.toString();
        }
        const juce::ScopedLock sl (lock);
        stream = nullptr;
    }
    if (threadShouldExit())
        return;

    busy = false;
    auto result = interpret (status, body, currentVersion, includePrerelease, currentPlatform(), skippedVersion);
    // 結果はメッセージスレッドへ。受け取る側（UiSession）は自分がまだ生きているかを確かめる
    juce::MessageManager::callAsync ([done = callback, result] { if (done) done (result); });
}
} // namespace vb::update
