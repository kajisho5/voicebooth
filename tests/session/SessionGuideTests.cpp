#include "session/FakeEngine.h"
#include "session/FakeSeparator.h"
#include "session/SessionTestUtil.h"

/*  お手本の位置の手直し（#26）：キーを続けて押している間は線だけをずらし、音（声・原曲）は押し終わってから 1 回だけずらす。
    前は押すたびに全長の音を 3 本写していた（48 kHz の長い曲で数百 MB）。
    お手本は合成した音で作る：カラオケ（低音と拍のノイズ）と、それに声（音程の変わる正弦波。途中の 8 秒だけ）を足した原曲。原曲 − カラオケ で声が取れる */

namespace vb::test
{
/** 録った声の代わりに、原曲の声と同じ旋律の正弦波を書く（sharpFrom〜sharpTo 秒は 60 セント高く。苦手な小節のテスト） */
class ToneEngine final : public FakeEngine
{
public:
    double sharpFrom = 0.0, sharpTo = 0.0;

    audio::RecordedTake stopRecording() override
    {
        auto r = FakeEngine::stopRecording();   // 長さとファイル（無音）
        if (r.length <= 0 || song == nullptr)
            return r;
        const auto rate = song->sampleRate;
        const double voice[] = { 440.0, 523.25, 587.33, 659.25, 587.33, 523.25 };
        juce::AudioBuffer<float> b (1, (int) r.length);
        double phase = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto t = (double) (r.startSample + i) / rate;
            auto hz = voice[(int) (t / 0.75) % 6];
            if (t >= sharpFrom && t < sharpTo)
                hz *= std::pow (2.0, 60.0 / 1200.0);
            phase += juce::MathConstants<double>::twoPi * hz / rate;
            const bool sing = ((t >= 4.0 && t < 8.0) || (t >= 12.0 && t < 16.0)) && std::fmod (t, 0.75) < 0.6;
            b.setSample (0, i, sing ? 0.3f * (float) std::sin (phase) : 0.0f);
        }
        r.file.deleteFile();
        std::unique_ptr<juce::OutputStream> out (r.file.createOutputStream().release());
        juce::WavAudioFormat wav;
        if (auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (1).withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        return r;
    }
};

class SessionGuideTests : public juce::UnitTest
{
public:
    SessionGuideTests() : juce::UnitTest ("Session guide nudge", "VoiceBoothSession") {}

    static juce::File writeSong (const juce::File& f, bool withVocal)
    {
        constexpr double rate = 44100.0;
        constexpr double seconds = 20.0;
        const auto n = (int) (seconds * rate);
        juce::AudioBuffer<float> b (2, n);
        juce::Random noise (7);   // 両方のファイルで同じノイズ（拍のときだけ使うので、声の有無で順がずれない）
        const double bass[] = { 55.0, 65.41, 73.42, 82.41 };
        const double voice[] = { 440.0, 523.25, 587.33, 659.25, 587.33, 523.25 };
        // 拍は不規則に打つ（規則的だと時間合わせが 1 拍ずれた所とも合ってしまう）
        std::vector<double> beats;
        juce::Random beatRandom (11);
        for (double t = 0.2; t < seconds; t += 0.3 + 0.4 * beatRandom.nextDouble())
            beats.push_back (t);
        size_t nextBeat = 0;
        double phaseBass = 0.0, phaseVoice = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto t = i / rate;
            phaseBass += juce::MathConstants<double>::twoPi * bass[(int) (t * 1.37) % 4] / rate;
            while (nextBeat + 1 < beats.size() && beats[nextBeat + 1] <= t)
                ++nextBeat;
            const auto beat = t - beats[nextBeat];
            auto v = 0.25 * std::sin (phaseBass) + (beat >= 0.0 && beat < 0.03 ? 0.4 * (1.0 - beat / 0.03) * (noise.nextFloat() * 2.0 - 1.0) : 0.0);
            if (withVocal)
            {
                const auto note = (int) (t / 0.75);
                phaseVoice += juce::MathConstants<double>::twoPi * voice[note % 6] / rate;
                // 声は 4〜8 秒と 12〜16 秒だけ（伴奏だけの部分で「引けた」と分かる）
                if (((t >= 4.0 && t < 8.0) || (t >= 12.0 && t < 16.0)) && std::fmod (t, 0.75) < 0.6)
                    v += 0.2 * std::sin (phaseVoice);
            }
            b.setSample (0, i, (float) v);
            b.setSample (1, i, (float) v);
        }
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
        juce::WavAudioFormat wav;
        if (auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (b, 0, n);
        return f;
    }

    void runTest() override
    {
        const auto tag = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()).substring (0, 8);
        const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vbguide-" + tag);
        const auto songName = "vbguide-" + tag;
        const auto karaoke = writeSong (work.getChildFile (songName + ".wav"), false);
        const auto original = writeSong (work.getChildFile (songName + "-original.wav"), true);

