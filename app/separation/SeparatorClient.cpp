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
    /** 分けた ONNX（front・layer0..N-1・head）がそろっているか。層の数は parts.json（無ければ前の既定の 6） */
    bool partsInstalled (const juce::File& dir)
    {
        int layers = 6;
        if (auto* o = juce::JSON::parse (dir.getChildFile ("parts.json")).getDynamicObject())
            if (const int n = o->getProperty ("layers"); n > 0)
                layers = juce::jmin (64, n);
        juce::StringArray names { "front", "head" };
        for (int i = 0; i < layers; ++i)
            names.add ("layer" + juce::String (i));
        for (auto& n : names)
            if (! dir.getChildFile (n + ".onnx").existsAsFile())
                return false;
        return true;
    }

    juce::File modelsDir (const juce::String& kind)
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("VoiceBooth").getChildFile ("Models").getChildFile (kind);
    }

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

juce::String SeparatorClient::modelId() { return "bs-roformer-anvuew-ft1-int8-1"; }

juce::File SeparatorClient::modelFolder()
{
    if (const auto env = juce::SystemStats::getEnvironmentVariable ("VB_SEPARATION_MODEL", {}); env.isNotEmpty())
        return juce::File (env);
    return modelsDir ("separation").getChildFile (modelId());
}

bool SeparatorClient::modelInstalled()
{
    return partsInstalled (modelFolder());
}

juce::String SeparatorClient::karaokeModelId() { return "bs-roformer-anvuew-karaoke-int8-1"; }

juce::File SeparatorClient::karaokeModelFolder()
{
    if (const auto env = juce::SystemStats::getEnvironmentVariable ("VB_KARAOKE_MODEL", {}); env.isNotEmpty())
        return juce::File (env);
    return modelsDir ("karaoke").getChildFile (karaokeModelId());
}

bool SeparatorClient::karaokeInstalled()
{
    return partsInstalled (karaokeModelFolder());
}

bool SeparatorClient::available()
{
    return executable().existsAsFile() && modelInstalled();
}

juce::String SeparatorClient::pitchModelId() { return "rmvpe-int8-1"; }

juce::File SeparatorClient::pitchModelFile()
{
    if (const auto env = juce::SystemStats::getEnvironmentVariable ("VB_PITCH_MODEL", {}); env.isNotEmpty())
        return juce::File (env);
    return modelsDir ("pitch").getChildFile (pitchModelId()).getChildFile ("rmvpe.onnx");
}

bool SeparatorClient::pitchAvailable()
{
    return executable().existsAsFile() && pitchModelFile().existsAsFile();
}

bool SeparatorClient::runPitch (const juce::File& wav, std::vector<std::pair<float, float>>& frames)
{
    frames.clear();
    if (! pitchAvailable() || ! wav.existsAsFile())
        return false;
    const auto out = wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".pitch.txt");
    out.deleteFile();
    const auto threads = juce::jlimit (2, 6, juce::SystemStats::getNumCpus() / 2);   // 分離と同じ（録音・再生を止めない）
    juce::ChildProcess child;
    if (! child.start (juce::StringArray { executable().getFullPathName(), "--pitch", pitchModelFile().getFullPathName(),
                                           "--in", wav.getFullPathName(), "--out", out.getFullPathName(),
                                           "--threads", juce::String (threads), "--parent-pid", processId() },
                       juce::ChildProcess::wantStdOut))
        return false;
    child.readAllProcessOutput();   // 終わるまで待つ（4 分の曲で 10〜30 秒）
    if (child.getExitCode() != 0 || ! out.existsAsFile())
        return false;

    juce::StringArray lines;
    lines.addLines (out.loadFileAsString());
    out.deleteFile();
    if (lines.isEmpty() || ! lines[0].startsWith ("vbpitch 1 "))
        return false;
    const auto n = lines[0].fromLastOccurrenceOf (" ", false, false).getIntValue();
    if (n <= 0 || lines.size() < n + 1)
        return false;
    frames.reserve ((size_t) n);
    for (int i = 1; i <= n; ++i)
        frames.emplace_back (lines[i].upToFirstOccurrenceOf (" ", false, false).getFloatValue(),
                             lines[i].fromFirstOccurrenceOf (" ", false, false).getFloatValue());
    return true;
}

bool SeparatorClient::start (const juce::File& in, const juce::File& outVocals, const juce::File& outBacking, Callbacks cb,
                             const juce::File& outLead)
{
    if (isThreadRunning())
        return false;
    input = in;
    vocals = outVocals;
    backing = outBacking;
    lead = outLead != juce::File() && karaokeInstalled() ? outLead : juce::File();
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
    if (lead != juce::File())   // ハモリのお手本：続けてリードボーカルを取る（2026-10-02）
        args.addArray ({ "--karaoke", karaokeModelFolder().getFullPathName(), "--lead", lead.getFullPathName() });
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
