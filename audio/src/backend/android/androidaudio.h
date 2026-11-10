/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QtGlobal>

#ifdef Q_OS_ANDROID

#include "audio/iaudiocontrol.h"
#include "audio/iaudiosink.h"
#include "audio/src/backend/voicegate.h"

#include <QMutex>

#include <aaudio/AAudio.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

struct AAudioApi;
class AndroidSink;
class AndroidSource;
class IAudioSettings;

/**
 * @brief Plays and records through Android's AAudio, telling Android what each stream is: calls,
 *        which Android routes as calls and cancels their echo where the device can, and ringing,
 *        which goes where ringtones go.
 */
class AndroidAudio : public IAudioControl
{
    Q_OBJECT

public:
    // Null before Android 9.
    static std::unique_ptr<AndroidAudio> make(IAudioSettings& settings);
    AndroidAudio(const AAudioApi& api, IAudioSettings& settings);
    ~AndroidAudio() override;

    void setOutputVolume(qreal volume) override;
    qreal maxOutputVolume() const override;
    qreal minOutputVolume() const override;

    qreal minInputGain() const override;
    qreal maxInputGain() const override;
    qreal inputGain() const override;
    void setInputGain(qreal dB) override;

    qreal minInputThreshold() const override;
    qreal maxInputThreshold() const override;
    qreal getInputThreshold() const override;
    void setInputThreshold(qreal normalizedThreshold) override;

    // Android chooses the devices.
    void reinitInput(const QString& inDevDesc) override;
    bool reinitOutput(const QString& outDevDesc) override;
    bool isOutputReady() const override;
    QStringList outDeviceNames() override;
    QStringList inDeviceNames() override;

    std::unique_ptr<IAudioSink> makeSink() override;
    std::unique_ptr<IAudioSource> makeSource() override;

    void destroySink(AndroidSink& sink);
    void destroySource(AndroidSource& source);

    const AAudioApi& api;
    // Between 0 and 1.
    float outputVolume() const;
    // A stream for a call's voice that takes writes without blocking, or null.
    AAudioStream* openVoice(int32_t channels, int32_t sampleRate) const;
    // A stream for a sound that asks @p callback for it, or null.
    AAudioStream* openSound(IAudioSink::Sound sound, AAudioStream_dataCallback callback,
                            AAudioStream_errorCallback errorCallback, void* userData) const;
    void closeStream(AAudioStream* stream) const;

private:
    AAudioStream* openInput() const;
    // The input thread: reads frames while there are sources.
    void record();
    void sendFrame();

    IAudioSettings& settings;
    std::atomic<float> volume;
    VoiceGate gate;

    QRecursiveMutex lock;
    std::unordered_set<AndroidSink*> sinks;
    std::unordered_set<AndroidSource*> sources;

    // What the input thread should do: inputChanged wakes it when this changes.
    std::mutex inputMutex;
    std::condition_variable inputChanged;
    bool inputWanted = false;
    // Until the sources change.
    bool inputFailed = false;
    bool quitting = false;
    std::array<int16_t, AUDIO_FRAME_SAMPLE_COUNT_PER_CHANNEL> frame{};
    std::thread inputThread;
};

#endif // Q_OS_ANDROID
