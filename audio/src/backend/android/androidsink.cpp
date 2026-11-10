/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "audio/src/backend/android/androidsink.h"

#ifdef Q_OS_ANDROID

#include "audio/src/backend/android/aaudioapi.h"
#include "audio/src/backend/android/androidaudio.h"

#include <QDebug>
#include <QFile>

#include <algorithm>

AndroidSink::AndroidSink(AndroidAudio& audio_)
    : audio{audio_}
{
}

AndroidSink::~AndroidSink()
{
    {
        const std::lock_guard<std::mutex> locker{mutex};
        stop();
    }
    audio.destroySink(*this);
}

void AndroidSink::playAudioBuffer(const int16_t* data, int samples, unsigned channels, int sampleRate) const
{
    const std::lock_guard<std::mutex> locker{mutex};
    if (killed) {
        qCritical() << "Trying to play audio on an invalid sink";
        return;
    }
    if (stream == nullptr || voiceChannels != channels || voiceRate != sampleRate) {
        stop();
        stream = audio.openVoice(static_cast<int32_t>(channels), sampleRate);
        if (stream == nullptr) {
            return;
        }
        voiceChannels = channels;
        voiceRate = sampleRate;
    }

    const float volume = audio.outputVolume();
    const int16_t* voice = data;
    if (volume < 1) {
        scaled.assign(data, data + samples * channels);
        for (auto& sample : scaled) {
            sample = static_cast<int16_t>(sample * volume);
        }
        voice = scaled.data();
    }
    // What doesn't fit is dropped, as the OpenAL backend does.
    const aaudio_result_t written = audio.api.streamWrite(stream, voice, samples, 0);
    if (written < 0) {
        qWarning() << "Voice stopped:" << audio.api.convertResultToText(written);
        // Disconnected, as when a headset is unplugged: the next buffer opens a stream on the
        // device Android chooses now.
        stop();
        return;
    }
    if (audio.api.streamGetState(stream) == AAUDIO_STREAM_STATE_OPEN) {
        audio.api.streamRequestStart(stream);
    }
}

void AndroidSink::playMono16Sound(const IAudioSink::Sound& sound_)
{
    QFile file(IAudioSink::getSound(sound_));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open sound file" << file.fileName();
        return;
    }
    const QByteArray data = file.readAll();
    if (data.size() < static_cast<qsizetype>(sizeof(int16_t))) {
        qWarning() << "Sound file contained no data";
        return;
    }

    const std::lock_guard<std::mutex> locker{mutex};
    if (killed) {
        qCritical() << "Trying to play sound on an invalid sink";
        return;
    }
    // A new sound interrupts what plays.
    stop();
    sound = sound_;
    soundData = data;
    position = 0;
    loop = false;
    stream = audio.openSound(sound, &AndroidSink::playSound, &AndroidSink::onSoundError, this);
    if (stream != nullptr) {
        soundPlaying = true;
        audio.api.streamRequestStart(stream);
    }
}

void AndroidSink::startLoop()
{
    loop = true;
}

void AndroidSink::stopLoop()
{
    loop = false;
    const std::lock_guard<std::mutex> locker{mutex};
    if (soundPlaying) {
        stop();
        finished();
    }
}

AndroidSink::operator bool() const
{
    const std::lock_guard<std::mutex> locker{mutex};
    return !killed;
}

void AndroidSink::kill()
{
    {
        const std::lock_guard<std::mutex> locker{mutex};
        killed = true;
        stop();
    }
    emit invalidated();
}

aaudio_data_callback_result_t AndroidSink::playSound(AAudioStream* stream, void* userData,
                                                     void* audioData, int32_t numFrames)
{
    std::ignore = stream;
    auto& sink = *static_cast<AndroidSink*>(userData);
    auto* const out = static_cast<int16_t*>(audioData);
    const auto* const data = reinterpret_cast<const int16_t*>(sink.soundData.constData());
    const qsizetype length = sink.soundData.size() / static_cast<qsizetype>(sizeof(int16_t));
    const float volume = sink.audio.outputVolume();

    for (int32_t i = 0; i < numFrames; ++i) {
        if (sink.position == length) {
            if (!sink.loop) {
                std::fill(out + i, out + numFrames, 0);
                sink.finished();
                return AAUDIO_CALLBACK_RESULT_STOP;
            }
            sink.position = 0;
        }
        out[i] = static_cast<int16_t>(data[sink.position++] * volume);
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AndroidSink::onSoundError(AAudioStream* stream, void* userData, aaudio_result_t error)
{
    auto* const sink = static_cast<AndroidSink*>(userData);
    qWarning() << "Sound stopped:" << sink->audio.api.convertResultToText(error);
    if (error != AAUDIO_ERROR_DISCONNECTED) {
        return;
    }
    // Disconnected, as when a headset is unplugged. A stream can't be closed from its own
    // callbacks, so the sound starts again on the device Android chooses now from the sink's
    // thread.
    QMetaObject::invokeMethod(
        sink,
        [sink, stream] {
            bool restart = false;
            {
                const std::lock_guard<std::mutex> locker{sink->mutex};
                restart = sink->stream == stream && !sink->killed;
            }
            if (restart) {
                const bool looping = sink->loop;
                sink->playMono16Sound(sink->sound);
                sink->loop = looping;
            }
        },
        Qt::QueuedConnection);
}

void AndroidSink::stop() const
{
    // The sound's callback stops before this returns.
    audio.closeStream(stream);
    stream = nullptr;
    voiceChannels = 0;
    voiceRate = 0;
}

void AndroidSink::finished()
{
    if (!soundPlaying.exchange(false)) {
        return;
    }
    // On the sink's thread, not the stream's.
    QMetaObject::invokeMethod(this, [this] { emit finishedPlaying(); }, Qt::QueuedConnection);
}

#endif // Q_OS_ANDROID
