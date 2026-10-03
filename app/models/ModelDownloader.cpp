#include "ModelDownloader.h"
#include <map>

namespace vb::models
{
//==============================================================================
namespace
{
    class JuceHttp final : public HttpSource
    {
    public:
        Response get (const juce::String& url, juce::int64 from, const juce::String& etag) override
        {
            Response r;
            juce::String headers;
            if (from > 0)
            {
                headers << "Range: bytes=" << from << "-\r\n";
                if (etag.isNotEmpty())
                    headers << "If-Range: " << etag << "\r\n";
            }
            juce::StringPairArray responseHeaders;
            int status = 0;
            auto stream = juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                .withExtraHeaders (headers)
                                                                .withConnectionTimeoutMs (20000)
                                                                .withResponseHeaders (&responseHeaders)
                                                                .withStatusCode (&status)
                                                                .withNumRedirectsToFollow (5));
            if (stream == nullptr)
                return r;
            r.status = status;
            for (auto& key : responseHeaders.getAllKeys())
            {
                const auto value = responseHeaders[key].trim();
                if (key.equalsIgnoreCase ("ETag"))
                    r.etag = value;
                else if (key.equalsIgnoreCase ("Content-Length"))
                    r.length = value.getLargeIntValue();
                else if (key.equalsIgnoreCase ("Content-Range"))   // bytes a-b/total
                {
                    const auto range = value.fromFirstOccurrenceOf ("bytes", false, true).trim();
                    r.rangeStart = range.upToFirstOccurrenceOf ("-", false, false).getLargeIntValue();
                    const auto total = range.fromLastOccurrenceOf ("/", false, false);
                    r.total = total == "*" ? -1 : total.getLargeIntValue();
                }
            }
            r.body = std::move (stream);
            return r;
        }
    };

    struct PartState
    {
        juce::String etag, sha256;
    };

    PartState readState (const juce::File& f)
    {
        PartState s;
        const auto v = juce::JSON::parse (f.loadFileAsString());
        s.etag = v.getProperty ("etag", {}).toString();
        s.sha256 = v.getProperty ("sha256", {}).toString();
        return s;
    }

    void writeState (const juce::File& f, const PartState& s, juce::int64 bytes)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("etag", s.etag);
        o->setProperty ("sha256", s.sha256);
        o->setProperty ("bytes", bytes);
        f.replaceWithText (juce::JSON::toString (juce::var (o)));
    }

    bool truncateTo (const juce::File& f, juce::int64 size)
    {
        if (size <= 0)
            return f.deleteFile();
        juce::FileOutputStream out (f);
        return out.openedOk() && out.setPosition (size) && out.truncate().wasOk();
    }

    juce::String blockHash (const juce::File& f, juce::int64 start, juce::int64 length)
    {
        juce::FileInputStream in (f);
        if (! in.openedOk() || ! in.setPosition (start))
            return {};
        juce::MemoryBlock block ((size_t) length);
        if (in.read (block.getData(), (int) length) != (int) length)
            return {};
        return sha256Hex (block.getData(), block.getSize());
    }
}

std::unique_ptr<HttpSource> makeHttpSource() { return std::make_unique<JuceHttp>(); }

bool fetchSmall (HttpSource& http, const juce::String& url, juce::MemoryBlock& out, juce::int64 maxBytes)
{
    out.reset();
    auto r = http.get (url, 0, {});
    if (r.body == nullptr || r.status != 200)
        return false;
    char buffer[16384];
    juce::int64 total = 0;
    for (;;)
    {
        const auto n = r.body->read (buffer, (int) juce::jmin ((juce::int64) sizeof (buffer), maxBytes + 1 - total));
        if (n <= 0)
            break;
        out.append (buffer, (size_t) n);
        total += n;
        if (total > maxBytes)
            return false;   // 大きすぎる（一覧・署名ではない）
    }
    return r.length < 0 || total == r.length;   // 途中で切れた物は使わない
}

//==============================================================================
ModelDownloader::ModelDownloader (std::unique_ptr<HttpSource> source)
    : juce::Thread ("VoiceBooth model download"), http (std::move (source)) {}

ModelDownloader::~ModelDownloader()
{
    *alive = false;
    cancel();
}

bool ModelDownloader::installed (const ModelEntry& e, const juce::File& dir)
{
    for (auto& f : e.files)
        if (dir.getChildFile (f.name).getSize() != f.size)
            return false;
    return ! e.files.empty();
}

