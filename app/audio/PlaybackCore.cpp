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

    // 古い曲は lock の外で解放する（オーディオスレッドを待たせない）
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        std::swap (song, newSong);
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
    smoothedGain.reset (outputSampleRate > 0.0 ? outputSampleRate : 48000.0, 0.02);   // 20 ms でなめらかに
    smoothedGain.setCurrentAndTargetValue (muted ? 0.0f : gain.load());
    fraction = 0.0;
    prepared = true;
}

void PlaybackCore::play()
{
    reachedEnd = false;
    stretchReset = true;    // 止まる前に Rubber Band に残っていた音を鳴らさない
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
    }

    r.start = pos;
    if (! playing)
    {
        position = pos;
        heard = (double) pos;
        return r;
    }

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
        smoothedGain.setCurrentAndTargetValue (0.0f);
    }
    if (stretching)
        return renderStretched (out, numChannels, numSamples, *song);

    smoothedGain.setTargetValue (muted ? 0.0f : gain.load());

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

        const auto g = smoothedGain.getNextValue();
        for (int c = 0; c < numChannels; ++c)
        {
            if (out[c] == nullptr) continue;
            const auto* src = buffer.getReadPointer (juce::jmin (c, songChannels - 1));   // モノラルの曲は両耳へ
            auto v = src[pos];
            if (convert)
            {
                const auto next = pos + 1 < length ? src[pos + 1] : v;
                v += (next - v) * (float) fraction;
            }
            out[c][i] = v * g;
        }

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
    const auto& buffer = s.buffer;
    const auto length = (juce::int64) buffer.getNumSamples();
    const auto songChannels = buffer.getNumChannels();
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
        smoothedGain.setCurrentAndTargetValue (0.0f);
    }
    else
    {
        if (! juce::exactlyEqual (timeRatio, appliedTime))   { st.setTimeRatio (timeRatio);   appliedTime = timeRatio; }
        if (! juce::exactlyEqual (pitchScale, appliedPitch)) { st.setPitchScale (pitchScale); appliedPitch = pitchScale; }
    }
    smoothedGain.setTargetValue (muted ? 0.0f : gain.load());

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
                    return r;
                }
                const auto g = smoothedGain.getNextValue();
                for (int c = 0; c < numChannels; ++c)
                    if (out[c] != nullptr)
                        out[c][i] = stretchOut.getSample (juce::jmin (c, chans - 1), k) * g;
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
            for (int c = 0; c < chans; ++c)
                inPtrs[c][k] = feedPos < length ? buffer.getSample (juce::jmin (c, songChannels - 1), (int) feedPos) : 0.0f;
            ++feedPos;
        }
        st.process (inPtrs, (size_t) need, false);
    }

    position = (juce::int64) std::llround (heard);
    r.played = i;
    return r;
}
} // namespace vb::audio
