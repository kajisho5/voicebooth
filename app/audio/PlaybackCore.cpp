#include "PlaybackCore.h"
#include <rubberband/RubberBandStretcher.h>

namespace vb::audio
{
PlaybackCore::PlaybackCore() = default;
PlaybackCore::~PlaybackCore() = default;

void PlaybackCore::setSong (std::shared_ptr<const SongAudio> newSong)
{
    playing = false;
    pendingSeek = -1;
    position = 0;
    loopOn = false;
    reachedEnd = false;
    stretchReset = true;

    // 古い曲（と、その曲のトラック）は lock の外で解放する（オーディオスレッドを待たせない）
    std::shared_ptr<const juce::AudioBuffer<float>> oldStems[maxStems];
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        std::swap (song, newSong);
        for (int k = 0; k < maxStems; ++k)
            std::swap (stems[k], oldStems[k]);
    }
    newSong.reset();
    rebuildStretcher();
}

void PlaybackCore::rebuildStretcher()
{
    double songRate = 0.0;
    int channels = 0;
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        if (song != nullptr)
        {
            songRate = song->sampleRate;
            channels = juce::jlimit (1, 2, song->buffer.getNumChannels());
        }
    }

    // 作るのは lock の外（数 ms かかる）。差し替えだけ lock の中で
    std::unique_ptr<RubberBand::RubberBandStretcher> fresh;
    juce::AudioBuffer<float> in, out;
    if (songRate > 0.0)
    {
        using RB = RubberBand::RubberBandStretcher;
        fresh = std::make_unique<RB> ((size_t) std::lround (songRate), (size_t) channels,
                                      RB::OptionProcessRealTime | RB::OptionEngineFiner | RB::OptionThreadingNever
                                    | RB::OptionChannelsTogether | RB::OptionPitchHighConsistency);
        fresh->setMaxProcessSize ((size_t) stretchBlock);
        in.setSize (channels, stretchBlock);
        out.setSize (channels, stretchBlock);
    }
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        std::swap (stretcher, fresh);
        std::swap (stretchIn, in);
        std::swap (stretchOut, out);
    }
    stretchReset = true;
}

void PlaybackCore::setStem (int slot, std::shared_ptr<const juce::AudioBuffer<float>> buffer)
{
    if (! juce::isPositiveAndBelow (slot, maxStems))
        return;
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        std::swap (stems[slot], buffer);
    }
    buffer.reset();   // 古い方はここで解放
}

void PlaybackCore::setStemGain (int slot, float linearGain)
{
    if (juce::isPositiveAndBelow (slot, maxStems))
        stemGain[slot] = juce::jmax (0.0f, linearGain);
}

PlaybackCore::Gains PlaybackCore::nextGains() noexcept
{
    Gains g;
    g.backing = smoothedGain.getNextValue();
    for (int k = 0; k < maxStems; ++k)
        g.stem[k] = stemSmoothed[k].getNextValue();
    return g;
}

float PlaybackCore::mixAt (const SongAudio& s, int channel, juce::int64 pos, const Gains& g) noexcept
{
    const auto& b = s.buffer;
    auto v = b.getSample (juce::jmin (channel, b.getNumChannels() - 1), (int) pos) * g.backing;   // モノラルの曲は両耳へ
    peakBacking = juce::jmax (peakBacking, std::abs (v));
    for (int k = 0; k < maxStems; ++k)
        if (stems[k] != nullptr && g.stem[k] > 0.0f && pos < stems[k]->getNumSamples())
        {
            const auto x = stems[k]->getSample (0, (int) pos) * g.stem[k];
            if (k == guideSlot)
                peakGuide = juce::jmax (peakGuide, std::abs (x));
            else if (k == harmGuideSlot)
                peakHarmGuide = juce::jmax (peakHarmGuide, std::abs (x));
            v += x;
        }
    return v;
}

