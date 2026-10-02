/*  VoiceBoothLyrics：歌の認識（B17。DESIGN 11.3）。本体とは別の実行ファイル（whisper.cpp、MIT）。JUCE は使わない。

      VoiceBoothLyrics --model <ggml の .bin> --in <16 kHz モノラルの WAV（32bit float か 16bit）>
                       [--language ja|en|ko|zh|auto] [--prompt-file <UTF-8 の文字>] [--threads N] [--parent-pid P] [--stdin-control]

    標準出力（1 行ずつ、UTF-8）：
      ready <秒>                        読み込めた（音の長さ）
      progress <0..1>
      piece <始まり秒>\t<終わり秒>\t<文字>   認識した文字の塊（トークンをつないで、UTF-8 として切れ目のない所で出す）
      done | error <理由>

    - 歌詞（--prompt-file）は最初の窓の手がかり（initial prompt）にする。歌詞と同じ字で書き起こしやすくなる
    - 本体が落ちたら止まる（--parent-pid）。--stdin-control なら "stop" の行か標準入力が閉じたら止まる */

#include "whisper.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#if defined (_WIN32)
 #define NOMINMAX
 #include <windows.h>
#else
 #include <signal.h>
 #include <unistd.h>
#endif

namespace
{
    std::atomic<bool> stopRequested { false };

    void say (const std::string& line)
    {
        std::fwrite (line.data(), 1, line.size(), stdout);
        std::fputc ('\n', stdout);
        std::fflush (stdout);
    }

    int fail (const std::string& why)
    {
        say ("error " + why);
        return 1;
    }

    std::string arg (int argc, char** argv, const char* name, const std::string& fallback = {})
    {
        for (int i = 1; i + 1 < argc; ++i)
            if (std::strcmp (argv[i], name) == 0)
                return argv[i + 1];
        return fallback;
    }

    bool flag (int argc, char** argv, const char* name)
    {
        for (int i = 1; i < argc; ++i)
            if (std::strcmp (argv[i], name) == 0)
                return true;
        return false;
    }

    /** 16 kHz モノラルの WAV（32bit float / 16bit PCM）を読む */
    bool readWav (const std::string& path, std::vector<float>& out, std::string& error)
    {
        std::ifstream f (path, std::ios::binary);
        if (! f) { error = "can't open input"; return false; }
        std::vector<char> data ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
        auto u16 = [&] (size_t p) { return (uint32_t) (uint8_t) data[p] | ((uint32_t) (uint8_t) data[p + 1] << 8); };
        auto u32 = [&] (size_t p) { return u16 (p) | (u16 (p + 2) << 16); };
        if (data.size() < 12 || std::memcmp (data.data(), "RIFF", 4) != 0 || std::memcmp (data.data() + 8, "WAVE", 4) != 0)
        { error = "not a WAV file"; return false; }

        uint32_t format = 0, channels = 0, rate = 0, bits = 0;
        size_t p = 12;
        while (p + 8 <= data.size())
        {
            const auto size = (size_t) u32 (p + 4);
            const auto body = p + 8;
            if (body + size > data.size()) break;
            if (std::memcmp (data.data() + p, "fmt ", 4) == 0 && size >= 16)
            {
                format = u16 (body); channels = u16 (body + 2); rate = u32 (body + 4); bits = u16 (body + 14);
                if (format == 0xFFFE && size >= 40) format = u16 (body + 24);   // WAVE_FORMAT_EXTENSIBLE
            }
            else if (std::memcmp (data.data() + p, "data", 4) == 0)
            {
                if (rate != 16000 || channels != 1) { error = "input must be 16 kHz mono"; return false; }
                if (format == 3 && bits == 32)
                {
                    out.resize (size / 4);
                    std::memcpy (out.data(), data.data() + body, out.size() * 4);
                    return true;
                }
                if (format == 1 && bits == 16)
                {
                    out.resize (size / 2);
                    for (size_t i = 0; i < out.size(); ++i)
                        out[i] = (float) (int16_t) u16 (body + i * 2) / 32768.0f;
                    return true;
                }
                error = "input must be 32-bit float or 16-bit PCM";
                return false;
            }
            p = body + size + (size & 1);
        }
        error = "no audio data";
        return false;
    }

    bool parentAlive (long pid)
    {
       #if defined (_WIN32)
        auto h = OpenProcess (SYNCHRONIZE, FALSE, (DWORD) pid);
        if (h == nullptr) return false;
        const auto r = WaitForSingleObject (h, 0);
        CloseHandle (h);
        return r == WAIT_TIMEOUT;
       #else
        return kill ((pid_t) pid, 0) == 0;
       #endif
    }

