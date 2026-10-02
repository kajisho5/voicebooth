#include "PlaybackEngine.h"
#include "MicPermission.h"

namespace vb::audio
{
PlaybackEngine::PlaybackEngine() = default;

PlaybackEngine::~PlaybackEngine()
{
    stopTimer();
    manager.removeChangeListener (this);
    manager.removeAudioCallback (this);
    manager.closeAudioDevice();
}

juce::String PlaybackEngine::openDevices (const juce::XmlElement* saved)
{
    permission = checkMicPermission();

    // 許可が無い（Mac）間は入力を開かない。保存した設定からも入力を外す
    std::unique_ptr<juce::XmlElement> outputOnly;
    if (saved != nullptr && ! inputAllowed())
    {
        outputOnly = std::make_unique<juce::XmlElement> (*saved);
        outputOnly->setAttribute ("audioInputDeviceName", juce::String());
        outputOnly->removeAttribute ("audioDeviceInChans");
        saved = outputOnly.get();
    }

    // 入力 1 ch（L）・出力 2 ch。前回の設定が戻せなければ既定のデバイス
    const auto ins = inputAllowed() ? 1 : 0;
    openError = manager.initialise (ins, 2, saved, true);
    if (openError.isNotEmpty() && ins > 0)
    {
        // 入力が原因で開けないことがある（使用中・拒否）。出力だけで開き直す（オフボは鳴らす）
        inputError = openError;
        openError = manager.initialise (0, 2, nullptr, true);
    }
    if (openError.isEmpty() && manager.getCurrentAudioDevice() == nullptr)
        openError = "no output device";

    manager.addAudioCallback (this);
    manager.addChangeListener (this);
    ensureMonoInput();
    lastSnapshot = takeSnapshot();
    lastProgressMs = juce::Time::getMillisecondCounter();
    startTimerHz (2);

    // Mac：まだ答えていなければ許可を求める（ダイアログ）。許可されたら入力を足す
    if (permission == MicPermission::asking)
    {
        juce::WeakReference<PlaybackEngine> weak (this);
        requestMicPermission ([weak] (bool granted)
        {
            if (weak == nullptr) return;
            weak->permission = granted ? MicPermission::granted : MicPermission::denied;
            if (granted)
                weak->ensureMonoInput();
            if (weak->onDeviceChange)
                weak->onDeviceChange (false);
        });
    }
    return openError;
}

bool PlaybackEngine::hasSeparateInputsAndOutputs() const
{
    auto* type = manager.getCurrentDeviceTypeObject();
    return type == nullptr || type->hasSeparateInputsAndOutputs();
}

DeviceSnapshot PlaybackEngine::takeSnapshot() const
{
    DeviceSnapshot s;
    const auto setup = manager.getAudioDeviceSetup();
    s.type = manager.getCurrentAudioDeviceType();
    s.input = setup.inputDeviceName;
    s.output = setup.outputDeviceName;
    if (auto* d = manager.getCurrentAudioDevice())
    {
        s.open = d->isPlaying();
        s.sampleRate = d->getCurrentSampleRate();
        s.bufferSize = d->getCurrentBufferSizeSamples();
    }
    return s;
}

juce::String PlaybackEngine::applySetup (juce::AudioDeviceManager::AudioDeviceSetup setup)
{
    const auto before = manager.getAudioDeviceSetup();
    core.stop();       // デバイスが開き直すので止める（位置はそのまま）
    meter.reset();

    auto error = manager.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
        manager.setAudioDeviceSetup (before, true);   // 元に戻す（出力まで失わない）

    lastSnapshot = takeSnapshot();
    lastProgressMs = juce::Time::getMillisecondCounter();   // 開き直した直後は止まり扱いしない
    graceUntilMs = lastProgressMs + 5000;                    // SR の切り替えは機器によって数秒かかる
    stalled = false;
    return error;
}

void PlaybackEngine::ensureMonoInput()
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr)
        return;

    auto setup = manager.getAudioDeviceSetup();

    if (! inputAllowed())
    {
        if (setup.inputDeviceName.isNotEmpty())
        {
            setup.inputDeviceName = {};
            setup.inputChannels.clear();
            applySetup (setup);
        }
        return;
    }

    // 入力が無ければ、このドライバの既定の入力
    if (setup.inputDeviceName.isEmpty())
    {
        auto* type = manager.getCurrentDeviceTypeObject();
        const auto names = type != nullptr ? type->getDeviceNames (true) : juce::StringArray();
        if (names.isEmpty())
            return;   // 入力デバイスが無い
        setup.inputDeviceName = hasSeparateInputsAndOutputs() || ! names.contains (setup.outputDeviceName)
                              ? names[juce::jmax (0, type->getDefaultDeviceIndex (true))]
                              : setup.outputDeviceName;   // ASIO などは入出力が同じ機器
    }

    // すでに 1 ch だけ開いていればそれを使う（前回の設定）
    const auto active = device->getActiveInputChannels();
    if (setup.inputDeviceName == manager.getAudioDeviceSetup().inputDeviceName && active.countNumberOfSetBits() == 1)
    {
        wantedChannel = active.getHighestBit();
        inputError = {};
        return;
    }

    setup.useDefaultInputChannels = false;
    setup.inputChannels = inputChannelMask (wantedChannel);
    auto error = applySetup (setup);

    // 選んだチャンネルが無い機器（モノラルのマイクなど）は L に
    if (auto* d = manager.getCurrentAudioDevice(); error.isEmpty() && d != nullptr && d->getActiveInputChannels().isZero())
    {
        const auto available = d->getInputChannelNames().size();
        if (available > 0 && wantedChannel != 0)
        {
            wantedChannel = resolveInputChannel (wantedChannel, available);
            setup.inputChannels = inputChannelMask (wantedChannel);
            error = applySetup (setup);
        }
    }
    inputError = error;
}

