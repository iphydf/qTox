/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "audio/src/backend/android/androidaudio.h"

#ifdef Q_OS_ANDROID

#include "audio/iaudiosettings.h"
#include "audio/src/backend/android/aaudioapi.h"
#include "audio/src/backend/android/androidsink.h"
#include "audio/src/backend/android/androidsource.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QMutexLocker>

#include <chrono>
#include <utility>

namespace {
namespace logcat {
Q_LOGGING_CATEGORY(audio, "qtox.audio")
} // namespace logcat

// The most a call's voice waits to be played, as the OpenAL backend's 16 buffers of 20 ms.
constexpr int32_t maxVoiceDelayMs = 320;
// How soon the input thread notices that it should stop.
constexpr std::chrono::nanoseconds readTimeout = std::chrono::milliseconds{100};

aaudio_usage_t usage(IAudioSink::Sound sound)
{
    switch (sound) {
    case IAudioSink::Sound::IncomingCall:
        return AAUDIO_USAGE_NOTIFICATION_RINGTONE;
    case IAudioSink::Sound::OutgoingCall:
    case IAudioSink::Sound::CallEnd:
        return AAUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING;
    case IAudioSink::Sound::NewMessage:
    case IAudioSink::Sound::Test:
        return AAUDIO_USAGE_NOTIFICATION;
    }
    return AAUDIO_USAGE_NOTIFICATION;
}
} // namespace

std::unique_ptr<AndroidAudio> AndroidAudio::make(IAudioSettings& settings)
{
    const AAudioApi* const api = AAudioApi::get();
    if (api == nullptr) {
        return {};
    }
    return std::make_unique<AndroidAudio>(*api, settings);
}

AndroidAudio::AndroidAudio(const AAudioApi& api_, IAudioSettings& settings_)
    : api{api_}
    , settings{settings_}
    , volume{static_cast<float>(settings_.getOutVolume()) * 0.01f}
{
    gate.setGain(settings.getAudioInGainDecibel());
    gate.setThreshold(settings.getAudioThreshold());
    inputThread = std::thread([this] { record(); });
}

AndroidAudio::~AndroidAudio()
{
    {
        const std::lock_guard<std::mutex> state{inputMutex};
        quitting = true;
    }
    inputChanged.notify_one();
    inputThread.join();
}

void AndroidAudio::setOutputVolume(qreal volume_)
{
    volume = static_cast<float>(qBound(minOutputVolume(), volume_, maxOutputVolume()));
}

qreal AndroidAudio::maxOutputVolume() const
{
    return 1;
}

qreal AndroidAudio::minOutputVolume() const
{
    return 0;
}

float AndroidAudio::outputVolume() const
{
    return volume;
}

qreal AndroidAudio::minInputGain() const
{
    return VoiceGate::minGain;
}

qreal AndroidAudio::maxInputGain() const
{
    return VoiceGate::maxGain;
}

qreal AndroidAudio::inputGain() const
{
    return gate.gain();
}

void AndroidAudio::setInputGain(qreal dB)
{
    gate.setGain(dB);
}

qreal AndroidAudio::minInputThreshold() const
{
    return VoiceGate::minThreshold;
}

qreal AndroidAudio::maxInputThreshold() const
{
    return VoiceGate::maxThreshold;
}

qreal AndroidAudio::getInputThreshold() const
{
    return gate.threshold();
}

void AndroidAudio::setInputThreshold(qreal normalizedThreshold)
{
    gate.setThreshold(normalizedThreshold);
}

void AndroidAudio::reinitInput(const QString& inDevDesc)
{
    std::ignore = inDevDesc;
    QMutexLocker<QRecursiveMutex> locker{&lock};
    const auto killed = sources;
    sources.clear();
    {
        const std::lock_guard<std::mutex> state{inputMutex};
        inputWanted = false;
    }
    inputChanged.notify_one();
    locker.unlock();
    // Outside the lock: their owners make new ones at once, which takes it.
    for (auto* source : killed) {
        source->kill();
    }
}

