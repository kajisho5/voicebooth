#include "UiSession.h"

/*  曲の情報を手で入れる（B4b。DESIGN 7.5）
    テンポ・拍子・キー・区間・歌詞は project に入る（B14 で .vbooth に保存）。ここで変えた値は「確定」。
    画面の状態だけを変え、オーディオスレッドには何も足さない */

namespace vb
{
void UiSession::songInfoChanged (juce::uint32 also)
{
    // 目盛り・BAR.BEAT・トップバー・歌詞・区間が描き直す
    notify (change::songInfo | also);
}

//==============================================================================
// テンポ・拍子・キー（7.5.1）

void UiSession::setBpm (double bpm)
{
    auto& t = s.project.tempo;
    if (bpm <= 0.0)
    {
        t.bpm = 0.0;
        t.source = song::Source::estimated;
        t.confidence = 0.0f;
        t.beats.clear();
    }
    else
    {
        t.bpm = song::roundBpm (song::clampBpm (bpm));
        t.source = song::Source::confirmed;
        t.confidence = 1.0f;
        t.beats.clear();   // 手入力は一定テンポ＋1 小節目の位置
    }
    songInfoChanged();
}

double UiSession::tapTempo (double nowSeconds)
{
    const auto bpm = tapper.tap (nowSeconds);
    if (bpm > 0.0)
        setBpm (bpm);
    else
        songInfoChanged();   // 叩いた回数の表示
    return bpm;
}

void UiSession::doubleBpm() { if (s.tempoKnown()) setBpm (song::doubledBpm (s.bpm())); }
void UiSession::halveBpm()  { if (s.tempoKnown()) setBpm (song::halvedBpm (s.bpm())); }

void UiSession::setTimeSignature (song::TimeSignature sig)
{
    auto& t = s.project.tempo;
    t.signature = sig;
    if (t.known())
        t.source = song::Source::confirmed;
    songInfoChanged();
}

void UiSession::setDownbeat (int64 sample)
{
    auto& t = s.project.tempo;
    t.downbeatSample = juce::jlimit ((int64) 0, s.project.lengthSamples, sample);
    if (t.known())
        t.source = song::Source::confirmed;
    songInfoChanged();
}

void UiSession::setDownbeatAtPlayhead() { setDownbeat (s.playhead); }

void UiSession::shiftDownbeat (int beats)
{
    if (! s.tempoKnown())
        return;

    // 曲の頭より前へは出さない（1 拍ずつ前の小節の拍へ回り込む）
    auto next = song::shiftDownbeat (s.project.tempo, beats, s.sampleRate());
    const auto bar = (int64) std::llround (s.project.tempo.samplesPerBeat (s.sampleRate()) * s.beatsPerBar());
    while (next < 0 && bar > 0)
        next += bar;
    setDownbeat (next);
}

void UiSession::setSongKey (int tonic, bool minor)
{
    auto& k = s.project.key;
    k.tonic = juce::isPositiveAndBelow (tonic, 12) ? tonic : -1;
    k.minor = minor;
    // 「分からない」を選んだ時は空のまま（解析が後で埋めてよい）。選んだキーは確定
    k.source = k.known() ? song::Source::confirmed : song::Source::estimated;
    k.confidence = k.known() ? 1.0f : 0.0f;
    songInfoChanged();
}

//==============================================================================
// 区間（7.5.2）

int UiSession::addSection (int64 sample, const juce::String& kind, const juce::String& name, bool snap)
{
    if (snap)
        sample = song::snapToBar (s.project.tempo, sample, s.sampleRate());

    song::Section sec;
    sec.startSample = juce::jlimit ((int64) 0, juce::jmax ((int64) 0, s.project.lengthSamples - 1), sample);
    sec.kind = kind.isNotEmpty() ? kind : juce::String (song::kind::generic);
    sec.name = sec.isCustom() ? name.trim() : juce::String();
    sec.source = song::Source::confirmed;

    const auto index = song::addSection (s.project.sections, sec, s.sampleRate());
    s.selectedSection = index;
    songInfoChanged();
    return index;
}

int UiSession::addSectionAtPlayhead (bool snap)
{
    return addSection (s.playhead, song::kind::generic, {}, snap);
}

void UiSession::renameSection (int index, const juce::String& kind, const juce::String& name)
{
    auto& list = s.project.sections;
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;

    auto& sec = list[(size_t) index];
    sec.kind = kind.isNotEmpty() ? kind : juce::String (song::kind::generic);
    sec.name = sec.isCustom() ? name.trim() : juce::String();
    if (sec.isCustom() && sec.name.isEmpty())
        sec.kind = song::kind::generic;
    sec.source = song::Source::confirmed;   // 触ったら確定
    songInfoChanged();
}

int UiSession::moveSection (int index, int64 sample, bool snap)
{
    if (snap)
        sample = song::snapToBar (s.project.tempo, sample, s.sampleRate());
    sample = juce::jlimit ((int64) 0, juce::jmax ((int64) 0, s.project.lengthSamples - 1), sample);

    const auto& list = s.project.sections;
    if (juce::isPositiveAndBelow (index, (int) list.size()) && list[(size_t) index].startSample == sample)
        return index;

    const auto moved = song::moveSection (s.project.sections, index, sample, s.sampleRate());
    s.selectedSection = moved;
    songInfoChanged();
    return moved;
}

void UiSession::removeSection (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) s.project.sections.size()))
        return;
    song::removeSection (s.project.sections, index);
    s.selectedSection = -1;
    songInfoChanged();
}

