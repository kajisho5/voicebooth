/*  VoiceBoothSeparator：ボーカル分離の別プロセス（DESIGN 11.3 / B16）
    本体から子プロセスとして起動する。重い処理とメモリ（約 2 GB）を本体から切り離し、終われば OS に返す。
    リアルタイムでは使わない（11.5）。

    使い方：
      VoiceBoothSeparator --model <フォルダ> --in <44.1 kHz ステレオの WAV> --vocals <出力 WAV> --backing <出力 WAV>
                          [--overlap 2] [--threads 0] [--parent-pid <本体の PID>] [--stdin-control]
    出力（標準出力、1 行ずつ）：
      ready <チャンクの数>        モデルを読んだ
      chunk <秒>                 最初のチャンクにかかった秒（時間の見込み = これ × チャンクの数。11.6.1）
      progress <0..1>
      done
      error <理由>               （終了コード 1）
    中止：--stdin-control を付けた時（本体から起動する時）は、標準入力に "stop" が来るか、標準入力が閉じたら止める（本体が落ちた時も止まる）

    モデル（Mel-Band RoFormer、Kimberley Jensen、MIT）は層ごとに分けた ONNX 8 個：
      front.onnx（spec → x）、layer0..5.onnx（x → y）、head.onnx（x, spec → est）
    ONNX Runtime のメモリ再利用は C/C++ API で切れないため、モデルを分けて順に回し、メモリアリーナを切る
    （1 つのグラフのまま既定の設定だとピーク 6〜12 GB、分けてアリーナを切ると約 2 GB。2026-10-02 実測） */

#include <juce_audio_formats/juce_audio_formats.h>
#include <onnxruntime_cxx_api.h>
#include "analysis/Separation.h"
#include <atomic>
#include <iostream>
#include <thread>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#else
 #include <signal.h>
 #include <cerrno>
#endif

namespace
{
namespace sep = vb::analysis::separation;

std::atomic<bool> stopRequested { false };

void say (const juce::String& line)
{
    std::cout << line.toStdString() << std::endl;   // 1 行ずつ flush（本体が読む）
}

/** 本体（--parent-pid）が生きているか（落ちたら分離も止める） */
bool parentAlive (juce::int64 pid)
{
    if (pid <= 0) return true;
   #if JUCE_WINDOWS
    auto h = OpenProcess (SYNCHRONIZE, FALSE, (DWORD) pid);
    if (h == nullptr) return false;
    const bool running = WaitForSingleObject (h, 0) == WAIT_TIMEOUT;
    CloseHandle (h);
    return running;
   #else
    return ::kill ((pid_t) pid, 0) == 0 || errno != ESRCH;
   #endif
}

int fail (const juce::String& why)
{
    say ("error " + why);
    return 1;
}

juce::String arg (const juce::StringArray& args, const juce::String& name, const juce::String& def = {})
{
    const auto i = args.indexOf (name);
    return i >= 0 && i + 1 < args.size() ? args[i + 1] : def;
}

/** 層ごとの ONNX を順に回す */
class Model
{
public:
    Model (const juce::File& folder, int threads)
        : env (ORT_LOGGING_LEVEL_WARNING, "VoiceBoothSeparator")
    {
        Ort::SessionOptions so;
        so.SetIntraOpNumThreads (threads);
        so.SetInterOpNumThreads (1);
        so.DisableCpuMemArena();   // セッションごとのアリーナを持ち越さない（分けた意味がなくなる）
        so.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

        juce::StringArray names { "front" };
        for (int i = 0; i < 6; ++i) names.add ("layer" + juce::String (i));
        names.add ("head");
        for (auto& n : names)
        {
            const auto f = folder.getChildFile (n + ".onnx");
            if (! f.existsAsFile())
                throw std::runtime_error ("missing " + f.getFullPathName().toStdString());
           #if JUCE_WINDOWS
            sessions.emplace_back (env, f.getFullPathName().toWideCharPointer(), so);
           #else
            sessions.emplace_back (env, f.getFullPathName().toRawUTF8(), so);
           #endif
        }
    }