//==============================================================================
void PlaybackEngine::setSong (std::shared_ptr<const SongAudio> song)
{
    songRate = song != nullptr ? song->sampleRate : 0.0;
    core.setSong (std::move (song));
    if (songRate > 0.0)
        matchDeviceRateToSong (songRate);
}

void PlaybackEngine::matchDeviceRateToSong (double rate)
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr || std::abs (device->getCurrentSampleRate() - rate) < 0.5)
        return;

    // DESIGN 13：原則リサンプルしない。デバイスが対応していれば曲の SR に切り替える（入力も同じ SR になる）
    if (! device->getAvailableSampleRates().contains (rate))
        return;

    auto setup = manager.getAudioDeviceSetup();
    setup.sampleRate = rate;
    applySetup (setup);
}

void PlaybackEngine::setBackingLevel (float fader, bool muted)
{
    core.setGain (PlaybackCore::faderToGain (fader));
    core.setMuted (muted);
}

void PlaybackEngine::setSelfMonitor (float fader, bool muted)
{
    monitor.setGain (PlaybackCore::faderToGain (fader));
    monitor.setMuted (muted);
}

void PlaybackEngine::setMonitorReverb (float fader)
{
    monitor.setReverb (PlaybackCore::faderToGain (fader));
}

juce::String PlaybackEngine::startRecording (const juce::File& file, bool floatSamples, int64 tailSamples)
{
    auto* d = manager.getCurrentAudioDevice();
    if (d == nullptr || ! d->isPlaying() || stalled)
        return "no device";
    if (d->getActiveInputChannels().isZero())
        return "no input";
    // 曲と SR が違う（試聴用に変換している）時は録らない。書き出しは元曲の SR のまま（DESIGN 6.5 / 13）
    if (songRate <= 0.0 || std::abs (d->getCurrentSampleRate() - songRate) >= 0.5)
        return "sample rate";
    return recorder.begin (file, d->getCurrentSampleRate(), floatSamples, tailSamples);
}