void PlaybackCore::setClick (bool on, float linearGain)
{
    metronome.setLevel (linearGain);
    clickOn = on;
}

void PlaybackCore::addClick (float* const* out, int numChannels, int i, float value) noexcept
{
    if (juce::exactlyEqual (value, 0.0f))
        return;
    peakClick = juce::jmax (peakClick, std::abs (value));
    for (int c = 0; c < numChannels; ++c)
        if (out[c] != nullptr)
            out[c][i] += value;
}

void PlaybackCore::addClickTail (float* const* out, int numChannels, int from, int to) noexcept
{
    for (int i = from; i < to; ++i)
        addClick (out, numChannels, i, metronome.tail());
}

void PlaybackCore::setPractice (double newSpeed, int newSemitones)
{
    speed = juce::jlimit (0.5, 1.5, newSpeed);
    semitones = juce::jlimit (-6, 6, newSemitones);
}

bool PlaybackCore::isPracticeShifted() const
{
    return std::abs (speed.load() - 1.0) > 1.0e-6 || semitones.load() != 0;
}

bool PlaybackCore::hasSong() const
{
    const juce::SpinLock::ScopedLockType sl (songLock);
    return song != nullptr;
}

void PlaybackCore::prepare (double outputSampleRate)
{
    outputRate = outputSampleRate;
    stretchReset = true;
    const auto r = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
    smoothedGain.reset (r, 0.02);   // 20 ms でなめらかに
    smoothedGain.setCurrentAndTargetValue (muted ? 0.0f : gain.load());
    for (int k = 0; k < maxStems; ++k)
    {
        stemSmoothed[k].reset (r, 0.02);
        stemSmoothed[k].setCurrentAndTargetValue (stemGain[k].load());
    }
    fade.reset (r, 0.02);
    fade.setCurrentAndTargetValue (1.0f);
    fraction = 0.0;
    metronome.prepare (r);
    backingLevel.prepare (r);
    guideLevel.prepare (r);
    harmGuideLevel.prepare (r);
    clickLevel.prepare (r);
    countLeft = 0.0;
    prepared = true;
}

void PlaybackCore::play (juce::int64 countIn, juce::int64 countClicksUntil)
{
    reachedEnd = false;
    stretchReset = true;    // 止まる前に Rubber Band に残っていた音を鳴らさない
    countInRequest = juce::jmax ((juce::int64) 0, countIn);
    countUntilRequest = countClicksUntil;
    countLeftOut = juce::jmax ((juce::int64) 0, countIn);   // 画面はすぐ「数えている」に（オーディオスレッドは次のブロックで始める）
    startRequested = true;
    playing = true;
}

void PlaybackCore::stop()
{
    playing = false;
}

void PlaybackCore::seek (juce::int64 sample)
{
    const auto clamped = juce::jmax ((juce::int64) 0, sample);
    pendingSeek = clamped;
    stretchReset = true;
    position = clamped;          // 画面にはすぐ反映（オーディオスレッドは次のブロックで追いつく）
}

juce::int64 PlaybackCore::getPosition() const
{
    const auto pending = pendingSeek.load();
    return pending >= 0 ? pending : position.load();
}

void PlaybackCore::setLoop (juce::int64 in, juce::int64 out, bool enabled)
{
    loopIn = in;
    loopOut = out;
    loopOn = enabled && out > in;
}

void PlaybackCore::setGain (float linearGain) { gain = juce::jmax (0.0f, linearGain); }
void PlaybackCore::setMuted (bool m)          { muted = m; }

float PlaybackCore::faderToGain (float p) noexcept
{
    if (p <= 0.0f)
        return 0.0f;
    const auto db = p >= 0.75f ? (p - 0.75f) / 0.25f * 6.0f        // 0.75 → 0 dB、1.0 → +6 dB
                               : 40.0f * std::log10 (p / 0.75f);   // 0.375 → -12 dB、0.1 → -35 dB
    return juce::Decibels::decibelsToGain (db, -100.0f);
}

