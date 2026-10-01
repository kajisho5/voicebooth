#include "DummySession.h"

namespace vb::dummy
{
namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr int hop = 480;                // 10 ms @ 48 kHz
    constexpr double singerLagSec = 0.04;   // 自分は 40 ms 遅れて入る

    float smoothstep (float x) { x = juce::jlimit (0.0f, 1.0f, x); return x * x * (3.0f - 2.0f * x); }

    /** お手本メロディ（秒, 長さ秒, MIDI, ビブラート, 子音） */
    struct NoteDef { double t, len; float midi; bool vib, cons; };

    const NoteDef melody[] = {
        // Aメロ 1
        { 31.00, 0.50, 55, false, true  }, { 31.50, 0.50, 57, false, false }, { 32.00, 0.50, 60, false, true  },
        { 32.50, 1.00, 59, true,  false }, { 33.50, 0.50, 57, false, true  }, { 34.00, 1.00, 55, true,  false },
        // Aメロ 2「同じ穴の無地 投げ所」
        { 35.50, 0.50, 57, false, true  }, { 36.00, 0.50, 60, false, false }, { 36.50, 0.50, 60, false, true  },
        { 37.00, 0.75, 62, false, true  }, { 37.75, 2.00, 64, true,  false }, { 39.75, 0.50, 62, false, true  },
        { 40.25, 0.75, 60, false, false },
        // Bメロ「抱っこしてください まだ」
        { 41.50, 0.50, 60, false, true  }, { 42.00, 0.50, 62, false, true  }, { 42.50, 0.50, 64, false, false },
        { 43.00, 0.50, 65, false, true  }, { 43.50, 1.00, 67, true,  false }, { 44.50, 0.50, 65, false, true  },
        { 45.00, 0.50, 64, false, false }, { 45.50, 1.00, 62, true,  true  },
        // サビ
        { 48.00, 0.50, 64, false, true  }, { 48.50, 0.50, 67, false, false }, { 49.00, 1.00, 69, true,  true  },
        { 50.00, 0.50, 67, false, false }, { 50.50, 0.50, 69, false, true  }, { 51.00, 0.50, 71, false, false },
        { 51.50, 1.50, 72, true,  true  }, { 53.00, 0.50, 71, false, false },
        { 54.00, 0.50, 69, false, true  }, { 54.50, 0.50, 67, false, false }, { 55.00, 1.00, 64, true,  true  },
        { 56.00, 0.50, 62, false, false }, { 56.50, 1.50, 64, true,  true  },
    };

    struct LyricDef { double t0, t1; const char* text; };

    const LyricDef lyricDefs[] = {
        { 31.0, 35.0, "夜をまたいで 帰り道" },
        { 35.5, 41.0, "同じ穴の無地 投げ所" },
        { 41.5, 46.5, "抱っこしてください まだ" },
        { 48.0, 53.5, "明日になれば 忘れるから" },
        { 54.0, 58.0, "いまだけ ここにいて" },
    };

    /** 時刻 t（秒）のお手本ピッチ。音符外なら負を返す */
    float refMidiAt (const Session& s, double t, bool& lowConfidence)
    {
        lowConfidence = false;
        const auto& notes = s.refNotes;

        for (size_t i = 0; i < notes.size(); ++i)
        {
            const auto& n = notes[i];
            const auto t0 = s.toSec (n.start), t1 = s.toSec (n.end);
            if (t < t0 || t >= t1)
                continue;

            const auto dt = t - t0;
            float m = n.midi;

            // 直前の音符と連続していればポルタメント
            if (i > 0 && std::abs (s.toSec (notes[i - 1].end) - t0) < 0.02)
                m = notes[i - 1].midi + (n.midi - notes[i - 1].midi) * smoothstep ((float) (dt / 0.07));

            if (n.vibrato && dt > 0.35)
                m += 0.3f * juce::jmin (1.0f, (float) ((dt - 0.35) / 0.3)) * (float) std::sin (twoPi * 5.5 * dt);

            // フレーズ末は少し落ちる
            const bool phraseEnd = (i + 1 == notes.size()) || std::abs (s.toSec (notes[i + 1].start) - t1) > 0.02;
            if (phraseEnd && t > t1 - 0.08)
            {
                const auto k = (float) ((t - (t1 - 0.08)) / 0.08);
                m -= 0.6f * k * k;
            }

            lowConfidence = n.consonant && dt < 0.045;
            return m;
        }
        return -1.0f;
    }