juce::String PlaybackEngine::startLatencyProbe()
{
    auto* d = manager.getCurrentAudioDevice();
    if (d == nullptr || ! d->isPlaying() || stalled)
        return "no device";
    if (d->getActiveInputChannels().isZero())
        return "no input";
    if (d->getActiveOutputChannels().isZero())
        return "no output";
    if (recorder.isActive())
        return "recording";
    probe.start (d->getCurrentSampleRate());
    return {};
}

RecordedTake PlaybackEngine::stopRecording()
{
    const auto r = recorder.finish();
    RecordedTake t;
    t.file = r.file;
    t.startSample = r.startSample;
    t.length = r.length;
    t.peak = r.peak;
    t.clipped = r.clipped;
    t.dropped = r.dropped;
    return t;
}

OutputStatus PlaybackEngine::getOutputStatus() const
{
    OutputStatus st;
    st.error = openError;
    auto* device = manager.getCurrentAudioDevice();
    st.stalled = stalled || (device != nullptr && ! device->isPlaying());   // 開いたまま止まった（ドライバのエラー）
    if (device != nullptr && ! st.stalled && device->getActiveOutputChannels().countNumberOfSetBits() > 0)
    {
        const auto setup = manager.getAudioDeviceSetup();
        st.open = true;
        st.deviceName = setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : device->getName();
        st.typeName = device->getTypeName();
        st.sampleRate = device->getCurrentSampleRate();
        st.bufferSize = device->getCurrentBufferSizeSamples();
        st.converting = songRate > 0.0 && std::abs (st.sampleRate - songRate) >= 0.5;
    }
    else if (st.error.isEmpty() && ! st.stalled)
    {
        st.error = "no output device";
    }
    return st;
}

//==============================================================================
DeviceList PlaybackEngine::getDeviceList() const
{
    DeviceList l;
    auto& m = const_cast<juce::AudioDeviceManager&> (manager);   // 一覧を読むだけ（初回は機器を探す）
    for (auto* t : m.getAvailableDeviceTypes())
        l.types.add (t->getTypeName());

    l.currentType = manager.getCurrentAudioDeviceType();
    if (auto* type = manager.getCurrentDeviceTypeObject())
    {
        l.inputs = type->getDeviceNames (true);
        l.outputs = type->getDeviceNames (false);
    }

    const auto setup = manager.getAudioDeviceSetup();
    l.currentInput = setup.inputDeviceName;
    l.currentOutput = setup.outputDeviceName;

    if (auto* d = manager.getCurrentAudioDevice())
    {
        l.sampleRates = d->getAvailableSampleRates();
        l.bufferSizes = d->getAvailableBufferSizes();
        l.sampleRate = d->getCurrentSampleRate();
        l.bufferSize = d->getCurrentBufferSizeSamples();
    }
    return l;
}

void PlaybackEngine::rescanDevices()
{
    // ALSA など抜き差しを知らせないドライバのため、入力セットアップを開いた時に探し直す
    if (auto* type = manager.getCurrentDeviceTypeObject())
        type->scanForDevices();

    // 開けていない・応答しなくなったデバイスは開き直してみる（つなぎ直した・サウンドサーバーが戻った）
    auto* d = manager.getCurrentAudioDevice();
    if (stalled || d == nullptr || ! d->isPlaying())
        reopen();
}

void PlaybackEngine::reopen()
{
    const auto saved = manager.createStateXml();
    core.stop();
    meter.reset();
    manager.closeAudioDevice();
    openError = manager.initialise (inputAllowed() ? 1 : 0, 2, saved.get(), true);
    if (openError.isEmpty() && manager.getCurrentAudioDevice() == nullptr)
        openError = "no output device";
    ensureMonoInput();
    if (songRate > 0.0)
        matchDeviceRateToSong (songRate);

    lastSnapshot = takeSnapshot();
    lastProgressMs = juce::Time::getMillisecondCounter();
    stalled = false;
    if (onDeviceChange)
        onDeviceChange (false);
}

