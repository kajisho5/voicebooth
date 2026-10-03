#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>

/*  新しいバージョンの確認（DESIGN 11.7）。GitHub のリリース（kajisho5/voicebooth）を 1 回読むだけ。
    - 送るのはリリース一覧の GET だけ（バージョン・OS・利用状況は送らない。User-Agent は GitHub が必須なので "VoiceBooth" だけ）
    - ビルドは署名していないので、自分では入れ替えない：見つけたら、この OS のインストーラー（無ければリリースのページ）を
      ブラウザで開くだけ
    - 選び方・比べ方は純粋な関数（テストする）。通信は Checker だけ */

namespace vb::update
{
/** バージョン（SemVer 2.0）。タグは "v0.1.0-beta.1" の形（頭の v は無くてもよい。"+..." のビルド情報は比べない） */
struct Version
{
    int major = 0, minor = 0, patch = 0;
    juce::StringArray pre;          // "beta.1" → { "beta", "1" }。空なら正式版
    bool valid = false;

    bool isPrerelease() const { return ! pre.isEmpty(); }
    juce::String toString() const;
};

Version parseVersion (const juce::String&);

/** SemVer の順（a < b なら負、同じなら 0、a > b なら正）。正式版はそのプレリリースより新しい（0.1.0-beta.2 < 0.1.0） */
int compareVersions (const Version& a, const Version& b);

/** このアプリのバージョン（CMake の project VERSION ＋ CI がタグから付けるプレリリースの印。例 "0.1.0-beta.1"） */
juce::String currentVersion();

enum class Platform { windows, mac, other };
Platform currentPlatform();

/** 知らせるリリース */
struct Release
{
    bool found = false;
    juce::String version;           // "0.2.0-beta.1"（頭の v は付けない）
    juce::String pageUrl;           // リリースのページ（html_url）
    juce::String assetUrl, assetName;   // この OS のインストーラー（*-win-x64-setup.exe / *-mac-universal.dmg）。無ければ空
    juce::int64 assetSize = 0;
    juce::String published;         // "2026-10-15"（published_at の日付）
    juce::String notes;             // 本文（Markdown のまま。見せる時は plainNotes）
    bool prerelease = false;
    bool sample = false;            // 画面の見本（--screen=update*。設定に覚えない）

    /** アプリ設定に覚えておく（次に起動した時、24 時間たっていなくても知らせを出し直すため） */
    juce::String toJson() const;
    static Release fromJson (const juce::String&);
};

/** ベータ（プレリリース）も知らせるか：いまのバージョンがベータなら常に、正式版なら使う人が選んだ時だけ */
bool includePrereleases (const juce::String& currentVersion, bool optIn);

/** GitHub の /releases の JSON から、知らせるバージョンを選ぶ。
    下書きは見ない、プレリリースは includePre の時だけ。並び順は信じずバージョンで比べて、いちばん新しい物を選ぶ。
    それがいまのバージョンより新しく、飛ばしたバージョン（skipped）でもない時だけ found。
    （飛ばしたバージョンより古い物は、もっと新しいバージョンを飛ばしたのだから出さない） */
Release pickRelease (const juce::String& json, const juce::String& currentVersion, bool includePre,
                     Platform, const juce::String& skipped = {});

/** 前回から 24 時間たったか（時計が戻った時も確かめ直す） */
bool isDue (juce::int64 lastCheckMs, juce::int64 nowMs);

/** 本文が「日本語 → --- → 英語」の 2 段（packaging/release-notes.md の形。前半に "English follows" がある）なら、
    日本語の画面には前半、ほかの言語には後半（英語）を返す。その形でなければそのまま */
juce::String notesForLanguage (const juce::String& markdown, bool japanese);

/** リリースの本文（Markdown）を画面に出す平文にする：見出しの #・強調・コード・リンクの URL・表の罫線・HTML のコメントを外し、
    箇条書きは「•」、空行は 1 つにまとめる。maxLines 行まで */
juce::String plainNotes (const juce::String& markdown, int maxLines = 80);

//==============================================================================
struct CheckResult
{
    enum class Status { ok, offline, rateLimited, failed };
    Status status = Status::failed;
    Release release;                // ok で新しいバージョンがある時だけ found
};

/** 返ってきた HTTP の結果を読む（テストする）。status 0 = つながらない */
CheckResult interpret (int httpStatus, const juce::String& body, const juce::String& currentVersion, bool includePre,
                       Platform, const juce::String& skipped);

/** 裏のスレッドで一覧を取りに行く（1 回ずつ）。持ち主（UiSession）が消える時（アプリの終了）は通信を切って止める */
class Checker : private juce::Thread
{
public:
    Checker();
    ~Checker() override;

    /** 確かめ始める。動いていれば false。done はメッセージスレッドで呼ぶ（受け取る側は自分が生きているかを確かめること） */
    bool start (const juce::String& currentVersion, bool includePre, const juce::String& skipped,
                std::function<void (CheckResult)> done);

private:
    void run() override;

    juce::String currentVersion, skippedVersion;
    bool includePrerelease = false;
    std::function<void (CheckResult)> callback;
    juce::CriticalSection lock;
    juce::WebInputStream* stream = nullptr;   // 通信中だけ（終了の時に切る）
    std::atomic<bool> busy { false };         // 確かめている（結果を渡したら false）

    JUCE_DECLARE_NON_COPYABLE (Checker)
};

/** 取りに行く URL（10 件。下書きは返ってこない） */
inline constexpr const char* releasesUrl = "https://api.github.com/repos/kajisho5/voicebooth/releases?per_page=10";
} // namespace vb::update