    bool run (const std::vector<float>& spec, int frames, std::vector<float>& est)
    {
        auto mem = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 5> specShape { 1, 2, sep::bins, frames, 2 };
        auto specTensor = Ort::Value::CreateTensor<float> (mem, const_cast<float*> (spec.data()), spec.size(), specShape.data(), specShape.size());

        const char* inSpec[] = { "spec" };
        const char* outX[] = { "x" };
        auto x = sessions[0].Run (Ort::RunOptions{ nullptr }, inSpec, &specTensor, 1, outX, 1);
        const char* inX[] = { "x" };
        const char* outY[] = { "y" };
        for (size_t i = 1; i + 1 < sessions.size(); ++i)
        {
            if (stopRequested.load()) return false;
            x = sessions[i].Run (Ort::RunOptions{ nullptr }, inX, &x[0], 1, outY, 1);
        }
        const char* inHead[] = { "x", "spec" };
        const char* outEst[] = { "est" };
        Ort::Value inputs[] = { std::move (x[0]), std::move (specTensor) };
        auto out = sessions.back().Run (Ort::RunOptions{ nullptr }, inHead, inputs, 2, outEst, 1);
        const auto* p = out[0].GetTensorData<float>();
        est.assign (p, p + spec.size());
        return ! stopRequested.load();
    }

private:
    Ort::Env env;
    std::vector<Ort::Session> sessions;
};

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& b)
{
    const auto temp = file.getSiblingFile (file.getFileName() + ".part");
    temp.deleteFile();
    {
        auto fs = std::make_unique<juce::FileOutputStream> (temp);
        if (! fs->openedOk()) return false;
        std::unique_ptr<juce::OutputStream> stream (fs.release());
        juce::WavAudioFormat wav;
        auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sep::sampleRate).withNumChannels (2)
                                                .withBitsPerSample (32).withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        if (w == nullptr || ! w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples())) return false;
    }
    file.deleteFile();
    return temp.moveFileTo (file);
}
} // namespace

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (juce::CharPointer_UTF8 (argv[i]));

    const juce::File modelDir (arg (args, "--model")), in (arg (args, "--in")), outVocals (arg (args, "--vocals")), outBacking (arg (args, "--backing"));
    const int overlap = juce::jlimit (1, 4, arg (args, "--overlap", "2").getIntValue());
    const int threads = juce::jmax (0, arg (args, "--threads", "0").getIntValue());
    if (! modelDir.isDirectory() || ! in.existsAsFile() || outVocals == juce::File() || outBacking == juce::File())
        return fail ("usage: --model <dir> --in <wav> --vocals <wav> --backing <wav> [--overlap 2] [--threads 0]");

    // 本体が落ちたら止める
    if (const auto parent = arg (args, "--parent-pid").getLargeIntValue(); parent > 0)
    {
        std::thread ([parent]
        {
            while (! stopRequested.load())
            {
                if (! parentAlive (parent)) { stopRequested = true; break; }
                std::this_thread::sleep_for (std::chrono::seconds (1));
            }
        }).detach();
    }

    // 本体からの中止（stop の行、または標準入力が閉じた）
    if (args.contains ("--stdin-control"))
    {
        std::thread watcher ([]
        {
            std::string line;
            while (std::getline (std::cin, line))
                if (line == "stop") break;
            stopRequested = true;
        });
        watcher.detach();
    }

    juce::AudioBuffer<float> mix;
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (new juce::FileInputStream (in), true));
        if (r == nullptr) return fail ("can't read " + in.getFullPathName());
        if (std::abs (r->sampleRate - sep::sampleRate) > 0.5) return fail ("input must be 44100 Hz");
        if (r->lengthInSamples <= 0 || r->lengthInSamples > 60 * 60 * 44100) return fail ("bad length");
        mix.setSize (2, (int) r->lengthInSamples);
        r->read (&mix, 0, mix.getNumSamples(), 0, true, true);
        if (r->numChannels == 1) mix.copyFrom (1, 0, mix, 0, 0, mix.getNumSamples());
    }

    std::unique_ptr<Model> model;
    try { model = std::make_unique<Model> (modelDir, threads); }
    catch (const std::exception& e) { return fail (juce::String ("can't load model: ") + e.what()); }

    say ("ready " + juce::String (sep::chunkCount (mix.getNumSamples(), overlap)));

    bool first = true;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    const sep::Model run = [&] (const std::vector<float>& spec, int frames, std::vector<float>& est)
    {
        try
        {
            const bool ok = model->run (spec, frames, est);
            if (first)
            {
                first = false;
                say ("chunk " + juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 2));
            }
            return ok;
        }
        catch (const std::exception& e)
        {
            say (juce::String ("error ") + e.what());
            return false;
        }
    };

    juce::AudioBuffer<float> vocals;
    if (! sep::demix (mix, run, overlap, vocals, [] (float p) { say ("progress " + juce::String (p, 4)); return ! stopRequested.load(); }))
        return fail (stopRequested.load() ? "stopped" : "separation failed");

    // 伴奏 = 元の音 − ボーカル
    juce::AudioBuffer<float> backing (2, mix.getNumSamples());
    for (int ch = 0; ch < 2; ++ch)
    {
        backing.copyFrom (ch, 0, mix, ch, 0, mix.getNumSamples());
        backing.addFrom (ch, 0, vocals, ch, 0, mix.getNumSamples(), -1.0f);
    }
    if (! writeWav (outVocals, vocals) || ! writeWav (outBacking, backing))
        return fail ("can't write output");
    say ("done");
    std::_Exit (0);   // 中止を待つ標準入力のスレッドを待たずに終わる
}