PlaybackCore::Rendered PlaybackCore::render (float* const* out, int numChannels, int numSamples) noexcept
{
    Rendered r;
    for (int c = 0; c < numChannels; ++c)
        if (out[c] != nullptr)
            juce::FloatVectorOperations::clear (out[c], numSamples);

    // メーター：このブロックの最大を、どの道を通っても最後に入れる（止まっている間は 0 が入って下がっていく）
    peakBacking = peakGuide = peakClick = peakHarmGuide = 0.0f;
    struct PushLevels
    {
        PlaybackCore& core;
        int n;
        ~PushLevels()
        {
            core.backingLevel.push (core.peakBacking, n);
            core.guideLevel.push (core.peakGuide, n);
            core.harmGuideLevel.push (core.peakHarmGuide, n);
            core.clickLevel.push (core.peakClick, n);
        }
    } pushLevels { *this, numSamples };

    const juce::SpinLock::ScopedTryLockType sl (songLock);
    if (! sl.isLocked() || song == nullptr || ! prepared)
        return r;

    const auto& buffer = song->buffer;
    const auto length = (juce::int64) buffer.getNumSamples();
    const auto songChannels = buffer.getNumChannels();
    if (length == 0 || songChannels == 0)
        return r;

    auto pos = position.load();
    if (const auto seekTo = pendingSeek.exchange (-1); seekTo >= 0)
    {
        pos = juce::jmin (seekTo, length);
        fraction = 0.0;
        heard = (double) pos;
        metronome.resync();
    }

    r.start = pos;
    if (! playing)
    {
        position = pos;
        heard = (double) pos;
        countLeft = 0.0;
        countLeftOut = 0;
        addClickTail (out, numChannels, 0, numSamples);   // 止めた時に鳴りかけていた音は最後まで（途中で切ると「プツッ」と鳴る）
        return r;
    }

    // 再生の頭（2026-10-02）：カウントインの長さを受け取り、拍を探し直す
    if (startRequested.exchange (false))
    {
        countLeft = (double) countInRequest.load();
        countUntil = countUntilRequest.load();
        metronome.resync();
    }

    // カウントイン：曲の前の無音の分だけ、聞こえるはずの位置（pos - 残り）を進めながら拍を鳴らす。曲の位置は止めたまま。
    // 練習のテンポの時は拍の間も同じだけ伸び縮みする（その後の曲と拍がつながる）
    int lead = 0;
    if (countLeft > 0.0)
    {
        const auto rate = outputRate.load();
        const auto srRatio = rate > 0.0 ? song->sampleRate / rate : 1.0;
        const auto step = (isPracticeShifted() && stretcher != nullptr ? speed.load() : 1.0) * srRatio;
        for (; lead < numSamples && countLeft > 0.0; ++lead)
        {
            addClick (out, numChannels, lead, metronome.next ((double) pos - countLeft, true));
            countLeft -= step;
        }
        countLeftOut = countLeft > 0.0 ? (juce::int64) std::ceil (countLeft) : 0;
        if (lead == numSamples)
        {
            position = pos;
            r.lead = lead;
            return r;
        }
    }

    // 曲はブロックの lead サンプル目から
    constexpr int maxOutputs = 16;
    float* shifted[maxOutputs] = {};
    const auto channels = juce::jmin (numChannels, maxOutputs);
    for (int c = 0; c < channels; ++c)
        shifted[c] = out[c] != nullptr ? out[c] + lead : nullptr;

    auto played = renderSong (shifted, channels, numSamples - lead, pos);
    played.lead = lead;
    return played;
}

