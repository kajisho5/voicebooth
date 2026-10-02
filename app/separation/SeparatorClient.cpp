#include "SeparatorClient.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#else
 #include <unistd.h>
#endif

namespace vb::separation
{
namespace
{
    const char* const partNames[] = { "front", "layer0", "layer1", "layer2", "layer3", "layer4", "layer5", "head" };

    juce::String processId()
    {
       #if JUCE_WINDOWS
        return juce::String ((juce::int64) GetCurrentProcessId());
       #else
        return juce::String ((juce::int64) getpid());
       #endif
    }
}

SeparatorClient::SeparatorClient() : juce::Thread ("VoiceBooth separation") {}

SeparatorClient::~SeparatorClient()
{
    *alive = false;
    stop();
}

juce::File SeparatorClient::executable()
{
    const auto self = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
   #if JUCE_WINDOWS
    return self.getSiblingFile ("VoiceBoothSeparator.exe");
   #else
    return self.getSiblingFile ("VoiceBoothSeparator");
   #endif
}

juce::String SeparatorClient::modelId() { return "mel-band-roformer-kj-int8-1"; }

juce::File SeparatorClient::modelFolder()
{
    if (const auto env = juce::SystemStats::getEnvironmentVariable ("VB_SEPARATION_MODEL", {}); env.isNotEmpty())
        return juce::File (env);
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("VoiceBooth").getChildFile ("Models").getChildFile ("separation").getChildFile (modelId());
}

bool SeparatorClient::modelInstalled()
{
    const auto dir = modelFolder();
    for (auto* n : partNames)
        if (! dir.getChildFile (juce::String (n) + ".onnx").existsAsFile())
            return false;
    return true;
}

bool SeparatorClient::available()
{
    return executable().existsAsFile() && modelInstalled();
}

bool SeparatorClient::start (const juce::File& in, const juce::File& outVocals, const juce::File& outBacking, Callbacks cb)
{
    if (isThreadRunning())
        return false;
    input = in;
    vocals = outVocals;
    backing = outBacking;
    callbacks = std::move (cb);
    startThread (juce::Thread::Priority::low);
    return true;
}

void SeparatorClient::stop()
{
    signalThreadShouldExit();
    {
        const juce::ScopedLock sl (childLock);
        if (child != nullptr)
            child->kill();
    }
    stopThread (5000);
}

void SeparatorClient::finish (bool ok, const juce::String& error)
{
    std::weak_ptr<bool> weak = alive;
    auto done = callbacks.done;
    juce::MessageManager::callAsync ([weak, done, ok, error] { if (! weak.expired() && done) done (ok, error); });
}

void SeparatorClient::run()
{
    // 分離はコアを使い切らない（録音・再生・画面を止めない）：論理コアの半分、2〜6
    const auto threads = juce::jlimit (2, 6, juce::SystemStats::getNumCpus() / 2);
    juce::StringArray args { executable().getFullPathName(),
                             "--model", modelFolder().getFullPathName(),
                             "--in", input.getFullPathName(),
                             "--vocals", vocals.getFullPathName(),
                             "--backing", backing.getFullPathName(),
                             "--threads", juce::String (threads),
                             "--parent-pid", processId() };
    {
        const juce::ScopedLock sl (childLock);
        child = std::make_unique<juce::ChildProcess>();
        if (! child->start (args, juce::ChildProcess::wantStdOut))
        {
            child = nullptr;
            finish (false, "can't start the separator");
            return;
        }
    }

    std::weak_ptr<bool> weak = alive;
    auto progressCb = callbacks.progress;
    auto report = [weak, progressCb] (float p, double eta)
    {
        juce::MessageManager::callAsync ([weak, progressCb, p, eta] { if (! weak.expired() && progressCb) progressCb (p, eta); });
    };

    juce::String pending, error;
    bool doneSeen = false;
    int chunks = 0;
    double firstChunk = -1.0;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    // 1 バイトずつ読む：readProcessOutput は（POSIX では fread で）頼んだ数がそろうまで待つので、
    // 大きく頼むと短い progress の行が終わるまで届かない。出力は数百バイトなので 1 バイトずつで足りる
    char buffer[1];
    for (;;)
    {
        const auto n = child->readProcessOutput (buffer, 1);
        if (n <= 0)
        {
            if (! child->isRunning() || threadShouldExit())
                break;
            wait (50);
            continue;
        }
        pending += juce::String::fromUTF8 (buffer, n);
        for (int nl; (nl = pending.indexOfChar ('\n')) >= 0;)
        {
            const auto line = pending.substring (0, nl).trim();
            pending = pending.substring (nl + 1);
            if (line.startsWith ("ready "))       chunks = line.substring (6).getIntValue();
            else if (line.startsWith ("chunk "))  { firstChunk = line.substring (6).getDoubleValue(); report (0.0f, firstChunk * juce::jmax (0, chunks - 1)); }
            else if (line.startsWith ("progress "))
            {
                const auto p = (float) line.substring (9).getDoubleValue();
                const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
                // 最初の 1 個は準備込みで遅いので、2 個目からは経過時間で見込む（11.6.1）
                const auto eta = p > 0.05f ? elapsed / p * (1.0 - p) : (firstChunk > 0.0 ? firstChunk * juce::jmax (0, chunks) * (1.0 - p) : -1.0);
                report (p, eta);
            }
            else if (line == "done")             doneSeen = true;
            else if (line.startsWith ("error ")) error = line.substring (6);
        }
    }

    const bool stopped = threadShouldExit();
    {
        const juce::ScopedLock sl (childLock);
        if (child != nullptr && child->isRunning())
            child->kill();
        child = nullptr;
    }
    if (stopped)                 finish (false, "stopped");
    else if (doneSeen)           finish (vocals.existsAsFile() && backing.existsAsFile(), "missing output");
    else                         finish (false, error.isNotEmpty() ? error : juce::String ("the separator stopped unexpectedly"));
}
} // namespace vb::separation
