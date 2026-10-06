#include "song/SongInfo.h"

/*  曲の情報（DESIGN 7.5）：小節・拍 ↔ サンプル、区間の一覧、歌詞のタップ合わせ。歌詞はすべて見本用に書いた架空のもの */

namespace vb::song
{
class SongInfoTests : public juce::UnitTest
{
public:
    SongInfoTests() : juce::UnitTest ("SongInfo", "VoiceBooth") {}

    static juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    static TempoInfo tempo (double bpm, int64 downbeat = 0, TimeSignature sig = {})
    {
        TempoInfo t;
        t.bpm = bpm;
        t.downbeatSample = downbeat;
        t.signature = sig;
        t.source = Source::confirmed;
        return t;
    }

    void runTest() override
    {
        constexpr double sr = 48000.0;

        beginTest ("bar/beat: 120 BPM 4/4 from sample 0");
        {
            const auto t = tempo (120.0);
            expectEquals (t.samplesPerBeat (sr), 24000.0);
            auto bb = barBeatAt (t, 0, sr);
            expect (bb.bar == 1 && bb.beat == 1);
            bb = barBeatAt (t, 24000, sr);
            expect (bb.bar == 1 && bb.beat == 2);
            bb = barBeatAt (t, 95999, sr);
            expect (bb.bar == 1 && bb.beat == 4);
            expectWithinAbsoluteError (bb.fraction, 1.0, 0.001);
            bb = barBeatAt (t, 96000, sr);
            expect (bb.bar == 2 && bb.beat == 1);
            expectEquals (barSample (t, 1, sr), (int64) 0);
            expectEquals (barSample (t, 3, sr), (int64) 192000);
        }

        beginTest ("bar/beat: bar 1 later than the song start gives pickup bars 0, -1");
        {
            const auto t = tempo (120.0, 30000);   // 1 小節目の頭が 0.625 秒
            auto bb = barBeatAt (t, 30000, sr);
            expect (bb.bar == 1 && bb.beat == 1);
            bb = barBeatAt (t, 29999, sr);
            expect (bb.bar == 0 && bb.beat == 4);
            bb = barBeatAt (t, 0, sr);
            expect (bb.bar == 0 && bb.beat == 3);   // 30000 - 2 拍 = -18000 → 0 は 3 拍目
            expectEquals (barSample (t, 0, sr), (int64) (30000 - 96000));
        }

        beginTest ("bar/beat: non-integer beat length stays exact on every beat (128 BPM @ 44.1 kHz)");
        {
            const auto t = tempo (128.0, 1234);
            for (int64 k = -20; k < 2000; ++k)
            {
                const auto s = beatSample (t, k, 44100.0);
                expectEquals (beatIndexAt (t, s, 44100.0), k);
                expectEquals (beatIndexAt (t, s - 1, 44100.0), k - 1);
            }
            // 1000 拍先でも丸めの誤差がたまらない（1 サンプル以内）
            expectWithinAbsoluteError ((double) beatSample (t, 1000, 44100.0), 1234.0 + 1000.0 * 44100.0 * 60.0 / 128.0, 1.0);
        }

        beginTest ("time signatures: 3/4 has 3 beats, 6/8 counts dotted quarters (2 per bar)");
        {
            expectEquals (TimeSignature { 4, 4 }.beatsPerBar(), 4);
            expectEquals (TimeSignature { 3, 4 }.beatsPerBar(), 3);
            expectEquals (TimeSignature { 6, 8 }.beatsPerBar(), 2);
            const auto t = tempo (60.0, 0, { 3, 4 });
            auto bb = barBeatAt (t, (int64) (3 * sr), sr);
            expect (bb.bar == 2 && bb.beat == 1);
            const auto c = tempo (60.0, 0, { 6, 8 });
            bb = barBeatAt (c, (int64) (3 * sr), sr);
            expect (bb.bar == 2 && bb.beat == 2);
            expectEquals ((int) timeSignatures().size(), 3);
        }

        beginTest ("snap to bar and shifting bar 1 by beats");
        {
            const auto t = tempo (120.0);
            expectEquals (snapToBar (t, 40000, sr), (int64) 0);        // 1 小節 = 96000
            expectEquals (snapToBar (t, 50000, sr), (int64) 96000);
            expectEquals (snapToBar (t, 191000, sr), (int64) 192000);
            expectEquals (snapToBar ({}, 12345, sr), (int64) 12345);   // テンポが分からなければそのまま
            expectEquals (shiftDownbeat (t, 1, sr), (int64) 24000);
            expectEquals (shiftDownbeat (t, -1, sr), (int64) -24000);
        }

        beginTest ("BPM text: format and parse (2 decimals, 30-300)");
        {
            expectEquals (formatBpm (128.0), juce::String ("128"));
            expectEquals (formatBpm (127.5), juce::String ("127.50"));
            expectEquals (formatBpm (99.999), juce::String ("100"));
            expectEquals (parseBpm ("128"), 128.0);
            expectEquals (parseBpm (" 92.346 "), 92.35);
            expectEquals (parseBpm ("92,5"), 92.5);
            expectEquals (parseBpm ("12"), 0.0);
            expectEquals (parseBpm ("301"), 0.0);
            expectEquals (parseBpm ("1.2.3"), 0.0);
            expectEquals (parseBpm ("abc"), 0.0);
        }

        beginTest ("key: short names, unknown is empty");
        {
            KeyInfo k;
            expect (! k.known());
            expect (k.shortName().isEmpty());
            k.tonic = 6;
            k.minor = true;
            expectEquals (k.shortName(), juce::String ("F#m"));
            k.tonic = 10;
            k.minor = false;
            expectEquals (k.shortName(), juce::String ("Bb"));
        }

        beginTest ("sections: sorted insert, same position replaces, move reorders, remove");
        {
            Sections list;
            Section a; a.startSample = 96000; a.kind = kind::verseA;
            Section b; b.startSample = 0;     b.kind = kind::intro;
            Section c; c.startSample = 480000; c.kind = kind::chorus;
            expectEquals (addSection (list, a, sr), 0);
            expectEquals (addSection (list, b, sr), 0);
            expectEquals (addSection (list, c, sr), 2);
            expect (list[0].kind == kind::intro && list[1].kind == kind::verseA && list[2].kind == kind::chorus);

            // 50 ms 以内は同じ位置：名前だけ入れ替わり、数は増えない
            Section again; again.startSample = 96000 + 1000; again.kind = kind::verseB;
            expectEquals (addSection (list, again, sr), 1);
            expectEquals ((int) list.size(), 3);
            expect (list[1].kind == kind::verseB && list[1].startSample == 96000);

            // 動かす：並び直して新しい番号を返す。推定は触ったら確定
            list[2].source = Source::estimated;
            expectEquals (moveSection (list, 2, 48000, sr), 1);
            expect (list[1].kind == kind::chorus && list[1].source == Source::confirmed);
            // ほかの頭と重なる所へは動かさない
            expectEquals (moveSection (list, 1, 96000 + 500, sr), 1);
            expectEquals (list[1].startSample, (int64) 48000);

            expectEquals (sectionIndexAt (list, 0), 0);
            expectEquals (sectionIndexAt (list, 50000), 1);
            expectEquals (sectionIndexAt (list, 10'000'000), 2);
            expectEquals (sectionEnd (list, 1, 5'000'000), (int64) 96000);
            expectEquals (sectionEnd (list, 2, 5'000'000), (int64) 5'000'000);

            removeSection (list, 0);
            expectEquals ((int) list.size(), 2);
            expectEquals (sectionIndexAt (list, 0), -1);
            removeSection (list, 7);   // 範囲外は何もしない
            expectEquals ((int) list.size(), 2);
        }

        beginTest ("sections: numbers only when a name repeats (generic always numbered)");
        {
            Sections list;
            auto add = [&] (int64 at, const char* k, const char* customName = "")
            {
                Section s; s.startSample = at; s.kind = k; s.name = u8 (customName);
                addSection (list, s, sr);
            };
            add (0, kind::intro);
            add (100000, kind::chorus);
            add (200000, kind::verseA);
            add (300000, kind::chorus);
            add (400000, kind::generic);
            add (500000, kind::custom, "ラップ");
            add (600000, kind::custom, "ラップ");
            add (700000, kind::custom, "セリフ");
            const auto n = sectionNumbers (list);
            const std::vector<int> expected { 0, 1, 0, 2, 1, 1, 2, 0 };
            expect (n == expected);
        }

        beginTest ("sections: lyric headings map to section kinds");
        {
            expectEquals (sectionKindOfHeading (u8 ("サビ")), juce::String (kind::chorus));
            expectEquals (sectionKindOfHeading (u8 ("サビ２")), juce::String (kind::chorus));
            expectEquals (sectionKindOfHeading ("Chorus"), juce::String (kind::chorus));
            expectEquals (sectionKindOfHeading ("Verse 1"), juce::String (kind::verseA));
            expectEquals (sectionKindOfHeading (u8 ("Ｂメロ")), juce::String (kind::verseB));
            expectEquals (sectionKindOfHeading (u8 ("落ちサビ")), juce::String (kind::dropChorus));
            expectEquals (sectionKindOfHeading (u8 ("大サビ")), juce::String (kind::lastChorus));
            expectEquals (sectionKindOfHeading ("Bridge"), juce::String (kind::verseC));
            expectEquals (sectionKindOfHeading (u8 ("間奏")), juce::String (kind::interlude));
            expect (sectionKindOfHeading (u8 ("Dメロ")).isEmpty());   // 名前はそのまま自由入力として使う
        }

        // 架空の歌詞（見本用）
        const auto plain = u8 ("【Aメロ】\n"
                               "夜の窓に灯りがともる\n"
                               "遠い駅の名前を数える\n"
                               "\n"
                               "【サビ】\n"
                               "ほどけた糸をたぐりよせて\n"
                               "まだ知らない朝へ走る\n"
                               "\n"
                               "【間奏】\n"
                               "\n"
                               "【サビ】\n"
                               "ほどけた糸をたぐりよせて\n"
                               "まだ知らない朝へ走る\n");

        beginTest ("lyrics: plain text has no times; display ends are empty");
        {
            const auto ly = lyricsFromDoc (parseLyrics (plain), sr, (int64) (180 * sr));
            expectEquals ((int) ly.lines.size(), 6);
            expectEquals (ly.numTimed(), 0);
            expectEquals ((int) ly.headings.size(), 4);
            expectEquals ((int) ly.chorusBlocks.size(), 2);
            expectEquals (lineAt (ly, 0), -1);
            expectEquals (firstLineToSync (ly, 0), 0);
        }

        beginTest ("lyrics: tap to sync sets starts in order and keeps them monotonic");
        {
            const auto length = (int64) (180 * sr);
            auto ly = lyricsFromDoc (parseLyrics (plain), sr, length);

            int next = 0;
            const double taps[] = { 10.0, 13.0, 20.0, 23.0, 40.0, 43.0 };
            for (auto t : taps)
                next = tapLine (ly, next, (int64) (t * sr), length, sr);
            expectEquals (next, 6);
            expect (ly.allTimed());

            // 同じ塊は次の行の頭まで、塊の終わりは最長 6 秒
            expectEquals (ly.lines[0].endSample, (int64) (13.0 * sr));
            expectEquals (ly.lines[1].endSample, (int64) (19.0 * sr));
            expectEquals (ly.lines[5].endSample, (int64) (49.0 * sr));
            expectEquals (lineAt (ly, (int64) (14.0 * sr)), 1);
            expectEquals (lineAt (ly, (int64) (19.5 * sr)), -1);   // 塊の間（間奏）は光らせない
            expectEquals (nextTimedLineAfter (ly, (int64) (19.5 * sr)), 2);

            // 戻って合わせ直す：その位置以降の最初の行から
            expectEquals (firstLineToSync (ly, (int64) (21.0 * sr)), 3);

            // 行 2 を 14 秒に叩き直す → 前の行（13 秒）より後なので残り、後ろは順番を保つ
            tapLine (ly, 2, (int64) (14.0 * sr), length, sr);
            expectEquals (ly.lines[1].startSample, (int64) (13.0 * sr));
            expectEquals (ly.lines[3].startSample, (int64) (23.0 * sr));

            // 行 1 を 25 秒に叩く → 前後の順番を崩す行は時刻を外す
            tapLine (ly, 1, (int64) (25.0 * sr), length, sr);
            expect (! ly.lines[2].timed() && ! ly.lines[3].timed());
            expect (ly.lines[0].timed() && ly.lines[4].timed());

            // 1 行戻す
            clearLineTime (ly, 1, length, sr);
            expect (! ly.lines[1].timed());
            expectEquals (ly.lines[0].endSample, (int64) (16.0 * sr));   // 次の時刻のある行が離れたので最長 6 秒

            // 範囲外の行は何もしない
            expectEquals (tapLine (ly, 99, 0, length, sr), 99);
        }

        beginTest ("lyrics: lrc times become samples; text round-trips through the pad");
        {
            const auto lrc = u8 ("[ti:見本]\n"
                                 "[00:12.50]夜の窓に灯りがともる\n"
                                 "[00:16.00]遠い駅の名前を数える\n"
                                 "\n"
                                 "【サビ】\n"
                                 "[00:31.25]ほどけた糸をたぐりよせて\n"
                                 "まだ知らない朝へ走る\n");
            const auto length = (int64) (120 * sr);
            const auto ly = lyricsFromDoc (parseLyrics (lrc), sr, length);
            expectEquals ((int) ly.lines.size(), 4);
            expectEquals (ly.lines[0].startSample, (int64) (12.5 * sr));
            expectEquals (ly.lines[2].startSample, (int64) (31.25 * sr));
            expect (! ly.lines[3].timed());
            expectEquals (ly.numTimed(), 3);

            const auto text = toLyricText (ly, sr);
            const auto back = lyricsFromDoc (parseLyrics (text), sr, length);
            expectEquals ((int) back.lines.size(), (int) ly.lines.size());
            expectEquals ((int) back.headings.size(), 1);
            expectEquals (back.headings[0].name, u8 ("サビ"));
            expectEquals (back.headings[0].firstLine, 2);
            for (size_t i = 0; i < ly.lines.size(); ++i)
            {
                expectEquals (back.lines[i].text, ly.lines[i].text);
                expect (back.lines[i].timed() == ly.lines[i].timed());
                if (ly.lines[i].timed())
                    expect (std::abs (back.lines[i].startSample - ly.lines[i].startSample) <= (int64) (0.0005 * sr) + 1);
                expectEquals (back.lines[i].block, ly.lines[i].block);
            }
        }

        beginTest ("lyrics: headings become confirmed sections once timed, chorus repeats become estimated");
        {
            const auto length = (int64) (180 * sr);
            auto ly = lyricsFromDoc (parseLyrics (plain), sr, length);
            Sections list;
            expectEquals (applyLyricSections (list, ly, sr), 0);   // 時刻が無い間は作らない

            int next = 0;
            for (auto t : { 10.0, 13.0, 20.0, 23.0 })
                next = tapLine (ly, next, (int64) (t * sr), length, sr);
            expectEquals (applyLyricSections (list, ly, sr), 2);   // Aメロ と 1 回目のサビ
            expect (list[0].kind == kind::verseA && list[0].source == Source::confirmed && list[0].heading == 0);
            expect (list[1].kind == kind::chorus && list[1].startSample == (int64) (20.0 * sr));

            for (auto t : { 40.0, 43.0 })
                next = tapLine (ly, next, (int64) (t * sr), length, sr);
            expectEquals (applyLyricSections (list, ly, sr), 1);   // 2 回目のサビ（間奏は歌詞の行が無いので作らない）
            expectEquals (applyLyricSections (list, ly, sr), 0);   // 二重に作らない
            expectEquals ((int) list.size(), 3);

            // 消した見出しの区間は作り直さない（バグチェック 2026-10-05）
            ly.headings[(size_t) list[0].heading].noSection = true;
            removeSection (list, 0);
            expectEquals (applyLyricSections (list, ly, sr), 0);
            expectEquals ((int) list.size(), 2);

            // 見出しの無い同じ塊のくり返しは「推定」のサビ
            const auto noHeadings = u8 ("ほどけた糸をたぐりよせて\nまだ知らない朝へ走る\n\n"
                                        "遠い駅の名前を数える\n\n"
                                        "ほどけた糸をたぐりよせて\nまだ知らない朝へ走る\n");
            auto ly2 = lyricsFromDoc (parseLyrics (noHeadings), sr, length);
            int n2 = 0;
            for (auto t : { 5.0, 8.0, 15.0, 30.0, 33.0 })
                n2 = tapLine (ly2, n2, (int64) (t * sr), length, sr);
            Sections list2;
            expectEquals (applyLyricSections (list2, ly2, sr), 2);
            expect (list2[0].kind == kind::chorus && list2[0].source == Source::estimated);

            // テンポが分かっていれば、見出しの区間の頭は一番近い小節線に（叩いた時刻が少し早い・遅い）
            {
                auto ly3 = lyricsFromDoc (parseLyrics (plain), sr, length);
                const auto t = tempo (120.0);   // 1 小節 = 2 秒
                int n3 = 0;
                for (auto at : { 7.96, 10.0, 16.05, 18.0 })
                    n3 = tapLine (ly3, n3, (int64) (at * sr), length, sr);
                Sections bars;
                expectEquals (applyLyricSections (bars, ly3, sr, &t), 2);
                expectEquals (bars[0].startSample, (int64) (8.0 * sr));
                expectEquals (bars[1].startSample, (int64) (16.0 * sr));
                expectEquals (ly3.lines[0].startSample, (int64) (7.96 * sr));   // 歌詞の時刻はそのまま
            }

            // 「区間にしない」を選んだら作らない
            ly2.sectionsFromHeadings = false;
            Sections list3;
            expectEquals (applyLyricSections (list3, ly2, sr), 0);
        }

        beginTest ("third guide: thirds on the key's scale");
        {
            KeyInfo c; c.tonic = 0;                    // C の長調
            expectEquals (diatonicThird (60, c, true), 64);    // C4 → E4（長 3 度）
            expectEquals (diatonicThird (62, c, true), 65);    // D4 → F4（短 3 度）
            expectEquals (diatonicThird (71, c, true), 74);    // B4 → D5（オクターブをまたぐ）
            expectEquals (diatonicThird (60, c, false), 57);   // C4 → A3（下へまたぐ）
            expectEquals (diatonicThird (64, c, false), 60);   // E4 → C4
            expectEquals (diatonicThird (65, c, false), 62);   // F4 → D4
            expectEquals (diatonicThird (61, c, true), 64);    // C#4 は C に寄せて E4
            expectEquals (diatonicThird (70, c, true), 72);    // Bb4 は A と B の真ん中：同じ近さなら下の A に寄せて C5
            KeyInfo a; a.tonic = 9; a.minor = true;    // A の短調（自然的短音階）
            expectEquals (diatonicThird (57, a, true), 60);    // A3 → C4（短 3 度）
            expectEquals (diatonicThird (60, a, true), 64);    // C4 → E4
            expectEquals (diatonicThird (64, a, true), 67);    // E4 → G4（自然的短音階なので G）
            expectEquals (diatonicThird (57, a, false), 53);   // A3 → F3
            KeyInfo eb; eb.tonic = 3;                  // Eb の長調
            expectEquals (diatonicThird (63, eb, true), 67);   // Eb4 → G4
            expectEquals (diatonicThird (70, eb, true), 74);   // Bb4 → D5
            expectEquals (diatonicThird (58, eb, true), 62);   // 主音より下の音：Bb3 → D4
            expectEquals (diatonicThird (60, KeyInfo(), true), -1);   // キーが分からない
        }
    }
};

static SongInfoTests songInfoTests;
} // namespace vb::song