        beginTest ("nudging the guide moves the line at once and the audio once after the keys stop");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (openSong (ui, karaoke));
            ui.loadGuide (original);
            expect (pumpUntil ([&] { return ! ui.get().guideBusy; }, 120000), "the guide is analysed");
            expect (ui.get().guideVocals != nullptr && ! ui.get().refPitch.empty(), "the guide is made by subtraction: " + ui.get().noticeText
                    + " / needsSeparation " + juce::String ((int) ui.get().guideNeedsSeparation));
            if (ui.get().guideVocals == nullptr || ui.get().refPitch.empty())
            {
                ui.attachEngine (nullptr);
                UiSession::projectFolderFor (songName).deleteRecursively();
                work.deleteRecursively();
                return;
            }
            pump (500);

            const auto rate = ui.get().sampleRate();
            const auto before = ui.get().guideVocals;
            const auto firstPoint = ui.get().refPitch.front().sample;
            for (int i = 0; i < 5; ++i)
                ui.nudgeGuide (1.0);
            const auto d = (juce::int64) std::llround (0.005 * rate);
            expectEquals (ui.get().refPitch.front().sample, firstPoint + d, "the line moves at once");
            expect (ui.get().guideVocals == before, "the audio is not copied on every key press");

            expect (pumpUntil ([&] { return ui.get().guideVocals != before; }, 3000), "the audio moves after the keys stop");
            const auto after = ui.get().guideVocals;
            if (after != nullptr && before != nullptr)
            {
                // 声は元の SR（オフボの SR）で持っている。後ろへ d ずらしたので、after[i + d] == before[i]
                const auto da = (int) std::llround ((double) d * before->sampleRate / rate);
                expectEquals (after->length(), before->length());
                float maxDiff = 0.0f;
                for (int i = 1000; i < 100000; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (after->buffer.getSample (0, i + da) - before->buffer.getSample (0, i)));
                expect (maxDiff < 1.0e-6f, "shifted by the whole nudge once: " + juce::String (maxDiff));
            }
            expectWithinAbsoluteError (ui.get().guideNudgeMs, 5.0, 1.0e-9);
            pump (500);
            expect (ui.get().guideVocals == after, "nothing more moves later");

            // 戻すと元の位置（線も音も）
            ui.nudgeGuide (-5.0);
            expectEquals (ui.get().refPitch.front().sample, firstPoint);
            expect (pumpUntil ([&] { return ui.get().guideVocals != after; }, 3000));
            if (auto back = ui.get().guideVocals; back != nullptr && before != nullptr)
                expectWithinAbsoluteError (back->buffer.getSample (0, 50000), before->buffer.getSample (0, 50000), 1.0e-6f);
            ui.attachEngine (nullptr);
        }

        beginTest ("a nudge pressed while the guide is analysed again ends up on both the voice and the original, once");
        {
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (openSong (ui, karaoke));
            ui.loadGuide (original);
            expect (pumpUntil ([&] { return ! ui.get().guideBusy; }, 120000));
            const auto v0 = ui.get().guideVocals;
            const auto o0 = ui.get().guideOriginal;
            expect (v0 != nullptr && o0 != nullptr);
            if (v0 != nullptr && o0 != nullptr)
            {
                const auto rate = ui.get().sampleRate();
                ui.loadGuide (original);          // 合わせ直している間に
                ui.nudgeGuide (5.0);              // 手直しを押す（前はここでためた分が、解析の終わりで捨てられたり二重になったりした）
                expect (pumpUntil ([&] { return ! ui.get().guideBusy; }, 120000));
                pump (600);                       // タイマーが残っていても何も起きないこと
                const auto v1 = ui.get().guideVocals;
                const auto o1 = ui.get().guideOriginal;
                const auto da = (int) std::llround (0.005 * rate * v0->sampleRate / rate);
                auto maxDiff = [da] (const audio::SongAudio& shifted, const audio::SongAudio& base)
                {
                    float m = 0.0f;
                    for (int i = 1000; i < 100000; ++i)
                        m = juce::jmax (m, std::abs (shifted.buffer.getSample (0, i + da) - base.buffer.getSample (0, i)));
                    return m;
                };
                expect (v1 != nullptr && maxDiff (*v1, *v0) < 1.0e-6f, "the voice is shifted by the nudge once");
                expect (o1 != nullptr && maxDiff (*o1, *o0) < 1.0e-6f, "the original is shifted by the nudge once");
            }
            ui.attachEngine (nullptr);
        }

        // ハモリ分け（#27）：引き算でお手本が取れても、リードとハモリの分離（4 分の曲で 15〜30 分）は黙って始めず、確認を出す
        auto analyse = [&] (UiSession& ui)
        {
            ui.loadGuide (original);
            expect (pumpUntil ([&] { return ! ui.get().guideBusy; }, 120000), "the guide is analysed");
            expect (ui.get().guideVocals != nullptr, "the guide is made by subtraction: " + ui.get().noticeText);
        };