    /** 自分のずれ（セント）。色分け（緑/黄/赤）の見本が出るように作る */
    float myCentsAt (double t)
    {
        float c = 9.0f * (float) std::sin (twoPi * 0.7 * t) + 6.0f * (float) std::sin (twoPi * 2.3 * t + 1.1);

        if (t >= 32.50 && t < 33.50) c -= 40.0f;                                             // B3 やや低い（黄）
        if (t >= 34.75 && t < 35.00) c += 70.0f * (float) ((t - 34.75) / 0.25);             // 語尾が上ずる
        if (t >= 36.50 && t < 37.00) c -= 36.0f;                                             // C4 低め（黄）
        if (t >= 37.00 && t < 37.15) c -= 95.0f * (1.0f - (float) ((t - 37.0) / 0.15));     // しゃくり（赤→黄→緑）
        if (t >= 38.40) c = 12.0f + 4.0f * (float) std::sin (twoPi * 1.7 * t);               // 現在音 +12c
        return c;
    }

    float sectionLoudness (double t)
    {
        if (t < 12.0)  return 0.45f;
        if (t < 48.0)  return 0.52f;
        if (t < 72.0)  return 0.88f;   // サビ（DESIGN 20: 0:48–1:12 を大きめに）
        if (t < 80.0)  return 0.60f;
        if (t < 96.0)  return 0.55f;
        if (t < 128.0) return 0.90f;
        return 0.90f * (float) juce::jmax (0.0, (136.0 - t) / 8.0);
    }
}

const project::LyricLine* Session::lyricAt (int64 sample) const
{
    for (auto& l : project.lyrics)
        if (sample >= l.startSample && sample < l.endSample)
            return &l;
    return nullptr;
}

const project::LyricLine* Session::lyricAfter (int64 sample) const
{
    for (auto& l : project.lyrics)
        if (l.startSample > sample)
            return &l;
    return nullptr;
}

Session makeSession()
{
    Session s;
    s.songName = "Tanuki_mix_demo";

    auto& p = s.project;
    p.songPath       = "audio/Tanuki_mix_demo.wav";
    p.sampleRate     = 48000;
    p.lengthSamples  = s.sec (136.0);          // 2:16
    p.tempoOriginal  = 120.0;
    p.keyOriginal    = 0;
    p.modeLast       = project::Mode::standard;
    p.inputProfileId = "usb-audio-interface-in1";

    s.playhead  = s.sec (39.0);                // 0:39
    s.rangeIn   = s.sec (48.0);                // サビ
    s.rangeOut  = s.sec (72.0);
    s.viewStart = s.sec (33.0);
    s.viewEnd   = s.sec (55.0);

    s.inputDevice = juce::String::fromUTF8 ("USB Audio Interface \xe2\x80\x94 In 1");   // 機器名（データ。翻訳しない）
    s.driver      = "WASAPI";

    // トラック（DESIGN 4.6 / take_map 例に合わせる）
    using project::Take;
    using project::CompSegment;

    project::Track backing { TrackType::backing, {}, {} };
    project::Track guide   { TrackType::guide, {}, {} };

    project::Track main { TrackType::main, {}, {} };
    for (int i = 1; i <= 4; ++i)
    {
        Take t;
        t.id = "take" + juce::String (i);
        t.path = "takes/main/" + t.id + ".wav";
        t.startSample = (i == 4 ? s.rangeIn : 0);
        t.endSample   = (i == 4 ? s.rangeOut : p.lengthSamples);
        t.clip = (i == 4);
        t.latencySamples = s.latencySamples;
        main.takes.push_back (t);
    }
    main.comp = { { 0, s.sec (48.0), "take2" },
                  { s.sec (48.0), s.sec (72.0), "take4" },
                  { s.sec (72.0), p.lengthSamples, "take2" } };

    project::Track dbl { TrackType::doubleTrack, {}, {} };
    {
        Take t;
        t.id = "take1"; t.path = "takes/double/take1.wav";
        t.startSample = s.sec (48.0); t.endSample = s.sec (72.0);
        t.latencySamples = s.latencySamples;
        dbl.takes.push_back (t);
        t.id = "take2"; t.path = "takes/double/take2.wav";
        dbl.takes.push_back (t);
        dbl.comp = { { s.sec (48.0), s.sec (72.0), "take2" } };
    }

    project::Track harm1 { TrackType::harm1, {}, {} };
    project::Track harm2 { TrackType::harm2, {}, {} };

    p.tracks = { backing, guide, main, dbl, harm1, harm2 };
    p.markers = { { s.sec (48.0), {}, project::MarkerKind::chorus } };

    s.trackUi = {
        { TrackType::main,        true,  false, false, 0.80f, 1 },
        { TrackType::doubleTrack, false, false, false, 0.55f, 2 },
        { TrackType::harm1,       false, false, false, 0.60f, 3 },
        { TrackType::harm2,       false, false, false, 0.60f, 4 },
    };
    s.selectedTrack = 0;

    for (auto& l : lyricDefs)
        p.lyrics.push_back ({ s.sec (l.t0), s.sec (l.t1), utf8 (l.text) });

    for (auto& n : melody)
        s.refNotes.push_back ({ s.sec (n.t), s.sec (n.t + n.len), n.midi, n.vib, n.cons });

    // ピッチ曲線（10 ms ホップ）
    const auto from = s.sec (28.0), to = s.sec (62.0);   // メロディのある区間
    for (int64 smp = from; smp < to; smp += hop)
    {
        const auto t = s.toSec (smp);
        bool low = false;

        const auto ref = refMidiAt (s, t, low);
        if (ref > 0.0f)
            s.refPitch.push_back ({ smp, ref, low ? 0.1f : 0.95f, 0.0f });

        // 自分のピッチは全区間ぶん用意し、描画側で「再生ヘッドまで」に切る
        {
            bool myLow = false;
            const auto base = refMidiAt (s, t - singerLagSec, myLow);
            if (base > 0.0f)
            {
                const auto cents = myCentsAt (t);
                s.myPitch.push_back ({ smp, base + cents / 100.0f, myLow ? 0.15f : 0.9f, cents });
            }
        }
    }

    return s;
}

