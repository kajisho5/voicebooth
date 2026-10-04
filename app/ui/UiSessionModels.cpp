#include "UiSession.h"
#include "i18n/Reasons.h"

/*  分離・リードボーカル・音程のモデルのダウンロード（B16c。2026-10-04 に UiSession.cpp から分けた。動きは変えていない）
    署名した一覧を読んで確認の画面を出す（requestSeparationModel）・足りないモデルを順にダウンロードする（startModelDownload）・
    キャンセル（待たない。遅れて届く知らせは downloadGeneration で捨てる） */

namespace vb
{
bool UiSession::modelsMissing() const
{
    using SC = separation::SeparatorClient;
    using DS = models::DownloadStatus::Stage;
    if (engine == nullptr || models::trustedKeys().empty() || ! SC::executable().existsAsFile())
        return false;
    if (s.modelDl.stage == (int) DS::downloading || s.modelDl.stage == (int) DS::verifying)
        return false;
    return ! SC::modelInstalled() || ! SC::karaokeInstalled() || ! SC::pitchModelFile().existsAsFile();
}

void UiSession::requestSeparationModel (bool onlyIfMissing)
{
    if (engine == nullptr)
        return;
    const auto keys = models::trustedKeys();
    if (keys.empty())
    {
        if (! onlyIfMissing)
            postNotice (tr ("model.notYet"));   // 配布の鍵がまだ（持ち主が用意したら使える）
        return;
    }
    auto openDialog = [this, onlyIfMissing]
    {
        if (onlyIfMissing && modelsToDownload().empty())
            return;   // 一覧にある物はすべて入っている（照合し直しは勧めない）
        // 分離・リードボーカル（ハモリのお手本）・音程のモデルのうち、まだ入っていない物をまとめて入れる（名前・ライセンス・大きさを並べる）
        juce::StringArray titles, licenses;
        juce::int64 size = 0;
        // 名前はかっこ書き（作者）を省く：3 つ並べても 1 行に収める（作者は「このアプリについて」に出す）
        for (auto& [entry, folder] : modelsToDownload())
        {
            titles.add (entry.title.upToFirstOccurrenceOf (" (", false, false));
            licenses.addIfNotAlreadyThere (entry.license);
            size += entry.totalSize();
        }
        if (titles.isEmpty() && modelEntry != nullptr)   // すべて入っている：分離のモデルを照合し直す
        {
            titles.add (modelEntry->title.upToFirstOccurrenceOf (" (", false, false));
            licenses.add (modelEntry->license);
            size = modelEntry->totalSize();
        }
        s.modelDl.known = true;
        s.modelDl.title = titles.joinIntoString (" + ");
        s.modelDl.license = licenses.joinIntoString (" / ");
        s.modelDl.size = size;
        ++s.modelDl.dialogSerial;
        notify (change::notice);
    };
    if (modelEntry != nullptr)
    {
        openDialog();
        return;
    }

    postNotice (tr ("model.checking"));
    const auto wantedId = separation::SeparatorClient::modelId();
    const auto karaokeId = separation::SeparatorClient::karaokeModelId();
    const auto pitchId = separation::SeparatorClient::pitchModelId();
    std::weak_ptr<bool> weak = alive;
    juce::Thread::launch ([this, weak, keys, wantedId, karaokeId, pitchId, openDialog]
    {
        auto http = models::makeHttpSource();
        juce::String error;
        std::shared_ptr<models::ModelEntry> found, foundKaraoke, foundPitch;
        juce::MemoryBlock list, sig;
        const auto url = models::manifestUrl();
        if (! models::fetchSmall (*http, url, list) || ! models::fetchSmall (*http, url + ".sig", sig))
            error = "can't reach " + url;
        else
        {
            // 署名を確かめてから中身を読む（HTTPS だけに頼らない。11.7）
            bool signedOk = false;
            for (auto& k : keys)
                signedOk = signedOk || models::verifySignature (list, sig.toString(), k);
            models::Manifest m;
            if (! signedOk)
                error = "the model list's signature doesn't match";
            else if (! models::parseManifest (list.toString(), m, error))
                error = "bad model list: " + error;
            else if (const auto* e = m.find (wantedId))
            {
                found = std::make_shared<models::ModelEntry> (*e);
                // 無くても分離は入れられる（ハモリのお手本が出ない・線は YIN のまま）
                if (const auto* k = m.find (karaokeId)) foundKaraoke = std::make_shared<models::ModelEntry> (*k);
                if (const auto* p = m.find (pitchId))   foundPitch = std::make_shared<models::ModelEntry> (*p);
            }
            else
                error = "the model isn't in the list";
        }
        juce::MessageManager::callAsync ([this, weak, found, foundKaraoke, foundPitch, error, openDialog]
        {
            if (weak.expired()) return;
            if (found == nullptr)
            {
                postNotice (tr ("model.listFailed", reasonText (error)));
                return;
            }
            modelEntry = std::make_unique<models::ModelEntry> (*found);
            if (foundKaraoke != nullptr) karaokeEntry = std::make_unique<models::ModelEntry> (*foundKaraoke);
            if (foundPitch != nullptr)   pitchEntry = std::make_unique<models::ModelEntry> (*foundPitch);
            openDialog();
        });
    });
}

std::vector<std::pair<models::ModelEntry, juce::File>> UiSession::modelsToDownload() const
{
    // 分離 → リードボーカル → 音程の順。入っている物は飛ばす
    std::vector<std::pair<models::ModelEntry, juce::File>> list;
    using SC = separation::SeparatorClient;
    if (modelEntry != nullptr && ! SC::modelInstalled())
        list.push_back ({ *modelEntry, SC::modelFolder() });
    if (karaokeEntry != nullptr && ! SC::karaokeInstalled())
        list.push_back ({ *karaokeEntry, SC::karaokeModelFolder() });
    if (pitchEntry != nullptr && ! SC::pitchModelFile().existsAsFile())
        list.push_back ({ *pitchEntry, SC::pitchModelFile().getParentDirectory() });
    return list;
}

void UiSession::startModelDownload()
{
    if (modelEntry == nullptr || s.isRecording)
        return;
    if (modelDownloader == nullptr)
        modelDownloader = std::make_unique<models::ModelDownloader> (models::makeHttpSource());
    // キャンセルした前の受け取りがまだ終わっていない（回線が止まっていると最大 20 秒）：終わるのを待ってから始める
    const bool stillStopping = modelDownloader->isBusy();
    if (stillStopping && s.modelDl.stage >= 0)
        return;   // 受け取り中
    ++downloadGeneration;
    s.modelDl.stage = (int) models::DownloadStatus::Stage::downloading;
    s.modelDl.received = 0;
    s.modelDl.error = {};
    // 進み具合はまとめた大きさで出す。すべて入っていれば分離のモデルを照合し直す
    downloadQueue = modelsToDownload();
    if (downloadQueue.empty())
        downloadQueue.push_back ({ *modelEntry, separation::SeparatorClient::modelFolder() });
    juce::int64 total = 0;
    for (auto& q : downloadQueue)
        total += q.first.totalSize();
    if (stillStopping)
        startQueuedModelWhenFree (0, 0, total);
    else
        startQueuedModel (0, 0, total);
    notify (change::view | change::notice);
}

void UiSession::startQueuedModel (size_t index, juce::int64 offset, juce::int64 total)
{
    if (index >= downloadQueue.size())
        return;
    std::weak_ptr<bool> weak = alive;
    const bool more = index + 1 < downloadQueue.size();
    const auto generation = downloadGeneration;
    modelDownloader->start (downloadQueue[index].first, downloadQueue[index].second,
                            [this, weak, index, offset, total, more, generation] (const models::DownloadStatus& st)
    {
        if (weak.expired() || generation != downloadGeneration) return;   // キャンセルした受け取りの知らせ（遅れて届く）は使わない
        const auto before = s.modelDl.stage;
        auto stage = st.stage;
        // 1 つ終わっても次が残っていれば、まだ「受け取り中」
        const bool next = more && stage == models::DownloadStatus::Stage::done;
        if (next)
            stage = models::DownloadStatus::Stage::downloading;
        s.modelDl.stage = (int) stage;
        s.modelDl.received = offset + st.received;
        s.modelDl.size = juce::jmax (total, offset + st.total);
        s.modelDl.bytesPerSecond = st.bytesPerSecond;
        s.modelDl.retryIn = st.retryInSeconds;
        s.modelDl.attempt = st.attempt;
        s.modelDl.paused = st.paused;
        s.modelDl.error = st.error;
        notify (before != s.modelDl.stage ? (juce::uint32) (change::view | change::notice) : (juce::uint32) change::view);
        if (next)
            startQueuedModelWhenFree (index + 1, offset + st.total, total);
    });
}

void UiSession::startQueuedModelWhenFree (size_t index, juce::int64 offset, juce::int64 total)
{
    // 前の物を受け取ったスレッドが終わるのを待ってから始める（終わる前は start が断る）
    std::weak_ptr<bool> weak = alive;
    const auto generation = downloadGeneration;
    juce::Timer::callAfterDelay (100, [this, weak, index, offset, total, generation]
    {
        if (weak.expired() || modelDownloader == nullptr || s.modelDl.stage < 0 || generation != downloadGeneration)
            return;   // 消えた・やめた
        if (modelDownloader->isBusy())
            startQueuedModelWhenFree (index, offset, total);
        else
            startQueuedModel (index, offset, total);
    });
}

void UiSession::cancelModelDownload()
{
    if (modelDownloader != nullptr)
        modelDownloader->cancel();   // 届いた分（.part）は残す。次は続きから
    ++downloadGeneration;
    s.modelDl.stage = -1;
    notify (change::view | change::notice);
}
} // namespace vb