bool ModelDownloader::start (const ModelEntry& e, const juce::File& dir, std::function<void (const DownloadStatus&)> cb)
{
    if (isThreadRunning())
        return false;
    entry = e;
    folder = dir;
    onStatus = std::move (cb);
    status = {};
    status.total = entry.totalSize();
    startThread (juce::Thread::Priority::low);
    return true;
}

void ModelDownloader::cancel()
{
    signalThreadShouldExit();
    notify();
    stopThread (20000);
}

void ModelDownloader::setPaused (bool p)
{
    paused = p;
    notify();
}

void ModelDownloader::report (DownloadStatus s)
{
    std::weak_ptr<bool> weak = alive;
    auto cb = onStatus;
    juce::MessageManager::callAsync ([weak, cb, s] { if (! weak.expired() && cb) cb (s); });
}

void ModelDownloader::run()
{
    if (! folder.createDirectory())
    {
        status.stage = DownloadStatus::Stage::failed;
        status.error = "can't create " + folder.getFullPathName();
        report (status);
        return;
    }

    // 空き容量（残りの分 + 少しのゆとり）。足りなければ始めない
    juce::int64 remaining = 0;
    for (auto& f : entry.files)
        remaining += juce::jmax ((juce::int64) 0, f.size - juce::jmax (folder.getChildFile (f.name).getSize(),
                                                                       folder.getChildFile (f.name + ".part").getSize()));
    if (folder.getBytesFreeOnVolume() > 0 && folder.getBytesFreeOnVolume() < remaining + 64 * 1024 * 1024)
    {
        status.stage = DownloadStatus::Stage::failed;
        status.error = "not enough disk space";
        report (status);
        return;
    }

    juce::int64 done = 0;
    for (auto& f : entry.files)
    {
        if (! downloadFile (f, done))
            return;   // 状態はその中で知らせた
        done += f.size;
    }
    status.stage = DownloadStatus::Stage::done;
    status.received = status.total;
    report (status);
}

