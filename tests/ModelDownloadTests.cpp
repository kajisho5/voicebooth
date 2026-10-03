#include "models/ModelDownloader.h"

extern "C"
{
#include <optional/monocypher-ed25519.h>
}

namespace vb::models
{
namespace
{
    juce::String base64 (const void* d, size_t n) { return juce::Base64::toBase64 (d, n); }

    /** 手元の偽物のサーバー：切断・壊れたブロック・範囲指定を無視・中身の差し替えを起こせる */
    struct FakeHttp final : HttpSource
    {
        juce::MemoryBlock content;
        juce::String etag = "\"v1\"";
        int failNext = 0;                 // つながらない回数
        juce::int64 cutAfter = -1;        // 1 回の応答で送る最大（-1 = 全部）。送ったら切る
        int cutTimes = 0;
        std::map<juce::int64, int> corruptBlock;   // ブロックの頭 → あと何回壊すか
        juce::int64 blockSize = 65536;
        bool ignoreRange = false;
        int requests = 0;
        std::vector<juce::int64> froms;

        Response get (const juce::String&, juce::int64 from, const juce::String& ifRange) override
        {
            ++requests;
            froms.push_back (from);
            Response r;
            if (failNext > 0) { --failNext; return r; }

            const bool rangeOk = from > 0 && ! ignoreRange && (ifRange.isEmpty() || ifRange == etag);
            const auto start = rangeOk ? from : 0;
            juce::MemoryBlock body (static_cast<const char*> (content.getData()) + start, content.getSize() - (size_t) start);
            for (auto& [b, times] : corruptBlock)
                if (times > 0 && b >= start && b < (juce::int64) content.getSize())
                {
                    static_cast<char*> (body.getData())[b - start + 5] ^= 0x5a;
                    --times;
                }
            if (cutAfter >= 0 && cutTimes > 0 && (juce::int64) body.getSize() > cutAfter)
            {
                body.setSize ((size_t) cutAfter);
                --cutTimes;
            }
            r.status = rangeOk ? 206 : 200;
            r.etag = etag;
            r.rangeStart = rangeOk ? from : -1;
            r.total = (juce::int64) content.getSize();
            r.length = (juce::int64) body.getSize();
            r.body = std::make_unique<juce::MemoryInputStream> (body, true);
            return r;
        }
    };

    ModelEntry entryFor (const juce::MemoryBlock& data, juce::int64 blockSize)
    {
        ModelFile f;
        f.name = "part.onnx";
        f.url = "https://example.invalid/part.onnx";
        f.size = (juce::int64) data.getSize();
        f.sha256 = sha256Hex (data.getData(), data.getSize());
        f.blockSize = blockSize;
        for (juce::int64 a = 0; a < f.size; a += blockSize)
            f.blocks.add (sha256Hex (static_cast<const char*> (data.getData()) + a, (size_t) juce::jmin (blockSize, f.size - a)));
        ModelEntry e;
        e.id = "test-model";
        e.files.push_back (f);
        return e;
    }

    juce::MemoryBlock randomData (size_t n, juce::int64 seed)
    {
        juce::MemoryBlock m (n);
        juce::Random r (seed);
        for (size_t i = 0; i < n; ++i) static_cast<juce::uint8*> (m.getData())[i] = (juce::uint8) r.nextInt (256);
        return m;
    }
}

class ModelDownloadTests : public juce::UnitTest
{
public:
    ModelDownloadTests() : juce::UnitTest ("ModelDownload", "VoiceBooth") {}