        beginTest ("a guide taken by subtraction asks before splitting the lead and harmonies, and starts only when asked");
        {
            auto st = std::make_shared<FakeSeparation>();
            st->karaoke = true;
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            expect (openSong (ui, karaoke));
            const auto offers = ui.get().leadOfferSerial;
            analyse (ui);
            expectEquals (ui.get().leadOfferSerial, offers + 1, "it asks");
            pump (600);
            expectEquals (st->starts, 0, "nothing starts by itself");
            expect (! ui.get().separating);
            const auto e = ui.leadSplitEstimate();
            expect (e.known() && e.lowMinutes() >= 1, "the question tells how long it takes");

            ui.startLeadSplit();   // 「続ける」
            expect (pumpUntil ([&] { return st->running; }, 30000), "it starts when asked");
            expectEquals (st->starts, 1);
            ui.stopSeparation();
            pump (300);
            ui.attachEngine (nullptr);
        }

        beginTest ("starting from the original only: the split was announced before it began, so it is not asked again");
        {
            auto st = std::make_shared<FakeSeparation>();
            st->karaoke = true;
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            expect (openSong (ui, karaoke));
            const auto offers = ui.get().leadOfferSerial;
            ui.agreeLeadSplit();   // 起動画面の「原曲からオフボを作りますか？」で伝えてある
            analyse (ui);
            expectEquals (ui.get().leadOfferSerial, offers, "not asked");
            expect (pumpUntil ([&] { return st->running; }, 30000), "it goes on by itself");
            ui.stopSeparation();
            pump (300);

            // 了承はその 1 回だけ：次にお手本を読み直したときは聞く
            analyse (ui);
            expectEquals (ui.get().leadOfferSerial, offers + 1, "asked the next time");
            ui.attachEngine (nullptr);
        }

        beginTest ("without the lead model nothing is asked and nothing starts");
        {
            auto st = std::make_shared<FakeSeparation>();
            st->karaoke = false;
            FakeEngine engine;
            UiSession ui;
            ui.attachEngine (&engine);
            ui.setSeparationService (std::make_unique<FakeSeparationService> (st));
            expect (openSong (ui, karaoke));
            const auto offers = ui.get().leadOfferSerial;
            analyse (ui);
            pump (300);
            expectEquals (ui.get().leadOfferSerial, offers);
            expectEquals (st->starts, 0);
            ui.attachEngine (nullptr);
        }

        beginTest ("the weak-spot loop puts the range on the bars sung sharp, loops them and starts there");
        {
            ToneEngine engine;
            engine.sharpFrom = 12.0;   // 2 回目の歌（12〜16 秒）だけ 60 セント高く
            engine.sharpTo = 16.0;
            UiSession ui;
            ui.attachEngine (&engine);
            expect (openSong (ui, karaoke));
            ui.setMode (project::Mode::standard);
            ui.setCountIn (0);
            ui.setBpm (120.0);   // 4/4 で 1 小節 2 秒：12〜16 秒は 7・8 小節目
            ui.setDownbeat (0);
            analyse (ui);
            UiSession::projectFolderFor (songName).getChildFile ("Audio/Takes").deleteRecursively();

            ui.tick (1.1);
            ui.seek (0);
            ui.setRecording (true);
            expect (ui.get().isRecording, "recording starts: " + ui.get().noticeText);
            engine.playhead = ui.get().project.lengthSamples;
            ui.setRecording (false);
            expect (pumpUntil ([&] { return ui.latestTakeStats() != nullptr; }, 60000), "the take is analysed");

            const auto rate = (double) ui.get().sampleRate();
            const auto spots = ui.weakSpots();
            expect (! spots.empty(), "a weak spot is found");
            if (! spots.empty())
            {
                expectEquals (spots[0].firstBar, (juce::int64) 7, "bars 7-8");
                expectWithinAbsoluteError ((double) spots[0].start / rate, 12.0, 0.01);
                expectWithinAbsoluteError ((double) spots[0].end / rate, 16.0, 0.01);
                expectLessThan (spots[0].inBand, 0.2f);
                for (auto& w : spots)
                    expect (w.end <= (juce::int64) (12.0 * rate) || w.start >= (juce::int64) (12.0 * rate), "the in-tune singing (4-8 s) is not taken as weak");

                ui.loopWeakSpot();
                expectEquals (ui.get().rangeIn, spots[0].start);
                expectEquals (ui.get().rangeOut, spots[0].end);
                expect (ui.get().loopOn && engine.loopEnabled, "it loops");
                expectEquals (engine.playhead, spots[0].start, "and starts at the head of the bars");
                expect (ui.get().noticeText.contains ("7"), ui.get().noticeText);
                expectEquals (ui.weakSpotIndex (spots), 0);

                // もう 1 回押すと次の所へ（1 つしかなければ同じ所のまま）
                ui.loopWeakSpot();
                const auto next = spots.size() > 1 ? spots[1] : spots[0];
                expectEquals (ui.get().rangeIn, next.start);
            }

            // 簡単モード（範囲の録り直しがない）では何もしない
            ui.clearRange();
            ui.setMode (project::Mode::easy);
            ui.loopWeakSpot();
            expect (! ui.get().hasRange(), "easy mode has no range re-recording");
            ui.attachEngine (nullptr);
        }

        UiSession::projectFolderFor (songName).deleteRecursively();
        work.deleteRecursively();
    }
};

static SessionGuideTests sessionGuideTests;
} // namespace vb::test