juce::String PlaybackEngine::setDeviceType (const juce::String& typeName)
{
    const auto oldType = manager.getCurrentAudioDeviceType();
    if (typeName == oldType)
        return {};

    const auto before = manager.getAudioDeviceSetup();
    core.stop();
    meter.reset();
    manager.setCurrentAudioDeviceType (typeName, true);

    juce::String error;
    if (manager.getCurrentAudioDevice() == nullptr)
    {
        // 新しいドライバで開けない → 元のドライバに戻す
        error = "can't open " + typeName;
        manager.setCurrentAudioDeviceType (oldType, true);
        manager.setAudioDeviceSetup (before, true);
    }
    ensureMonoInput();
    if (songRate > 0.0)
        matchDeviceRateToSong (songRate);
    lastSnapshot = takeSnapshot();
    return error;
}

juce::String PlaybackEngine::setInputDevice (const juce::String& name)
{
    auto setup = manager.getAudioDeviceSetup();
    if (setup.inputDeviceName == name)
        return {};
    setup.inputDeviceName = name;
    if (! hasSeparateInputsAndOutputs())
        setup.outputDeviceName = name;      // ASIO などは入出力が同じ機器
    setup.useDefaultInputChannels = false;
    wantedChannel = 0;                       // 別の機器は L から
    setup.inputChannels = inputChannelMask (0);
    inputError = applySetup (setup);
    return inputError;
}

juce::String PlaybackEngine::setOutputDevice (const juce::String& name)
{
    auto setup = manager.getAudioDeviceSetup();
    if (setup.outputDeviceName == name)
        return {};
    setup.outputDeviceName = name;
    if (! hasSeparateInputsAndOutputs() && setup.inputDeviceName.isNotEmpty())
        setup.inputDeviceName = name;       // ASIO などは入出力が同じ機器
    const auto error = applySetup (setup);
    if (error.isEmpty() && songRate > 0.0)
        matchDeviceRateToSong (songRate);   // 新しい機器も曲の SR に
    return error;
}

juce::String PlaybackEngine::setInputChannel (int channel)
{
    auto* d = manager.getCurrentAudioDevice();
    const auto ch = resolveInputChannel (channel, d != nullptr ? d->getInputChannelNames().size() : 0);
    if (ch < 0)
        return {};

    auto setup = manager.getAudioDeviceSetup();
    wantedChannel = ch;
    setup.useDefaultInputChannels = false;
    setup.inputChannels = inputChannelMask (ch);
    inputError = applySetup (setup);
    return inputError;
}

juce::String PlaybackEngine::setBufferSize (int samples)
{
    auto setup = manager.getAudioDeviceSetup();
    if (setup.bufferSize == samples)
        return {};
    setup.bufferSize = samples;
    return applySetup (setup);
}

void PlaybackEngine::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // 自分で変えた分は lastSnapshot に入っている。違えば外から変わった（抜けた・OS で切り替えた・止まった）
    const auto now = takeSnapshot();
    if (now == lastSnapshot)
        return;

    const auto lost = deviceLost (lastSnapshot, now);
    lastSnapshot = now;
    core.stop();
    meter.reset();
    if (onDeviceChange)
        onDeviceChange (lost);
}

void PlaybackEngine::timerCallback()
{
    // ピッチの検出（B8）はデバイスの SR に合わせて準備する（メッセージスレッドで。確保とスレッドの起動があるので）
    if (auto* dev = manager.getCurrentAudioDevice(); dev != nullptr && std::abs (dev->getCurrentSampleRate() - pitch.getSampleRate()) > 0.5)
        pitch.prepare (dev->getCurrentSampleRate());

    const auto now = juce::Time::getMillisecondCounter();
    const auto count = callbacks.load();
    if (count != lastCallbacks)
    {
        lastCallbacks = count;
        lastProgressMs = now;
        if (stalled)
        {
            stalled = false;   // 自然に戻った
            lastSnapshot = takeSnapshot();
            if (onDeviceChange)
                onDeviceChange (false);
        }
        return;
    }

    // 開いていたのに、ドライバが止まった・1.5 秒コールバックが来ない → 止まった（抜けた・サウンドサーバーが落ちた）
    auto* d = manager.getCurrentAudioDevice();
    const bool running = d != nullptr && d->isPlaying();
    if (now < graceUntilMs && running)
        return;   // 開き直した直後：コールバックが戻るのを待つ
    if (! stalled && lastSnapshot.open && (! running || now - lastProgressMs > 1500))
    {
        stalled = true;
        core.stop();
        meter.reset();
        lastSnapshot = takeSnapshot();
        if (onDeviceChange)
            onDeviceChange (true);
    }
}