bool ModelDownloader::downloadFile (const ModelFile& f, juce::int64 doneBefore)
{
    using Stage = DownloadStatus::Stage;
    const auto dest = folder.getChildFile (f.name);
    const auto part = folder.getChildFile (f.name + ".part");
    const auto stateFile = folder.getChildFile (f.name + ".part.json");

    // もう入っている（中身も合う）
    if (dest.getSize() == f.size && sha256Hex (dest) == f.sha256)
    {
        status.received = doneBefore + f.size;
        report (status);
        return true;
    }
    dest.deleteFile();

    // 前回の続き：別のバージョン（期待する SHA-256 が違う）の .part は捨てる
    PartState st;
    if (stateFile.existsAsFile())
    {
        st = readState (stateFile);
        if (st.sha256 != f.sha256)
        {
            part.deleteFile();
            st = {};
        }
    }
    st.sha256 = f.sha256;
    juce::int64 have = part.existsAsFile() ? part.getSize() : 0;
    if (have > f.size) { part.deleteFile(); have = 0; }

    // 届いている分のうち、そろったブロックを照合（壊れていればそこから取り直す）
    for (juce::int64 b = 0; (b + 1) * f.blockSize <= have || (have == f.size && b * f.blockSize < have); ++b)
    {
        const auto start = b * f.blockSize, len = juce::jmin (f.blockSize, f.size - start);
        if (start + len > have) break;
        if (blockHash (part, start, len) != f.blocks[(int) b])
        {
            truncateTo (part, start);
            have = start;
            break;
        }
    }

    std::map<juce::int64, int> badCount;
    int restarts = 0;
    size_t attempt = 0;
    std::vector<char> buffer (256 * 1024);
    windowStart = juce::Time::getMillisecondCounterHiRes();
    windowBytes = 0;

    auto waitOrCancel = [this] (int seconds, int attemptNo)
    {
        for (int left = seconds; left > 0; --left)
        {
            status.stage = Stage::waiting;
            status.retryInSeconds = left;
            status.attempt = attemptNo;
            report (status);
            if (wait (1000), threadShouldExit()) return false;
        }
        status.stage = Stage::downloading;
        return true;
    };

    while (have < f.size)
    {
        if (threadShouldExit()) { status.stage = Stage::cancelled; writeState (stateFile, st, have); report (status); return false; }
        while (paused.load() && ! threadShouldExit())
        {
            status.paused = true;
            report (status);
            wait (300);
        }
        status.paused = false;
        status.stage = Stage::downloading;

        auto resp = http->get (f.url, have, have > 0 ? st.etag : juce::String());
        // 頭から頼んだのに途中からの 206 が返る（おかしな中継）も、つながらないのと同じに扱う
        const bool connected = resp.body != nullptr && (resp.status == 200 || resp.status == 206)
                            && ! (have == 0 && resp.status == 206 && resp.rangeStart != 0);
        if (! connected)
        {
            if (attempt >= retryDelays.size())
            {
                status.stage = Stage::interrupted;
                writeState (stateFile, st, have);
                report (status);
                return false;
            }
            const auto delay = retryDelays[attempt++];
            if (! waitOrCancel (delay, (int) attempt)) { status.stage = Stage::cancelled; report (status); return false; }
            continue;
        }

        if (have > 0)
        {
            // 続きをつないでよいか：206、頭が合う、全体の大きさが合う、ETag が変わっていない
            const bool resumable = resp.status == 206 && resp.rangeStart == have && resp.total == f.size
                                && (st.etag.isEmpty() || resp.etag.isEmpty() || resp.etag == st.etag);
            if (! resumable)
            {
                part.deleteFile();   // 中身が差し替わった・範囲指定が効かない：頭から
                have = 0;
                st.etag = {};
                continue;
            }
        }
        if (resp.etag.isNotEmpty())
            st.etag = resp.etag;
        writeState (stateFile, st, have);

        bool progressed = false, verifyRestart = false, giveUp = false;
        {
            juce::FileOutputStream out (part);   // 続きに足す（JUCE は既存のファイルの終わりから書く）
            if (! out.openedOk() || out.getPosition() != have)
            {
                status.stage = Stage::failed;
                status.error = "can't write " + part.getFullPathName();
                report (status);
                return false;
            }
            auto& in = *resp.body;
            for (;;)
            {
                if (threadShouldExit() || paused.load())
                    break;
                const auto want = (int) juce::jmin ((juce::int64) buffer.size(), f.size - have,
                                                    f.blockSize - (have % f.blockSize));   // ブロックの境目で止める
                if (want <= 0) break;
                const auto n = in.read (buffer.data(), want);
                if (n <= 0) break;
                out.write (buffer.data(), (size_t) n);
                have += n;
                progressed = true;
                windowBytes += n;
                status.received = doneBefore + have;
                const auto now = juce::Time::getMillisecondCounterHiRes();
                if (now - windowStart >= 500.0)
                {
                    status.bytesPerSecond = (double) windowBytes * 1000.0 / (now - windowStart);
                    windowStart = now;
                    windowBytes = 0;
                    report (status);
                }

                // ブロックがそろったら照合
                if (have % f.blockSize == 0 || have == f.size)
                {
                    out.flush();
                    const auto b = (have - 1) / f.blockSize;
                    const auto start = b * f.blockSize, len = have - start;
                    if (blockHash (part, start, len) != f.blocks[(int) b])
                    {
                        out.setPosition (start);
                        out.truncate();
                        have = start;
                        if (++badCount[b] >= 2)
                        {
                            // 同じ所が 2 回続けて壊れた：頭から。それでもだめなら失敗
                            if (++restarts >= 2)
                            {
                                giveUp = true;   // 消すのはファイルを閉じてから（Windows は開いたままでは消せない）
                                break;
                            }
                            badCount.clear();
                            have = 0;
                            verifyRestart = true;
                        }
                        break;   // 取り直す（もう一度頼む）
                    }
                    writeState (stateFile, st, have);
                }
            }
            out.flush();
        }
        if (giveUp)
        {
            part.deleteFile();
            stateFile.deleteFile();
            status.stage = Stage::failed;
            status.error = "verification failed";
            report (status);
            return false;
        }
        if (verifyRestart)
            part.deleteFile();
        if (progressed)
            attempt = 0;   // 進んだら待ちの回数を戻す
        else if (have < f.size && ! threadShouldExit() && ! paused.load())
        {
            if (attempt >= retryDelays.size())
            {
                status.stage = Stage::interrupted;
                writeState (stateFile, st, have);
                report (status);
                return false;
            }
            const auto delay = retryDelays[attempt++];
            if (! waitOrCancel (delay, (int) attempt)) { status.stage = Stage::cancelled; report (status); return false; }
        }
    }

    // 全体の照合 → 名前を変える
    status.stage = Stage::verifying;
    report (status);
    if (sha256Hex (part) != f.sha256)
    {
        part.deleteFile();
        stateFile.deleteFile();
        status.stage = Stage::failed;
        status.error = "verification failed";
        report (status);
        return false;
    }
    if (! part.moveFileTo (dest))
    {
        status.stage = Stage::failed;
        status.error = "can't move " + dest.getFileName();
        report (status);
        return false;
    }
    stateFile.deleteFile();
    return true;
}
} // namespace vb::models
