/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "audio/src/backend/android/androidsource.h"

#ifdef Q_OS_ANDROID

#include "audio/src/backend/android/androidaudio.h"

#include <QMutexLocker>

AndroidSource::AndroidSource(AndroidAudio& audio_)
    : audio{audio_}
{
}

AndroidSource::~AndroidSource()
{
    audio.destroySource(*this);
}

AndroidSource::operator bool() const
{
    const QMutexLocker<QMutex> locker{&killLock};
    return !killed;
}

void AndroidSource::kill()
{
    {
        const QMutexLocker<QMutex> locker{&killLock};
        killed = true;
    }
    emit invalidated();
}

#endif // Q_OS_ANDROID
