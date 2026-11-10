/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QtGlobal>

#ifdef Q_OS_ANDROID

#include "audio/iaudiosink.h"
#include "util/interface.h"

#include <QByteArray>
#include <QObject>

#include <aaudio/AAudio.h>
#include <atomic>
#include <mutex>
#include <vector>

class AndroidAudio;

/**
 * @brief Plays a call's voice or a sound, each on a stream of its own.
 */
class AndroidSink : public QObject, public IAudioSink
{
    Q_OBJECT
public:
    explicit AndroidSink(AndroidAudio& audio);
    AndroidSink(const AndroidSink&) = delete;
    AndroidSink& operator=(const AndroidSink&) = delete;
    AndroidSink(AndroidSink&&) = delete;
    AndroidSink& operator=(AndroidSink&&) = delete;
    ~AndroidSink() override;

    void playAudioBuffer(const int16_t* data, int samples, unsigned channels,
                         int sampleRate) const override;
    void playMono16Sound(const IAudioSink::Sound& sound) override;
    void startLoop() override;
    void stopLoop() override;

    explicit operator bool() const override;

    void kill();

    SIGNAL_IMPL(AndroidSink, finishedPlaying, void)
    SIGNAL_IMPL(AndroidSink, invalidated, void)

private:
    static aaudio_data_callback_result_t playSound(AAudioStream* stream, void* userData,
                                                   void* audioData, int32_t numFrames);
    static void onSoundError(AAudioStream* stream, void* userData, aaudio_result_t error);
    // With the mutex held.
    void stop() const;
    // Says that the sound ended, once.
    void finished();

    AndroidAudio& audio;
    mutable std::mutex mutex;
    bool killed = false;
    mutable AAudioStream* stream = nullptr;
    // The voice's format while the stream plays one.
    mutable unsigned voiceChannels = 0;
    mutable int voiceRate = 0;
    mutable std::vector<int16_t> scaled;

    // While the stream plays a sound, only its callback uses these.
    IAudioSink::Sound sound = IAudioSink::Sound::Test;
    QByteArray soundData;
    qsizetype position = 0;
    std::atomic<bool> loop{false};
    std::atomic<bool> soundPlaying{false};
};

#endif // Q_OS_ANDROID
