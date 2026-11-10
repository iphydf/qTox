/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */


#pragma once

#include "alsink.h"
#include "alsource.h"
#include "voicegate.h"

#include "audio/iaudiocontrol.h"

#include <QMutex>
#include <QObject>
#include <QTimer>

#include <AL/al.h>
#include <AL/alc.h>
#include <memory>
#include <unordered_set>

#ifndef ALC_ALL_DEVICES_SPECIFIER
// compatibility with older versions of OpenAL
#include <AL/alext.h>
#endif

class IAudioSettings;

class OpenAL : public IAudioControl
{
    Q_OBJECT

public:
    explicit OpenAL(IAudioSettings& _settings);
    ~OpenAL() override;

    qreal maxOutputVolume() const override
    {
        return 1;
    }
    qreal minOutputVolume() const override
    {
        return 0;
    }
    void setOutputVolume(qreal volume) override;

    qreal minInputGain() const override;
    qreal maxInputGain() const override;

    qreal inputGain() const override;
    void setInputGain(qreal dB) override;

    qreal minInputThreshold() const override;
    qreal maxInputThreshold() const override;

    qreal getInputThreshold() const override;
    void setInputThreshold(qreal normalizedThreshold) override;

    void reinitInput(const QString& inDevDesc) override;
    bool reinitOutput(const QString& outDevDesc) override;

    bool isOutputReady() const override;

    QStringList outDeviceNames() override;
    QStringList inDeviceNames() override;

    std::unique_ptr<IAudioSink> makeSink() override;
    void destroySink(AlSink& sink);

    std::unique_ptr<IAudioSource> makeSource() override;
    void destroySource(AlSource& source);

    void startLoop(uint sourceId);
    void stopLoop(uint sourceId);
    void playMono16Sound(AlSink& sink, const IAudioSink::Sound& sound);

    void playAudioBuffer(uint sourceId, const int16_t* data, int samples, unsigned channels,
                         int sampleRate);

protected:
    static void checkAlError() noexcept;
    static void checkAlcError(ALCdevice* device) noexcept;

    virtual void cleanupInput();
    virtual void cleanupOutput();

    bool autoInitInput();
    bool autoInitOutput();

    bool initInput(const QString& deviceName, uint32_t channels);

    void doAudio();

    virtual void doInput();
    virtual void doOutput();
    virtual void captureSamples(ALCdevice* device, int16_t* buffer, ALCsizei samples);

private:
    virtual bool initInput(const QString& deviceName);
    virtual bool initOutput(const QString& deviceName);

    static void cleanupBuffers(uint sourceId);
    void cleanupSound();

protected:
    IAudioSettings& settings;
    QThread* audioThread;
    mutable QRecursiveMutex audioLock;
    QString inDev{};
    QString outDev{};

    ALCdevice* alInDev = nullptr;
    QTimer captureTimer;
    QTimer cleanupTimer;

    ALCdevice* alOutDev = nullptr;
    ALCcontext* alOutContext = nullptr;

    bool outputInitialized = false;

    // Qt containers need copy operators, so use stdlib containers
    std::unordered_set<AlSink*> sinks;
    std::unordered_set<AlSink*> soundSinks;
    std::unordered_set<AlSource*> sources;

    int inputChannels = 0;
    VoiceGate gate;
    int16_t* inputBuffer = nullptr;
};