bool AndroidAudio::reinitOutput(const QString& outDevDesc)
{
    std::ignore = outDevDesc;
    QMutexLocker<QRecursiveMutex> locker{&lock};
    const auto killed = sinks;
    sinks.clear();
    locker.unlock();
    for (auto* sink : killed) {
        sink->kill();
    }
    return isOutputReady();
}

bool AndroidAudio::isOutputReady() const
{
    return settings.getAudioOutDevEnabled();
}

QStringList AndroidAudio::outDeviceNames()
{
    return {};
}

QStringList AndroidAudio::inDeviceNames()
{
    return {};
}

std::unique_ptr<IAudioSink> AndroidAudio::makeSink()
{
    if (!settings.getAudioOutDevEnabled()) {
        return {};
    }
    auto sink = std::make_unique<AndroidSink>(*this);
    const QMutexLocker<QRecursiveMutex> locker{&lock};
    sinks.insert(sink.get());
    return sink;
}

void AndroidAudio::destroySink(AndroidSink& sink)
{
    const QMutexLocker<QRecursiveMutex> locker{&lock};
    sinks.erase(&sink);
}

std::unique_ptr<IAudioSource> AndroidAudio::makeSource()
{
    if (!settings.getAudioInDevEnabled()) {
        return {};
    }
    auto source = std::make_unique<AndroidSource>(*this);
    const QMutexLocker<QRecursiveMutex> locker{&lock};
    if (sources.empty()) {
        {
            const std::lock_guard<std::mutex> state{inputMutex};
            inputWanted = true;
            inputFailed = false;
        }
        inputChanged.notify_one();
    }
    sources.insert(source.get());
    qCDebug(logcat::audio) << "Subscribed to audio input [" << sources.size() << "subscriptions ]";
    return source;
}

void AndroidAudio::destroySource(AndroidSource& source)
{
    const QMutexLocker<QRecursiveMutex> locker{&lock};
    if (sources.erase(&source) == 0 || !sources.empty()) {
        return;
    }
    {
        const std::lock_guard<std::mutex> state{inputMutex};
        inputWanted = false;
    }
    inputChanged.notify_one();
}

AAudioStream* AndroidAudio::openVoice(int32_t channels, int32_t sampleRate) const
{
    AAudioStreamBuilder* builder = nullptr;
    if (api.createStreamBuilder(&builder) != AAUDIO_OK) {
        return nullptr;
    }
    api.builderSetDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    api.builderSetFormat(builder, AAUDIO_FORMAT_PCM_I16);
    api.builderSetChannelCount(builder, channels);
    api.builderSetSampleRate(builder, sampleRate);
    api.builderSetUsage(builder, AAUDIO_USAGE_VOICE_COMMUNICATION);
    api.builderSetContentType(builder, AAUDIO_CONTENT_TYPE_SPEECH);
    api.builderSetBufferCapacityInFrames(builder, sampleRate * maxVoiceDelayMs / 1000);
    AAudioStream* stream = nullptr;
    const aaudio_result_t result = api.builderOpenStream(builder, &stream);
    api.builderDelete(builder);
    if (result != AAUDIO_OK) {
        qCWarning(logcat::audio) << "Cannot open a voice stream:" << api.convertResultToText(result);
        return nullptr;
    }
    // Writes may use all the capacity.
    api.streamSetBufferSizeInFrames(stream, api.streamGetBufferCapacityInFrames(stream));
    return stream;
}

