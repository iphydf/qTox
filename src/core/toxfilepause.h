/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2018-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

class ToxFilePause
{
public:
    void localPause()
    {
        localPauseState = true;
    }

    void localResume()
    {
        localPauseState = false;
    }

    void localPauseToggle()
    {
        localPauseState = !localPauseState;
    }

    void remotePause()
    {
        remotePauseState = true;
    }

    void remoteResume()
    {
        remotePauseState = false;
    }

    void remotePauseToggle()
    {
        remotePauseState = !remotePauseState;
    }

    bool localPaused() const
    {
        return localPauseState;
    }

    bool remotePaused() const
    {
        return remotePauseState;
    }

    bool paused() const
    {
        return localPauseState || remotePauseState;
    }

private:
    bool localPauseState = false;
    bool remotePauseState = false;
};