PlaybackCore::Rendered PlaybackCore::renderSong (float* const* out, int numChannels, int numSamples, juce::int64 pos) noexcept
{
    Rendered r;
    r.start = pos;
    const auto length = (juce::int64) song->buffer.getNumSamples();

    // 練習のテンポ / キー（B11）：変えている間だけ Rubber Band を通す。切り替わる時は 20 ms でなめらかに入る
    const bool wantStretch = isPracticeShifted() && stretcher != nullptr;
    if (wantStretch != stretching)
    {
        stretching = wantStretch;
        if (wantStretch)
        {
            heard = (double) pos;
            stretchReset = true;
        }
        else
        {
            pos = juce::jlimit ((juce::int64) 0, length, (juce::int64) std::llround (heard));
            r.start = pos;
            fraction = 0.0;
        }
        fade.setCurrentAndTargetValue (0.0f);
    }
    if (stretching)
        return renderStretched (out, numChannels, numSamples, *song);

    smoothedGain.setTargetValue (muted ? 0.0f : gain.load());
    for (int k = 0; k < maxStems; ++k)
        stemSmoothed[k].setTargetValue (stemGain[k].load());
    fade.setTargetValue (1.0f);

    const auto rate = outputRate.load();
    const auto ratio = rate > 0.0 ? song->sampleRate / rate : 1.0;   // 1.0 なら変換なし（ふつう）
    const bool convert = std::abs (ratio - 1.0) > 1.0e-9;
    const bool loop = loopOn.load();
    const auto in = loopIn.load(), outPoint = juce::jmin (loopOut.load(), length);

    int i = 0;
    for (; i < numSamples; ++i)
    {
        if (loop && pos >= outPoint && outPoint > in)
        {
            pos = in + (pos - outPoint);
            r.wrapped = true;
        }

        if (pos >= length)
        {
            playing = false;
            reachedEnd = true;
            pos = length;
            break;
        }

        const auto g = nextGains();
        const auto f = fade.getNextValue();
        for (int c = 0; c < numChannels; ++c)
        {
            if (out[c] == nullptr) continue;
            auto v = mixAt (*song, c, pos, g);
            if (convert)
            {
                const auto next = pos + 1 < length ? mixAt (*song, c, pos + 1, g) : v;
                v += (next - v) * (float) fraction;
            }
            out[c][i] = v * f;
        }

        // クリック：このサンプルで聞こえる曲の位置から（フェードは掛けない。素通しと伸ばす時の切り替えでも拍は途切れない）
        const auto heardPos = (double) pos + (convert ? fraction : 0.0);
        addClick (out, numChannels, i, metronome.next (heardPos, clickAudible (heardPos)));

        if (convert)
        {
            fraction += ratio;
            const auto whole = (juce::int64) fraction;
            pos += whole;
            fraction -= (double) whole;
        }
        else
        {
            ++pos;
        }
    }

    addClickTail (out, numChannels, i, numSamples);   // 曲の終わりの後
    position = pos;
    heard = (double) pos;
    r.played = i;
    r.step = convert ? ratio : 1.0;
    return r;
}

