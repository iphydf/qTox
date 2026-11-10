/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "framerotation.h"

extern "C"
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <libavutil/display.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#pragma GCC diagnostic pop
}

#include <cmath>
#include <memory>

namespace {
struct FrameDeleter
{
    void operator()(AVFrame* frame) const
    {
        av_frame_free(&frame);
    }
};

using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

FramePtr newFrame(int width, int height)
{
    FramePtr frame{av_frame_alloc()};
    if (frame == nullptr) {
        return {};
    }
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width;
    frame->height = height;
    // Without padding, as toxav takes frames: VideoFrame would otherwise copy each one.
    if (av_frame_get_buffer(frame.get(), 1) < 0) {
        return {};
    }
    return frame;
}

FramePtr toYuv420(const AVFrame& frame)
{
    FramePtr yuv = newFrame(frame.width, frame.height);
    SwsContext* const sws =
        sws_getContext(frame.width, frame.height, static_cast<AVPixelFormat>(frame.format), frame.width,
                       frame.height, AV_PIX_FMT_YUV420P, SWS_POINT, nullptr, nullptr, nullptr);
    if (yuv == nullptr || sws == nullptr) {
        sws_freeContext(sws);
        return {};
    }
    sws_scale(sws, frame.data, frame.linesize, 0, frame.height, yuv->data, yuv->linesize);
    sws_freeContext(sws);
    return yuv;
}
} // namespace

FrameOrientation frameOrientation(const int32_t* displayMatrix)
{
    if (displayMatrix == nullptr) {
        return {};
    }
    // Clockwise, between 0 and 360, as the ffmpeg tool's get_rotation() says.
    double theta = -std::round(av_display_rotation_get(displayMatrix));
    theta -= 360 * std::floor(theta / 360 + 0.9 / 360);

    // The transpose, hflip and vflip filters that the ffmpeg tool inserts for these angles.
    if (std::fabs(theta - 90) < 1.0) {
        // cclock_flip, or clock.
        return displayMatrix[3] > 0 ? FrameOrientation{3, true} : FrameOrientation{1, false};
    }
    if (std::fabs(theta - 180) < 1.0) {
        // Both flips make a half turn.
        const bool hflip = displayMatrix[0] < 0;
        const bool vflip = displayMatrix[4] < 0;
        return {vflip ? 2 : 0, hflip != vflip};
    }
    if (std::fabs(theta - 270) < 1.0) {
        // clock_flip, or cclock.
        return displayMatrix[3] < 0 ? FrameOrientation{1, true} : FrameOrientation{3, false};
    }
    // vflip, if anything: other angles aren't a camera's.
    return displayMatrix[4] < 0 ? FrameOrientation{2, true} : FrameOrientation{};
}

AVFrame* orientFrame(const AVFrame& frame, FrameOrientation orientation)
{
    FramePtr converted;
    const AVFrame* source = &frame;
    if (frame.format != AV_PIX_FMT_YUV420P) {
        converted = toYuv420(frame);
        if (converted == nullptr) {
            return nullptr;
        }
        source = converted.get();
    }

    const int turns = ((orientation.quarterTurns % 4) + 4) % 4;
    const bool sideways = turns % 2 == 1;
    FramePtr result =
        sideways ? newFrame(frame.height, frame.width) : newFrame(frame.width, frame.height);
    if (result == nullptr) {
        return nullptr;
    }

    for (int plane = 0; plane < 3; ++plane) {
        // The chroma planes have half the luma plane's size, rounded up.
        const int shift = plane == 0 ? 0 : 1;
        const int width = AV_CEIL_RSHIFT(source->width, shift);
        const int height = AV_CEIL_RSHIFT(source->height, shift);
        const int outWidth = sideways ? height : width;
        const int outHeight = sideways ? width : height;
        const uint8_t* const in = source->data[plane];
        uint8_t* const out = result->data[plane];
        for (int y = 0; y < outHeight; ++y) {
            for (int x = 0; x < outWidth; ++x) {
                // Where the pixel is in the mirrored frame, then in the frame.
                int mx = x;
                int my = y;
                switch (turns) {
                case 1:
                    mx = y;
                    my = height - 1 - x;
                    break;
                case 2:
                    mx = width - 1 - x;
                    my = height - 1 - y;
                    break;
                case 3:
                    mx = width - 1 - y;
                    my = x;
                    break;
                default:
                    break;
                }
                const int sx = orientation.mirror ? width - 1 - mx : mx;
                out[y * result->linesize[plane] + x] = in[my * source->linesize[plane] + sx];
            }
        }
    }
    result->pts = frame.pts;
    return result.release();
}
