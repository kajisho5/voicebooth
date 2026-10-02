#pragma once

#include "ModelManifest.h"
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>

/*  モデルのダウンロード（DESIGN 11.7 / B16）。使う人が押した時だけ始める（自動では取りに行かない）
    - 受け取り中は <名前>.part と <名前>.part.json（届いたバイト数・ETag・期待する SHA-256）。終了しても次はそこから
    - 続きは Range: bytes=<届いた数>- と If-Range: <ETag>。返ってきたのが 206 で、Content-Range の頭と全体の大きさ・ETag が合う時だけつなぐ。
      合わない・200（中身が差し替わった／範囲指定が効かない）なら .part を捨てて頭から
    - block_size（8 MB）ごとに SHA-256 を照合し、壊れたブロックだけ取り直す。同じブロックが 2 回続けて壊れたらファイルを頭から、
      それでも合わなければ失敗（壊れた物は消す）。最後にファイル全体の SHA-256 も照合してから名前を変える
    - 通信が切れたら 3 / 10 / 30 秒あけて 3 回まで続きから。だめなら「通信が切れました」（届いた分は残す）
    - 一時停止できる（録音・再生の間）。空き容量は始める前に確かめる
    通信は HttpSource で差し替えられる（テストでは手元の偽物）。裏のスレッドで動く。コールバックはメッセージスレッド */

namespace vb::models
{
/** HTTP の GET（Range つき）。本物は juce::URL、テストは偽物 */
class HttpSource
{
public:
    virtual ~HttpSource() = default;

    struct Response
    {
        int status = 0;                       // 0 = つながらない
        juce::String etag;
        juce::int64 rangeStart = -1, total = -1;   // Content-Range（206 の時）
        juce::int64 length = -1;              // Content-Length
        std::unique_ptr<juce::InputStream> body;
    };

    /** from > 0 なら Range: bytes=from- と If-Range: etag（etag が空なら付けない） */
    virtual Response get (const juce::String& url, juce::int64 from, const juce::String& etag) = 0;
};

/** juce::URL を使う本物 */
std::unique_ptr<HttpSource> makeHttpSource();

struct DownloadStatus
{
    enum class Stage { downloading, waiting, verifying, done, interrupted, failed, cancelled };
    Stage stage = Stage::downloading;
    juce::int64 received = 0, total = 0;     // すべてのファイルの合計（バイト）
    double bytesPerSecond = 0.0;
    int retryInSeconds = 0, attempt = 0;      // waiting の時：何秒後に何回目
    juce::String error;                       // failed の時（英語の短い文）
    bool paused = false;
};

class ModelDownloader : private juce::Thread
{
public:
    explicit ModelDownloader (std::unique_ptr<HttpSource>);
    ~ModelDownloader() override;

    /** folder にモデルのファイルを置く（無ければ作る）。動いていれば false */
    bool start (const ModelEntry&, const juce::File& folder, std::function<void (const DownloadStatus&)> onStatus);
    void cancel();
    void setPaused (bool);
    bool isBusy() const { return isThreadRunning(); }

    /** そのフォルダにモデルがそろっているか（大きさだけで見る。中身は入れる時に照合済み） */
    static bool installed (const ModelEntry&, const juce::File& folder);

    /** テスト用：待ち時間（秒）を短くする */
    void setRetryDelays (std::vector<int> seconds) { retryDelays = std::move (seconds); }

private:
    void run() override;
    bool downloadFile (const ModelFile&, juce::int64 doneBefore);
    void report (DownloadStatus);

    std::unique_ptr<HttpSource> http;
    ModelEntry entry;
    juce::File folder;
    std::function<void (const DownloadStatus&)> onStatus;
    std::atomic<bool> paused { false };
    std::vector<int> retryDelays { 3, 10, 30 };
    DownloadStatus status;
    double windowStart = 0.0;
    juce::int64 windowBytes = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
} // namespace vb::models