Session makeSongSession (const Session& prev, const juce::String& name, const juce::String& path,
                         int sampleRate, int64 lengthSamples,
                         std::shared_ptr<const audio::WaveformOverview> wave)
{
    Session s;
    s.songName = name;
    s.backingWave = std::move (wave);
    s.tempoKnown = false;
    s.keyKnown = false;

    auto& p = s.project;
    p.songPath      = path;
    p.sampleRate    = sampleRate;
    p.lengthSamples = lengthSamples;
    p.modeLast      = prev.mode;
    p.tracks = { { TrackType::backing, {}, {} }, { TrackType::guide, {}, {} },
                 { TrackType::main, {}, {} },    { TrackType::doubleTrack, {}, {} },
                 { TrackType::harm1, {}, {} },   { TrackType::harm2, {}, {} } };

    s.trackUi = {
        { TrackType::main,        true,  false, false, 0.80f, 1 },
        { TrackType::doubleTrack, false, false, false, 0.55f, 2 },
        { TrackType::harm1,       false, false, false, 0.60f, 3 },
        { TrackType::harm2,       false, false, false, 0.60f, 4 },
    };

    // 頭から 30 秒を表示（短い曲は全体）
    s.viewStart = 0;
    s.viewEnd = juce::jmax ((int64) 1, juce::jmin (lengthSamples, s.sec (30.0)));

    // 表示の好み
    s.mode                = prev.mode;
    s.octaveAlign         = prev.octaveAlign;
    s.fullRange           = prev.fullRange;
    s.lowMidi             = prev.lowMidi;
    s.highMidi            = prev.highMidi;
    s.pitchToleranceCents = prev.pitchToleranceCents;
    s.countInBars         = prev.countInBars;
    s.clickOn             = prev.clickOn;
    s.loopOn              = prev.loopOn;
    // オフボは 0 dB（0.75）から。前も開いた曲なら、その音量を引き継ぐ
    s.offVocalGain        = prev.backingWave != nullptr ? prev.offVocalGain : 0.75f;
    s.backingMuted        = prev.backingWave != nullptr && prev.backingMuted;
    s.mainGain            = prev.mainGain;
    s.harmonyGain         = prev.harmonyGain;
    s.monitorGain         = prev.monitorGain;
    s.monitorReverb       = prev.monitorReverb;

    // 入力（B3 で実デバイスにつなぐまではダミーのまま）
    s.inputDevice     = prev.inputDevice;
    s.driver          = prev.driver;
    s.bufferSize      = prev.bufferSize;
    s.inputPeakDb     = prev.inputPeakDb;
    s.inputRmsDb      = prev.inputRmsDb;
    s.inputPeakHoldDb = prev.inputPeakHoldDb;
    s.latencySamples  = prev.latencySamples;
    s.output          = prev.output;
    s.engineAttached  = prev.engineAttached;
    s.updateVersion   = prev.updateVersion;
    return s;
}

