/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <cstdint>

struct AVFrame;

/**
 * @brief How to turn a frame upright: mirrored left to right first, then turned clockwise.
 */
struct FrameOrientation
{
    int quarterTurns = 0;
    bool mirror = false;

    bool isUpright() const
    {
        return quarterTurns == 0 && !mirror;
    }
};

// From a stream's display matrix, as the ffmpeg tool's autorotate reads it; upright for none.
FrameOrientation frameOrientation(const int32_t* displayMatrix);

// A new frame in YUV 4:2:0 that is @p frame turned as @p orientation says, or null.
AVFrame* orientFrame(const AVFrame& frame, FrameOrientation orientation);
