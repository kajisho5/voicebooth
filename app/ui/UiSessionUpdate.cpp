#include "UiSession.h"
#include "system/AppCache.h"

/*  更新の確認（DESIGN 11.7）とアプリ共通のキャッシュの場所（DESIGN 8）。どちらもアプリの設定（Main.cpp が保存する）。
    更新は GitHub のリリースを読むだけ（送るのはその GET だけ）。ビルドは署名していないので自分では入れ替えない：
    知らせを出し、押されたらブラウザでインストーラー（無ければリリースのページ）を開く（MainComponent::openUpdate）。
    音声の処理には触れない */

namespace vb
{
void UiSession::restoreAppPrefs (bool autoCheck, bool betas, const juce::String& skipped, juce::int64 lastCheckMs,
                                 const juce::String& found, const juce::File& folder)
{
    s.updateAutoCheck = autoCheck;
    s.updateBetas = betas;
    s.updateSkipped = skipped;
    s.updateLastCheck = lastCheckMs;
    s.cacheFolder = folder;

    // 前の起動で見つけたバージョン：24 時間たっていなくても知らせを出し直す（確かめ直さない）。
    // もう入れた（いまのバージョンがそれ以上）・飛ばした・ベータを切った後のベータなら出さない
    const auto r = update::Release::fromJson (found);
    const auto cur = update::currentVersion();
    if (r.found && update::compareVersions (update::parseVersion (r.version), update::parseVersion (cur)) > 0
        && r.version != skipped && (! r.prerelease || update::includePrereleases (cur, betas)))
        setUpdateAvailable (r);
    notify (change::prefs);
}

void UiSession::checkForUpdatesIfDue()
{
    // 録音・再生中は確かめない（DESIGN 11.7。起動時と止まっている時だけ）
    if (! s.updateAutoCheck || s.updateChecking || s.isPlaying || s.isRecording
        || ! update::isDue (s.updateLastCheck, juce::Time::currentTimeMillis()))
        return;

    std::weak_ptr<bool> weak = alive;
    s.updateChecking = updateChecker.start (update::currentVersion(), update::includePrereleases (update::currentVersion(), s.updateBetas),
                                            s.updateSkipped,
                                            [this, weak] (update::CheckResult r) { if (! weak.expired()) finishUpdateCheck (r, false); });
}

void UiSession::checkForUpdatesNow()
{
    if (s.updateChecking || s.isPlaying || s.isRecording)   // 設定のキーも止めている間だけ押せる
        return;

    // 押して確かめた時は、飛ばしたバージョンでも見せる（自分から聞いたので）
    std::weak_ptr<bool> weak = alive;
    s.updateChecking = updateChecker.start (update::currentVersion(), update::includePrereleases (update::currentVersion(), s.updateBetas), {},
                                            [this, weak] (update::CheckResult r) { if (! weak.expired()) finishUpdateCheck (r, true); });
    notify (change::prefs);
}

void UiSession::finishUpdateCheck (const update::CheckResult& r, bool userAsked)
{
    using Status = update::CheckResult::Status;
    s.updateChecking = false;

    if (r.status == Status::ok)
    {
        // 確かめられた時だけ時計を進める（つながらなかったら、次の起動でまた確かめる）
        s.updateLastCheck = juce::Time::currentTimeMillis();
        if (r.release.found && userAsked && r.release.version == s.updateSkipped)
            s.updateSkipped = {};
        setUpdateAvailable (r.release);   // 無ければ前の知らせも消す（取り下げられたバージョンなど）
    }

    // 起動時の確認は黙っている（つながらない・上限でも何も言わない）。押した時だけ結果を知らせる
    if (userAsked)
    {
        switch (r.status)
        {
            case Status::ok:
                if (r.release.found)
                {
                    s.updateNoticeSerial = s.noticeSerial + 1;   // この知らせに「見る」を付ける
                    postNotice (tr ("update.found", r.release.version));
                }
                else
                {
                    postNotice (tr ("update.latest", update::currentVersion()));
                }
                break;
            case Status::offline:     postNotice (tr ("update.offline")); break;
            case Status::rateLimited: postNotice (tr ("update.rateLimited")); break;
            case Status::failed:      postNotice (tr ("update.failed")); break;
        }
    }
    notify (change::prefs);
}

void UiSession::setUpdateAutoCheck (bool on) { s.updateAutoCheck = on; notify (change::prefs); }

void UiSession::setUpdateBetas (bool on)
{
    s.updateBetas = on;
    // ベータを切ったら、いま出しているベータの知らせは下げる（正式版の人に勧めない）
    if (! on && s.updateRelease.prerelease && ! update::includePrereleases (update::currentVersion(), false))
        setUpdateAvailable ({});
    notify (change::prefs);
}

void UiSession::setUpdateAvailable (const update::Release& r)
{
    s.updateRelease = r.found ? r : update::Release {};
    s.updateVersion = r.found ? r.version : juce::String();
    notify (change::device | change::prefs);   // ステータスバーの知らせは device で描き直す
}

void UiSession::skipUpdate()
{
    if (s.updateVersion.isEmpty())
        return;
    if (! s.updateRelease.sample)   // 見本（--screen=update）のバージョンは覚えない
        s.updateSkipped = s.updateVersion;
    setUpdateAvailable ({});
}

//==============================================================================
juce::File UiSession::cacheFolder() const
{
    return s.cacheFolder != juce::File() ? s.cacheFolder : system::defaultCacheFolder();
}

void UiSession::setCacheFolder (const juce::File& folder)
{
    // 書けない場所は選ばせない（次にオフボを作る時に失敗するので、いま知らせる）
    if (folder != juce::File() && (! folder.isDirectory() || ! folder.hasWriteAccess()))
    {
        postNotice (tr ("settings.cache.notWritable", system::displayPath (folder)));
        return;
    }
    const auto before = cacheFolder();
    s.cacheFolder = folder == system::defaultCacheFolder() ? juce::File() : folder;
    if (cacheFolder() != before)
    {
        // 今ある分は動かさない（大きなファイルを黙って運ばない）。前の場所のことを伝える
        const auto left = system::cacheSize (before);
        postNotice (left > 0 ? tr ("settings.cache.moved", system::formatSize (left), system::displayPath (before))
                             : tr ("settings.cache.movedEmpty"));
    }
    notify (change::prefs);
}
} // namespace vb