    /** UTF-8 として、最後まで文字がそろっている長さ（途中で切れた多バイト文字は残す） */
    size_t completeUtf8 (const std::string& s)
    {
        size_t i = 0, ok = 0;
        while (i < s.size())
        {
            const auto c = (uint8_t) s[i];
            const size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
            if (i + n > s.size()) break;
            i += n;
            ok = i;
        }
        return ok;
    }

    std::string clean (std::string s)
    {
        for (auto& c : s)
            if (c == '\t' || c == '\n' || c == '\r') c = ' ';
        return s;
    }
}

int main (int argc, char** argv)
{
    const auto modelPath = arg (argc, argv, "--model"), inPath = arg (argc, argv, "--in");
    if (modelPath.empty() || inPath.empty())
        return fail ("usage: --model <ggml .bin> --in <16 kHz mono wav> [--language ja] [--prompt-file f] [--threads N]");

    const auto language = arg (argc, argv, "--language", "auto");
    const auto threads = std::max (1, std::atoi (arg (argc, argv, "--threads", "4").c_str()));
    const auto parentPid = std::atol (arg (argc, argv, "--parent-pid", "0").c_str());

    std::string prompt;
    if (const auto pf = arg (argc, argv, "--prompt-file"); ! pf.empty())
    {
        std::ifstream f (pf, std::ios::binary);
        prompt.assign ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    }

    // 止める合図：本体が落ちた・"stop" の行・標準入力が閉じた
    if (parentPid > 0)
        std::thread ([parentPid] { while (! stopRequested) { if (! parentAlive (parentPid)) stopRequested = true; std::this_thread::sleep_for (std::chrono::milliseconds (500)); } }).detach();
    if (flag (argc, argv, "--stdin-control"))
        std::thread ([] { char line[64]; while (std::fgets (line, sizeof (line), stdin) != nullptr) if (std::strncmp (line, "stop", 4) == 0) break; stopRequested = true; }).detach();

    std::vector<float> pcm;
    std::string error;
    if (! readWav (inPath, pcm, error))
        return fail (error);

    whisper_log_set ([] (ggml_log_level, const char*, void*) {}, nullptr);   // ライブラリのログは出さない（標準出力は約束の行だけ）
    auto cparams = whisper_context_default_params();
    cparams.use_gpu = true;   // Apple シリコンは Metal（無ければ CPU）
    auto* ctx = whisper_init_from_file_with_params (modelPath.c_str(), cparams);
    if (ctx == nullptr)
        return fail ("can't load the model");

    say ("ready " + std::to_string ((double) pcm.size() / 16000.0));

    auto params = whisper_full_default_params (WHISPER_SAMPLING_BEAM_SEARCH);
    params.n_threads = threads;
    params.language = language.c_str();
    params.detect_language = language == "auto";
    params.translate = false;
    params.no_context = false;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.token_timestamps = true;
    params.suppress_nst = true;
    params.initial_prompt = prompt.empty() ? nullptr : prompt.c_str();
    params.progress_callback = [] (whisper_context*, whisper_state*, int p, void*) { say ("progress " + std::to_string (p / 100.0)); };
    params.abort_callback = [] (void*) { return stopRequested.load(); };

    if (whisper_full (ctx, params, pcm.data(), (int) pcm.size()) != 0)
    {
        whisper_free (ctx);
        return fail (stopRequested ? "stopped" : "recognition failed");
    }

    // トークンをつないで、文字がそろった所で塊にする（日本語は 1 文字が複数のトークンに割れることがある）
    const auto eot = whisper_token_eot (ctx);
    for (int s = 0; s < whisper_full_n_segments (ctx); ++s)
    {
        std::string pending;
        double t0 = -1.0, t1 = 0.0;
        for (int k = 0; k < whisper_full_n_tokens (ctx, s); ++k)
        {
            const auto data = whisper_full_get_token_data (ctx, s, k);
            if (data.id >= eot)
                continue;   // 時刻・特別なトークン
            if (t0 < 0.0) t0 = data.t0 / 100.0;
            t1 = data.t1 / 100.0;
            pending += whisper_full_get_token_text (ctx, s, k);
            const auto ok = completeUtf8 (pending);
            if (ok == pending.size())
            {
                if (! pending.empty())
                    say ("piece " + std::to_string (t0) + "\t" + std::to_string (std::max (t0, t1)) + "\t" + clean (pending));
                pending.clear();
                t0 = -1.0;
            }
        }
    }
    whisper_free (ctx);
    say ("done");
    return 0;
}