PlaybackCore::Rendered PlaybackCore::renderStretched (float* const* out, int numChannels, int numSamples, const SongAudio& s) noexcept
{
    Rendered r;
    auto& st = *stretcher;
    const auto length = (juce::int64) s.buffer.getNumSamples();
    const auto chans = stretchIn.getNumChannels();

    const auto rate = outputRate.load();
    const auto srRatio = rate > 0.0 ? s.sampleRate / rate : 1.0;
    const auto step = speed.load() * srRatio;                                           // 出力 1 サンプルで進む曲のサンプル
    const auto timeRatio = 1.0 / step;
    const auto pitchScale = std::pow (2.0, semitones.load() / 12.0) * srRatio;          // 出力の SR で鳴らすぶんを打ち消す
    const bool loop = loopOn.load();
    const auto in = loopIn.load(), outPoint = juce::jmin (loopOut.load(), length);

    float* inPtrs[2] = { stretchIn.getWritePointer (0), stretchIn.getWritePointer (chans - 1) };
    float* outPtrs[2] = { stretchOut.getWritePointer (0), stretchOut.getWritePointer (chans - 1) };

    if (stretchReset.exchange (false))
    {
        st.reset();
        st.setTimeRatio (timeRatio);
        st.setPitchScale (pitchScale);
        appliedTime = timeRatio;
        appliedPitch = pitchScale;
        feedPos = juce::jlimit ((juce::int64) 0, length, (juce::int64) std::llround (heard));
        stretchIn.clear();
        for (auto pad = (int) st.getPreferredStartPad(); pad > 0;)
        {
            const auto n = juce::jmin (pad, stretchBlock);
            st.process (inPtrs, (size_t) n, false);
            pad -= n;
        }
        dropLeft = (int) st.getStartDelay();
        fade.setCurrentAndTargetValue (0.0f);
    }
    else
    {
        if (! juce::exactlyEqual (timeRatio, appliedTime))   { st.setTimeRatio (timeRatio);   appliedTime = timeRatio; }
        if (! juce::exactlyEqual (pitchScale, appliedPitch)) { st.setPitchScale (pitchScale); appliedPitch = pitchScale; }
    }
    smoothedGain.setTargetValue (muted ? 0.0f : gain.load());
    for (int k = 0; k < maxStems; ++k)
        stemSmoothed[k].setTargetValue (stemGain[k].load());
    fade.setTargetValue (1.0f);

    r.start = (juce::int64) std::llround (heard);
    r.step = step;
    int i = 0, guard = 0;
    while (i < numSamples && guard++ < 256)
    {
        const auto avail = st.available();
        if (avail > 0)
        {
            if (dropLeft > 0)   // 頭の遅れの分は捨てる（聞こえる位置と heard をそろえる）
            {
                const auto n = juce::jmin (avail, dropLeft, stretchBlock);
                st.retrieve (outPtrs, (size_t) n);
                dropLeft -= n;
                continue;
            }
            const auto n = juce::jmin (avail, numSamples - i, stretchBlock);
            st.retrieve (outPtrs, (size_t) n);
            for (int k = 0; k < n; ++k, ++i)
            {
                if (loop && heard >= (double) outPoint && outPoint > in)
                {
                    heard = (double) in + (heard - (double) outPoint);
                    r.wrapped = true;
                }
                if (heard >= (double) length)
                {
                    playing = false;
                    reachedEnd = true;
                    heard = (double) length;
                    position = length;
                    r.played = i;
                    addClickTail (out, numChannels, i, numSamples);
                    return r;
                }
                const auto f = fade.getNextValue();
                for (int c = 0; c < numChannels; ++c)
                    if (out[c] != nullptr)
                        out[c][i] = stretchOut.getSample (juce::jmin (c, chans - 1), k) * f;
                // クリックは伸ばした後に足す（聞こえている位置 heard で拍を数える。キーで高さが変わらない）
                addClick (out, numChannels, i, metronome.next (heard, clickAudible (heard)));
                heard += step;
            }
            continue;
        }

        // 足りない分を入れる（ループの終わりで頭へ、曲の後ろは無音）
        auto need = (int) st.getSamplesRequired();
        need = juce::jlimit (1, stretchBlock, need > 0 ? need : 256);
        for (int k = 0; k < need; ++k)
        {
            if (loop && feedPos >= outPoint && outPoint > in)
                feedPos = in + (feedPos - outPoint);
            const auto g = nextGains();   // 音量は入れる側で掛ける（変えてから聞こえるまで Rubber Band の遅れの分かかる）
            for (int c = 0; c < chans; ++c)
                inPtrs[c][k] = feedPos < length ? mixAt (s, c, feedPos, g) : 0.0f;
            ++feedPos;
        }
        st.process (inPtrs, (size_t) need, false);
    }

    addClickTail (out, numChannels, i, numSamples);   // Rubber Band が出しきれなかった分（ふつうは無い）
    position = (juce::int64) std::llround (heard);
    r.played = i;
    return r;
}
} // namespace vb::audio