//==============================================================================
InputStatus PlaybackEngine::getInputStatus() const
{
    InputStatus st;
    st.permission = permission;
    st.error = inputError;

    auto* d = manager.getCurrentAudioDevice();
    const auto setup = manager.getAudioDeviceSetup();
    const auto active = d != nullptr ? d->getActiveInputChannels() : juce::BigInteger();

    if (d != nullptr && d->isPlaying() && ! active.isZero() && ! stalled)
    {
        st.open = true;
        st.problem = InputProblem::none;
        st.deviceName = setup.inputDeviceName.isNotEmpty() ? setup.inputDeviceName : d->getName();
        st.typeName = d->getTypeName();
        st.channel = active.getHighestBit();
        st.numChannels = d->getInputChannelNames().size();
        st.sampleRate = d->getCurrentSampleRate();
        st.bufferSize = d->getCurrentBufferSizeSamples();
        st.inputLatency = d->getInputLatencyInSamples();
        st.outputLatency = d->getOutputLatencyInSamples();
        st.silent = meter.isDigitalSilence();
        st.bluetooth = looksLikeBluetooth (st.deviceName);
        return st;
    }

    if (stalled || (d != nullptr && ! d->isPlaying())) st.problem = InputProblem::stalled;
    else if (permission == MicPermission::denied)  st.problem = InputProblem::permissionDenied;
    else if (permission == MicPermission::asking)  st.problem = InputProblem::permissionAsking;
    else if (inputError.isNotEmpty())              st.problem = InputProblem::openFailed;
    else if (setup.inputDeviceName.isNotEmpty())   st.problem = InputProblem::noChannels;
    else                                           st.problem = InputProblem::noDevice;

    if (d != nullptr)
    {
        st.typeName = d->getTypeName();
        st.sampleRate = d->getCurrentSampleRate();
        st.bufferSize = d->getCurrentBufferSizeSamples();
    }
    return st;
}

//==============================================================================
void PlaybackEngine::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                                                       int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    callbacks.fetch_add (1, std::memory_order_relaxed);

    // 入力は 1 ch だけ開いている（inputs[0]）。メーターに通す（録音 B5 はここで、モニターより前で取る）
    const float* input = numInputs > 0 && inputs != nullptr ? inputs[0] : nullptr;
    if (input != nullptr)
        meter.process (input, numSamples);

    // 往復の遅れを測っている間（B6）は、出力は測定音だけ（曲・自分の声は鳴らさない。自分の声を返すと測定音が回り込む）
    if (probe.process (input, outputs, numOutputs, numSamples))
        return;

    // オフボ（出力を全部書く）。鳴らした曲の範囲に合わせて素の声を録る（B5）
    const auto played = core.render (outputs, numOutputs, numSamples);
    recorder.process (input, numSamples, played.start, played.played, played.wrapped);
    pitch.push (input, numSamples, played.start, played.wrapped ? 0 : played.played);   // 自分の声のピッチ（B8）。検出は別のスレッド

    // 自分の声（とモニターリバーブ）を足す
    monitor.process (input, outputs, numOutputs, numSamples);
}

void PlaybackEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    core.prepare (device->getCurrentSampleRate());
    meter.prepare (device->getCurrentSampleRate());
    monitor.prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void PlaybackEngine::audioDeviceStopped()
{
}
} // namespace vb::audio
