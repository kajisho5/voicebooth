#include "LyricsClient.h"

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

namespace vb::lyrics
{
namespace
{
    juce::String processId()
    {
       #if JUCE_WINDOWS
        return juce::String ((juce::int64) GetCurrentProcessId());
       #else
        return juce::String ((juce::int64) getpid());
       #endif
    }
}

LyricsClient::LyricsClient() : juce::Thread ("VoiceBooth lyrics") {}

LyricsClient::~LyricsClient()
{
    *alive = false;
    stop();
}

juce::File LyricsClient::executable()
{
    const auto self = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
   #if JUCE_WINDOWS
    return self.getSiblingFile ("VoiceBoothLyrics.exe");
   #else
    return self.getSiblingFile ("VoiceBoothLyrics");
   #endif
}

juce::String LyricsClient::modelId() { return "whisper-small-1"; }

juce::File LyricsClient::modelFolder()
{
    if (const auto env = juce::SystemStats::getEnvironmentVariable ("VB_LYRICS_MODEL", {}); env.isNotEmpty())
        return juce::File (env);
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("VoiceBooth").getChildFile ("Models").getChildFile ("lyrics").getChildFile (modelId());
}

bool LyricsClient::cpuSupported()
{
   #if JUCE_INTEL
    return juce::SystemStats::hasAVX2() && juce::SystemStats::hasFMA3();
   #else
    return true;
   #endif
}

bool LyricsClient::available()
{
    return executable().existsAsFile() && modelInstalled() && cpuSupported();
}

juce::String LyricsClient::languageFor (const juce::String& text)
{
    int kana = 0, hangul = 0, han = 0, latin = 0;
    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = *p;
        if ((c >= 0x3041 && c <= 0x30FF)) ++kana;
        else if (c >= 0xAC00 && c <= 0xD7A3) ++hangul;
        else if (c >= 0x4E00 && c <= 0x9FFF) ++han;
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) ++latin;
    }
    if (kana > 0)   return "ja";   // かなが少しでもあれば日本語（漢字が多くても）
    if (hangul > 0) return "ko";
    if (han > 0)    return "zh";
    return latin > 0 ? "en" : "auto";
}

bool LyricsClient::start (const juce::File& in, const juce::String& prompt, const juce::String& lang, Callbacks cb)
{
    if (isThreadRunning())
        return false;
    input = in;
    language = lang;
    callbacks = std::move (cb);
    pieces.clear();
    promptFile = input.getSiblingFile ("prompt.txt");
    promptFile.replaceWithText (prompt, false, false, nullptr);
    startThread (juce::Thread::Priority::low);
    return true;
}

void LyricsClient::stop()
{
    signalThreadShouldExit();
    {
        const juce::ScopedLock sl (childLock);
        if (child != nullptr)
            child->kill();
    }
    stopThread (5000);
}

void LyricsClient::finish (bool ok, const juce::String& error)
{
    std::weak_ptr<bool> weak = alive;
    auto done = callbacks.done;
    auto result = std::move (pieces);
    juce::MessageManager::callAsync ([weak, done, ok, error, result]
    {
        if (! weak.expired() && done)
            done (ok, error, result);
    });
}

void LyricsClient::run()
{
    // 認識はコアを使い切らない（録音・再生・画面を止めない）：論理コアの半分、2〜6
    const auto threads = juce::jlimit (2, 6, juce::SystemStats::getNumCpus() / 2);
    juce::StringArray args { executable().getFullPathName(),
                             "--model", modelFile().getFullPathName(),
                             "--in", input.getFullPathName(),
                             "--language", language,
                             "--prompt-file", promptFile.getFullPathName(),
                             "--threads", juce::String (threads),
                             "--parent-pid", processId() };
    {
        const juce::ScopedLock sl (childLock);
        child = std::make_unique<juce::ChildProcess>();
        if (! child->start (args, juce::ChildProcess::wantStdOut))
        {
            child = nullptr;
            finish (false, "can't start the recogniser");
            return;
        }
    }

    std::weak_ptr<bool> weak = alive;
    auto progressCb = callbacks.progress;
    juce::String error;
    bool doneSeen = false;
    juce::MemoryBlock line;
    // 1 バイトずつ読む（SeparatorClient と同じ理由：readProcessOutput は頼んだ数がそろうまで待つ）。行は UTF-8 のまま集めてから文字にする
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
        if (buffer[0] != '\n')
        {
            line.append (buffer, 1);
            continue;
        }
        const auto text = juce::String::fromUTF8 ((const char*) line.getData(), (int) line.getSize()).trimEnd();
        line.reset();
        if (text.startsWith ("progress "))
        {
            const auto p = (float) text.substring (9).getDoubleValue();
            juce::MessageManager::callAsync ([weak, progressCb, p] { if (! weak.expired() && progressCb) progressCb (p); });
        }
        else if (text.startsWith ("piece "))
        {
            const auto parts = juce::StringArray::fromTokens (text.substring (6), "\t", {});
            if (parts.size() >= 3)
                pieces.push_back ({ parts[2], parts[0].getDoubleValue(), parts[1].getDoubleValue() });
        }
        else if (text == "done")             doneSeen = true;
        else if (text.startsWith ("error ")) error = text.substring (6);
    }

    const bool stopped = threadShouldExit();
    {
        const juce::ScopedLock sl (childLock);
        if (child != nullptr && child->isRunning())
            child->kill();
        child = nullptr;
    }
    promptFile.deleteFile();
    if (stopped)       finish (false, "stopped");
    else if (doneSeen) finish (true, {});
    else               finish (false, error.isNotEmpty() ? error : juce::String ("the recogniser stopped unexpectedly"));
}
} // namespace vb::lyrics