AAudioStream* AndroidAudio::openSound(IAudioSink::Sound sound, AAudioStream_dataCallback callback,
                                      AAudioStream_errorCallback errorCallback, void* userData) const
{
    AAudioStreamBuilder* builder = nullptr;
    if (api.createStreamBuilder(&builder) != AAUDIO_OK) {
        return nullptr;
    }
    api.builderSetDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    api.builderSetFormat(builder, AAUDIO_FORMAT_PCM_I16);
    api.builderSetChannelCount(builder, 1);
    api.builderSetSampleRate(builder, AUDIO_SAMPLE_RATE);
    api.builderSetUsage(builder, usage(sound));
    api.builderSetContentType(builder, AAUDIO_CONTENT_TYPE_SONIFICATION);
    api.builderSetDataCallback(builder, callback, userData);
    api.builderSetErrorCallback(builder, errorCallback, userData);
    AAudioStream* stream = nullptr;
    const aaudio_result_t result = api.builderOpenStream(builder, &stream);
    api.builderDelete(builder);
    if (result != AAUDIO_OK) {
        qCWarning(logcat::audio) << "Cannot open a sound stream:" << api.convertResultToText(result);
        return nullptr;
    }
    return stream;
}

AAudioStream* AndroidAudio::openInput() const
{
    AAudioStreamBuilder* builder = nullptr;
    if (api.createStreamBuilder(&builder) != AAUDIO_OK) {
        return nullptr;
    }
    api.builderSetDirection(builder, AAUDIO_DIRECTION_INPUT);
    api.builderSetFormat(builder, AAUDIO_FORMAT_PCM_I16);
    api.builderSetChannelCount(builder, 1);
    api.builderSetSampleRate(builder, AUDIO_SAMPLE_RATE);
    api.builderSetInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
    AAudioStream* stream = nullptr;
    aaudio_result_t result = api.builderOpenStream(builder, &stream);
    api.builderDelete(builder);
    if (result == AAUDIO_OK) {
        result = api.streamRequestStart(stream);
        if (result != AAUDIO_OK) {
            api.streamClose(stream);
        }
    }
    if (result != AAUDIO_OK) {
        qCWarning(logcat::audio) << "Cannot record:" << api.convertResultToText(result);
        return nullptr;
    }
    qCDebug(logcat::audio) << "Recording";
    return stream;
}

void AndroidAudio::closeStream(AAudioStream* stream) const
{
    if (stream != nullptr) {
        api.streamRequestStop(stream);
        api.streamClose(stream);
    }
}

void AndroidAudio::record()
{
    AAudioStream* stream = nullptr;
    // Samples of the frame read so far.
    int32_t filled = 0;
    std::unique_lock<std::mutex> state{inputMutex};
    while (!quitting) {
        if (!inputWanted || inputFailed) {
            closeStream(stream);
            stream = nullptr;
            filled = 0;
            inputChanged.wait(state);
            continue;
        }
        state.unlock();

        bool failed = false;
        if (stream == nullptr) {
            stream = openInput();
            failed = stream == nullptr;
        }
        if (stream != nullptr) {
            const aaudio_result_t result =
                api.streamRead(stream, frame.data() + filled,
                               static_cast<int32_t>(frame.size()) - filled, readTimeout.count());
            if (result >= 0) {
                filled += result;
                if (std::cmp_equal(filled, frame.size())) {
                    filled = 0;
                    sendFrame();
                }
            } else {
                // Disconnected, as when a headset is unplugged: a new stream records from the
                // device Android chooses now.
                qCWarning(logcat::audio) << "Recording stopped:" << api.convertResultToText(result);
                closeStream(stream);
                stream = nullptr;
                filled = 0;
                failed = result != AAUDIO_ERROR_DISCONNECTED;
            }
        }

        state.lock();
        inputFailed = inputFailed || failed;
    }
    state.unlock();
    closeStream(stream);
}

void AndroidAudio::sendFrame()
{
    const qreal frameVolume = gate.process(frame.data(), frame.size(), AUDIO_FRAME_DURATION);

    const QMutexLocker<QRecursiveMutex> locker{&lock};
    for (auto* source : sources) {
        emit source->volumeAvailable(frameVolume);
    }
    if (!gate.isOpen()) {
        return;
    }
    for (auto* source : sources) {
        emit source->frameAvailable(frame.data(), frame.size(), 1, AUDIO_SAMPLE_RATE);
    }
}

#endif // Q_OS_ANDROID
