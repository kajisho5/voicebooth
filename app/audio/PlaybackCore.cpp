#include "PlaybackCore.h"

namespace vb::audio
{
void PlaybackCore::setSong (std::shared_ptr<const SongAudio> newSong)
{
    playing = false;
    pendingSeek = -1;
    position = 0;
    loopOn = false;
    reachedEnd = false;

    // 古い曲は lock の外で解放する（オーディオスレッドを待たせない）
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        std::swap (song, newSong);
    }
    newSong.reset();
}

bool PlaybackCore::hasSong() const
{
    const juce::SpinLock::ScopedLockType sl (songLock);
    return song != nullptr;
}

void PlaybackCore::prepare (double outputSampleRate)
{
    outputRate = outputSampleRate;
    smoothedGain.reset (outputSampleRate > 0.0 ? outputSampleRate : 48000.0, 0.02);   // 20 ms でなめらかに
    smoothedGain.setCurrentAndTargetValue (muted ? 0.0f : gain.load());
    fraction = 0.0;
    prepared = true;
}

void PlaybackCore::play()
{
    reachedEnd = false;
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
    }

    r.start = pos;
    if (! playing)
    {
        position = pos;
        return r;
    }

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
    r.played = i;
    return r;
}
} // namespace vb::audio