void UiSession::selectSection (int index)
{
    const auto i = juce::isPositiveAndBelow (index, (int) s.project.sections.size()) ? index : -1;
    if (i == s.selectedSection)
        return;
    s.selectedSection = i;
    songInfoChanged();
}

void UiSession::goToSection (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) s.project.sections.size()))
        return;
    selectSection (index);
    seek (s.project.sections[(size_t) index].startSample);
}

void UiSession::loopSection (int index)
{
    const auto& list = s.project.sections;
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;
    selectSection (index);
    setRange (list[(size_t) index].startSample, song::sectionEnd (list, index, s.project.lengthSamples));
    setLoop (true);
    seek (s.rangeIn);
}

//==============================================================================
// 歌詞（7.5.3）

void UiSession::setLyrics (song::Lyrics lyrics)
{
    song::updateLineEnds (lyrics, s.project.lengthSamples, s.sampleRate());
    s.project.lyrics = std::move (lyrics);
    s.lyricSyncing = false;
    s.lyricCursor = 0;
    song::applyLyricSections (s.project.sections, s.project.lyrics, s.sampleRate(), &s.project.tempo);
    songInfoChanged (change::playhead);
}

void UiSession::clearLyrics()
{
    s.project.lyrics = {};
    s.lyricSyncing = false;
    s.lyricCursor = 0;
    songInfoChanged (change::playhead);
}

void UiSession::setLyricSyncing (bool on)
{
    on = on && ! s.project.lyrics.empty();
    s.lyricSyncing = on;
    if (on)
        s.lyricCursor = juce::jmin (song::firstLineToSync (s.project.lyrics, s.playhead),
                                    (int) s.project.lyrics.lines.size() - 1);
    songInfoChanged (change::playhead);
}

bool UiSession::tapLyric()
{
    auto& ly = s.project.lyrics;
    if (! s.lyricSyncing || ! juce::isPositiveAndBelow (s.lyricCursor, (int) ly.lines.size()))
        return false;

    s.lyricCursor = song::tapLine (ly, s.lyricCursor, s.playhead, s.project.lengthSamples, s.sampleRate());
    song::applyLyricSections (s.project.sections, ly, s.sampleRate(), &s.project.tempo);   // 見出しの行に時刻が付いたら区間に

    const bool more = s.lyricCursor < (int) ly.lines.size();
    if (! more)
    {
        s.lyricSyncing = false;   // 最後の行まで済んだ
        s.lyricCursor = (int) ly.lines.size() - 1;
    }
    songInfoChanged (change::playhead);
    return more;
}

void UiSession::undoLyricTap()
{
    auto& ly = s.project.lyrics;
    if (! s.lyricSyncing || s.lyricCursor <= 0)
        return;
    --s.lyricCursor;
    song::clearLineTime (ly, s.lyricCursor, s.project.lengthSamples, s.sampleRate());
    songInfoChanged (change::playhead);
}

void UiSession::stepLyric (int delta)
{
    const auto n = (int) s.project.lyrics.lines.size();
    if (n == 0)
        return;
    s.lyricCursor = juce::jlimit (0, n - 1, s.lyricCursor + delta);
    songInfoChanged (change::playhead);
}
} // namespace vb