float backingPeak (const Session& s, int64 start, int64 end)
{
    if (s.backingWave != nullptr)
        return s.backingWave->getPeak (start, end).magnitude();

    float a = 0.0f;
    for (int k = 0; k < 4; ++k)
        a = juce::jmax (a, backingAmplitude (s, start + (end - start) * k / 4));
    return a;
}

float backingRms (const Session& s, int64 start, int64 end)
{
    if (s.backingWave != nullptr)
        return s.backingWave->getRms (start, end);
    return backingPeak (s, start, end) * 0.5f;
}

float backingAmplitude (const Session& s, int64 sample)
{
    const auto t = s.toSec (sample);
    const auto beat = std::fmod (t, 60.0 / s.bpm()) / (60.0 / s.bpm());
    const auto pulse = (float) std::exp (-beat * 6.0);
    const auto wobble = 0.85f + 0.15f * (float) std::sin (twoPi * 1.3 * t);
    const auto grain = 0.88f + 0.12f * (float) std::abs (std::sin (twoPi * 23.0 * t) * std::cos (twoPi * 4.1 * t));
    return sectionLoudness (t) * (0.55f + 0.45f * pulse) * wobble * grain;
}

float vocalAmplitude (const Session& s, TrackType type, int64 sample)
{
    if (! isRecorded (s, type, sample))
        return 0.0f;

    const auto lag = (type == TrackType::doubleTrack ? 0.055 : singerLagSec);
    const auto t = s.toSec (sample) - lag;

    for (auto& n : s.refNotes)
    {
        const auto t0 = s.toSec (n.start), t1 = s.toSec (n.end);
        if (t < t0 || t >= t1)
            continue;

        const auto phase = (float) ((t - t0) / (t1 - t0));
        const auto attack = juce::jmin (1.0f, (float) ((t - t0) / 0.025));
        const auto release = juce::jmin (1.0f, (float) ((t1 - t) / 0.04));
        const auto body = 0.62f + 0.38f * std::sqrt ((float) std::sin (juce::MathConstants<double>::pi * phase));
        const auto grain = 0.74f + 0.26f * (float) std::abs (std::sin (twoPi * 31.0 * t) * std::cos (twoPi * 7.7 * t));
        auto amp = (t >= 48.0 && t < 72.0 ? 0.86f : 0.56f) * attack * release * body * grain;

        if (type == TrackType::doubleTrack)
            amp *= 0.78f;

        // take4 のクリップ（0:50.10–0:50.35）
        if (type == TrackType::main && t >= 50.10 && t < 50.35)
            amp = 1.08f;

        return amp;
    }

    // 音符外：ブレス/ノイズフロア
    return 0.025f + 0.015f * (float) std::abs (std::sin (twoPi * 3.0 * t));
}

bool isRecorded (const Session& s, TrackType type, int64 sample)
{
    if (type == TrackType::backing)
        return true;

    if (auto* tr = s.project.findTrack (type))
        for (auto& c : tr->comp)
            if (sample >= c.startSample && sample < c.endSample)
                return true;

    return false;
}

const PitchPoint* myPitchAt (const Session& s, int64 sample)
{
    // 10 ms 刻みなので二分探索で直前の点
    auto it = std::upper_bound (s.myPitch.begin(), s.myPitch.end(), sample,
                                [] (int64 v, const PitchPoint& p) { return v < p.sample; });
    if (it == s.myPitch.begin())
        return nullptr;
    --it;
    if (sample - it->sample > 960 || it->confidence < 0.5f)
        return nullptr;
    return &*it;
}

juce::String noteName (float midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto n = (int) std::lround (midi);
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}
} // namespace vb::dummy
