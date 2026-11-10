/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QtGlobal>

#ifdef Q_OS_ANDROID

#include "audio/iaudiosource.h"

#include <QMutex>

class AndroidAudio;

/**
 * @brief Gives the frames that AndroidAudio records.
 */
class AndroidSource : public IAudioSource
{
    Q_OBJECT
public:
    explicit AndroidSource(AndroidAudio& audio);
    AndroidSource(const AndroidSource&) = delete;
    AndroidSource& operator=(const AndroidSource&) = delete;
    AndroidSource(AndroidSource&&) = delete;
    AndroidSource& operator=(AndroidSource&&) = delete;
    ~AndroidSource() override;

    explicit operator bool() const override;

    void kill();

private:
    AndroidAudio& audio;
    bool killed = false;
    mutable QMutex killLock;
};

#endif // Q_OS_ANDROID