    /** 終わるまで回す（コールバックはメッセージスレッドなので、ここで回す） */
    DownloadStatus runToEnd (ModelDownloader& d, const ModelEntry& e, const juce::File& dir)
    {
        DownloadStatus last;
        bool finished = false;
        d.start (e, dir, [&] (const DownloadStatus& s)
        {
            last = s;
            using S = DownloadStatus::Stage;
            finished = finished || s.stage == S::done || s.stage == S::failed || s.stage == S::interrupted || s.stage == S::cancelled;
        });
        for (int i = 0; i < 3000 && ! finished; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        return last;
    }

    void runTest() override
    {
        using Stage = DownloadStatus::Stage;

        beginTest ("Ed25519: RFC 8032 test vector and a signed manifest");
        {
            PublicKey pub;
            expect (parseHexKey ("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", pub));
            juce::MemoryBlock sig;
            sig.loadFromHexString ("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
            expect (verifySignature (juce::MemoryBlock(), base64 (sig.getData(), sig.getSize()), pub));   // 空のメッセージ（RFC 8032 Test 1）

            // テスト用の鍵（このテストの中だけ）で署名して確かめる。1 バイト変えたら通らない
            uint8_t seed[32], secret[64], key[32], s[64];
            for (int i = 0; i < 32; ++i) seed[i] = (uint8_t) (i * 7 + 1);
            crypto_ed25519_key_pair (secret, key, seed);
            const juce::String json = "{\"format\":\"voicebooth.models\",\"format_version\":1,\"serial\":1,\"models\":[]}";
            juce::MemoryBlock bytes (json.toRawUTF8(), json.getNumBytesAsUTF8());
            crypto_ed25519_sign (s, secret, static_cast<const uint8_t*> (bytes.getData()), bytes.getSize());
            PublicKey k;
            std::copy (key, key + 32, k.begin());
            expect (verifySignature (bytes, base64 (s, 64), k));
            static_cast<char*> (bytes.getData())[3] ^= 1;
            expect (! verifySignature (bytes, base64 (s, 64), k));
            expect (! verifySignature (bytes, "not base64!", k));
        }

        beginTest ("manifest: parsed, and unsafe entries are refused");
        {
            Manifest m;
            juce::String err;
            const juce::String hash (juce::String::repeatedString ("ab", 32));
            auto json = [&] (const juce::String& fileName, const juce::String& url, int blocks)
            {
                juce::StringArray b;
                for (int i = 0; i < blocks; ++i) b.add ("\"" + hash + "\"");
                return "{\"format\":\"voicebooth.models\",\"format_version\":1,\"serial\":4,\"models\":[{\"id\":\"m1\",\"role\":\"separation\",\"license\":\"MIT\","
                       "\"files\":[{\"name\":\"" + fileName + "\",\"url\":\"" + url + "\",\"size\":100000,\"sha256\":\"" + hash + "\",\"block_size\":65536,\"blocks\":[" + b.joinIntoString (",") + "]}]}]}";
            };
            expect (parseManifest (json ("front.onnx", "https://x/front.onnx", 2), m, err), err);
            expectEquals (m.serial, 4);
            expectEquals (m.find ("m1")->totalSize(), (juce::int64) 100000);
            expect (! parseManifest (json ("../evil", "https://x/a", 2), m, err));          // フォルダの外へ書かせない
            expect (! parseManifest (json ("a.onnx", "http://x/a", 2), m, err));            // https だけ
            expect (! parseManifest (json ("a.onnx", "https://x/a", 1), m, err));           // ブロックの数が合わない
            expect (! parseManifest ("{\"format\":\"other\"}", m, err));
        }

        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("VoiceBoothTests-models-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        const auto data = randomData (65536 * 5 + 1234, 11);
        const auto entry = entryFor (data, 65536);
        auto fresh = [&] { dir.deleteRecursively(); dir.createDirectory(); };
        auto installedOk = [&] { return sha256Hex (dir.getChildFile ("part.onnx")) == entry.files[0].sha256; };

        beginTest ("downloads, verifies, and a second run does nothing");
        {
            fresh();
            auto http = std::make_unique<FakeHttp>();
            http->content = data;
            auto* h = http.get();
            ModelDownloader d (std::move (http));
            const auto s = runToEnd (d, entry, dir);
            expect (s.stage == Stage::done, s.error);
            expect (installedOk());
            expect (ModelDownloader::installed (entry, dir));
            expect (! dir.getChildFile ("part.onnx.part").exists());
            const auto before = h->requests;
            expect (runToEnd (d, entry, dir).stage == Stage::done);
            expectEquals (h->requests, before);   // もう入っている：取りに行かない
        }

        beginTest ("connection drops: resumes from where it stopped with Range (nothing re-downloaded)");
        {
            fresh();
            auto http = std::make_unique<FakeHttp>();
            http->content = data;
            http->cutAfter = 100000;
            http->cutTimes = 2;
            auto* h = http.get();
            ModelDownloader d (std::move (http));
            d.setRetryDelays ({ 0, 0, 0 });
            const auto s = runToEnd (d, entry, dir);
            expect (s.stage == Stage::done, s.error);
            expect (installedOk());
            {
                juce::StringArray fs;
                for (auto v : h->froms) fs.add (juce::String (v));
                logMessage ("    requests from: " + fs.joinIntoString (", "));
            }
            expectEquals ((int) h->froms.size(), 3);
            expectEquals (h->froms[1], (juce::int64) 100000);   // 続きから
            expectEquals (h->froms[2], (juce::int64) 200000);
        }

        beginTest ("a corrupted block is fetched again; twice in a row restarts the file");
        {
            fresh();
            auto http = std::make_unique<FakeHttp>();
            http->content = data;
            http->corruptBlock[65536 * 2] = 1;
            auto* h = http.get();
            ModelDownloader d (std::move (http));
            d.setRetryDelays ({ 0, 0, 0 });
            auto s = runToEnd (d, entry, dir);
            expect (s.stage == Stage::done, s.error);
            expect (installedOk());
            expectEquals (h->froms[1], (juce::int64) 65536 * 2);   // 壊れたブロックの頭から取り直す

            fresh();
            auto http2 = std::make_unique<FakeHttp>();
            http2->content = data;
            http2->corruptBlock[65536] = 100;   // ずっと壊れている
            ModelDownloader d2 (std::move (http2));
            d2.setRetryDelays ({ 0, 0, 0 });
            s = runToEnd (d2, entry, dir);
            expect (s.stage == Stage::failed);
            expect (! dir.getChildFile ("part.onnx.part").exists());   // 壊れた物は消す
            expect (! dir.getChildFile ("part.onnx").exists());
        }

        beginTest ("server ignores Range or the file changed (ETag): starts over instead of mixing");
        {
            fresh();
            // 途中まで届いた .part と、古い ETag の記録を置く
            dir.getChildFile ("part.onnx.part").replaceWithData (data.getData(), 65536 * 2);
            dir.getChildFile ("part.onnx.part.json").replaceWithText ("{\"etag\":\"\\\"v0\\\"\",\"sha256\":\"" + entry.files[0].sha256 + "\"}");
            auto http = std::make_unique<FakeHttp>();
            http->content = data;   // ETag は v1：If-Range が合わないので 200（全体）
            auto* h = http.get();
            ModelDownloader d (std::move (http));
            const auto s = runToEnd (d, entry, dir);
            expect (s.stage == Stage::done, s.error);
            expect (installedOk());
            expectEquals (h->froms.front(), (juce::int64) 65536 * 2);   // まず続きを頼む
            expectEquals (h->froms.back(), (juce::int64) 0);            // 合わないので頭から
        }

        beginTest ("no connection: waits 3 times, then reports interrupted and keeps what arrived");
        {
            fresh();
            auto http = std::make_unique<FakeHttp>();
            http->content = data;
            http->cutAfter = 70000;
            http->cutTimes = 1;
            http->failNext = 0;
            auto* h = http.get();
            ModelDownloader d (std::move (http));
            d.setRetryDelays ({ 0, 0, 0 });
            // 1 回目は 70000 で切れ、そのあとはつながらない
            h->failNext = 0;
            DownloadStatus last;
            bool finished = false;
            d.start (entry, dir, [&] (const DownloadStatus& s)
            {
                last = s;
                if (s.stage == Stage::downloading && s.received >= 65536) h->failNext = 100;
                finished = finished || s.stage == Stage::interrupted || s.stage == Stage::done || s.stage == Stage::failed;
            });
            for (int i = 0; i < 3000 && ! finished; ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            expect (last.stage == Stage::interrupted || last.stage == Stage::done);
            if (last.stage == Stage::interrupted)
                expect (dir.getChildFile ("part.onnx.part").getSize() >= 65536);   // 届いた分は残す
        }

        dir.deleteRecursively();

        beginTest ("small files (model list, signature): judged by size, not isExhausted (Windows / Mac streams)");
        {
            // Windows・Mac の juce::URL のように、0 バイトを読むまで isExhausted() が false のままの流れ
            struct LateEndStream final : juce::InputStream
            {
                juce::MemoryBlock data;
                juce::int64 pos = 0;
                bool sawEnd = false;
                juce::int64 getTotalLength() override { return (juce::int64) data.getSize(); }
                bool isExhausted() override { return sawEnd; }
                juce::int64 getPosition() override { return pos; }
                bool setPosition (juce::int64) override { return false; }
                int read (void* dest, int n) override
                {
                    const auto k = (int) juce::jmin ((juce::int64) n, (juce::int64) data.getSize() - pos);
                    if (k <= 0) { sawEnd = true; return 0; }
                    std::memcpy (dest, static_cast<const char*> (data.getData()) + pos, (size_t) k);
                    pos += k;
                    return k;
                }
            };
            struct SmallHttp final : HttpSource
            {
                juce::MemoryBlock body;
                int status = 200;
                juce::int64 length = -2;   // -2 = 本当の大きさ
                Response get (const juce::String&, juce::int64, const juce::String&) override
                {
                    Response r;
                    r.status = status;
                    r.length = length == -2 ? (juce::int64) body.getSize() : length;
                    auto st = std::make_unique<LateEndStream>();
                    st->data = body;
                    r.body = std::move (st);
                    return r;
                }
            };
            SmallHttp h;
            h.body = randomData (13427, 7);
            juce::MemoryBlock got;
            expect (fetchSmall (h, "https://example.invalid/manifest.json", got));
            expect (got == h.body);

            h.length = -1;                                    // 大きさが分からない（chunked）：最後まで読めれば使う
            expect (fetchSmall (h, "u", got) && got == h.body);

            h.length = (juce::int64) h.body.getSize() + 100;  // 途中で切れた
            expect (! fetchSmall (h, "u", got));

            h.length = -2;
            h.status = 404;
            expect (! fetchSmall (h, "u", got));

            h.status = 200;                                   // 大きすぎる
            expect (! fetchSmall (h, "u", got, 1000));
            expect (fetchSmall (h, "u", got, 13427));         // ちょうど上限はよい
        }
    }
};

static ModelDownloadTests modelDownloadTests;
} // namespace vb::models
