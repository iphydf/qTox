/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#pragma once

#include "util/interface.h"

#include <QObject>

#include <cassert>

/**
 * @brief The IAudioSink class represents an interface to devices that can play audio.
 *
 * @enum IAudioSink::Sound
 * @brief Provides the different sounds for use in the getSound function.
 * @see getSound
 *
 * @value NewMessage Returns the new message notification sound.
 * @value Test Returns the test sound.
 * @value IncomingCall Returns the incoming call sound.
 * @value OutgoingCall Returns the outgoing call sound.
 *
 * @fn QString IAudioSink::getSound(Sound s)
 * @brief Function to get the path of the requested sound.
 *
 * @param s Name of the sound to get the path of.
 * @return The path of the requested sound.
 *
 * @fn void IAudioSink::playAudioBuffer(const int16_t* data, int samples,
 *                                  unsigned channels, int sampleRate)
 * @brief adds a number of audio frames to the play buffer
 *
 * @param[in] data 16bit mono or stereo PCM data with alternating channel
 *            mapping for stereo (LRLR)
 * @param[in] samples number of samples per channel
 * @param[in] channels number of channels, currently 1 or 2 is supported
 * @param[in] sampleRate sample rate in Hertz
 *
 * @fn void IAudioSink::playMono16Sound(const Sound& sound)
 * @brief Play a 44100Hz mono 16bit PCM sound from the builtin sounds.
 *
 * @param[in] sound The sound to play
 *
 * @fn void IAudioSink::startLoop()
 * @brief starts looping the sound played with playMono16Sound(...)
 *
 * @fn void IAudioSink::stopLoop()
 * @brief stops looping the sound played with playMono16Sound(...)
 *
 */

class IAudioSink
{
public:
    enum class Sound
    {
        NewMessage,
        Test,
        IncomingCall,
        OutgoingCall,
        CallEnd
    };

    static QString getSound(Sound s)
    {
        switch (s) {
        case Sound::Test:
            return QStringLiteral(":/audio/notification.s16le.pcm");
        case Sound::NewMessage:
            return QStringLiteral(":/audio/notification.s16le.pcm");
        case Sound::IncomingCall:
            return QStringLiteral(":/audio/call_incoming.s16le.pcm");
        case Sound::OutgoingCall:
            return QStringLiteral(":/audio/call_outgoing.s16le.pcm");
        case Sound::CallEnd:
            return QStringLiteral(":/audio/call_end.s16le.pcm");
        }
        assert(false);
        return {};
    }

    IAudioSink() = default;
    virtual ~IAudioSink();
    IAudioSink(const IAudioSink&) = default;
    IAudioSink& operator=(const IAudioSink&) = default;
    IAudioSink(IAudioSink&&) = default;
    IAudioSink& operator=(IAudioSink&&) = default;

    virtual void playAudioBuffer(const int16_t* data, int samples, unsigned channels,
                                 int sampleRate) const = 0;
    virtual void playMono16Sound(const Sound& sound) = 0;
    virtual void startLoop() = 0;
    virtual void stopLoop() = 0;

    virtual explicit operator bool() const = 0;

signals:
    DECLARE_SIGNAL(finishedPlaying, void);
    DECLARE_SIGNAL(invalidated, void);
};
