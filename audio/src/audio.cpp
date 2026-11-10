/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "audio/audio.h"

#include "audio/iaudiosettings.h"
#include "backend/android/androidaudio.h"
#include "backend/openal.h"

#include <memory>

/**
 * @brief Select the audio backend
 * @param settings Audio settings to use
 * @return Audio backend selection based on settings
 */
std::unique_ptr<IAudioControl> Audio::makeAudio(IAudioSettings& settings)
{
    Q_INIT_RESOURCE(audio_res);
#ifdef Q_OS_ANDROID
    // OpenAL plays calls as media, without the earpiece or echo cancellation.
    if (auto android = AndroidAudio::make(settings)) {
        return android;
    }
#endif
    return std::unique_ptr<IAudioControl>(new OpenAL(settings));
}
