/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/video/framerotation.h"

#include <QTest>

extern "C"
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <libavutil/display.h>
#include <libavutil/frame.h>
#pragma GCC diagnostic pop
}

#include <array>
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

// A 4x2 frame whose luma pixels are 0 to 7, row by row.
FramePtr numberedFrame()
{
    FramePtr frame{av_frame_alloc()};
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = 4;
    frame->height = 2;
    av_frame_get_buffer(frame.get(), 0);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
            frame->data[0][y * frame->linesize[0] + x] = static_cast<uint8_t>(y * 4 + x);
        }
    }
    // No colour.
    for (int plane = 1; plane < 3; ++plane) {
        frame->data[plane][0] = 128;
        frame->data[plane][1] = 128;
    }
    return frame;
}

QVector<int> luma(const AVFrame& frame)
{
    QVector<int> pixels;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            pixels.append(frame.data[0][y * frame.linesize[0] + x]);
        }
    }
    return pixels;
}
} // namespace

class TestFrameRotation : public QObject
{
    Q_OBJECT
private slots:
    void cameraOrientations();
    void quarterTurn();
    void mirroredQuarterTurn();
};

void TestFrameRotation::cameraOrientations()
{
    QVERIFY(frameOrientation(nullptr).isUpright());

    // As FFmpeg's android_camera describes a back camera whose sensor is turned 90 degrees, and a
    // front camera whose sensor is turned 270.
    std::array<int32_t, 9> matrix{};
    av_display_rotation_set(matrix.data(), 90);
    FrameOrientation back = frameOrientation(matrix.data());
    QCOMPARE(back.quarterTurns, 1);
    QVERIFY(!back.mirror);

    av_display_rotation_set(matrix.data(), 270);
    av_display_matrix_flip(matrix.data(), 1, 0);
    FrameOrientation front = frameOrientation(matrix.data());
    QCOMPARE(front.quarterTurns, 1);
    QVERIFY(front.mirror);
}

void TestFrameRotation::quarterTurn()
{
    const FramePtr frame = numberedFrame();
    const FramePtr turned{orientFrame(*frame, {1, false})};
    QVERIFY(turned != nullptr);
    QCOMPARE(turned->width, 2);
    QCOMPARE(turned->height, 4);
    QCOMPARE(luma(*turned), (QVector<int>{4, 0, 5, 1, 6, 2, 7, 3}));
}

void TestFrameRotation::mirroredQuarterTurn()
{
    const FramePtr frame = numberedFrame();
    const FramePtr turned{orientFrame(*frame, {1, true})};
    QVERIFY(turned != nullptr);
    QCOMPARE(luma(*turned), (QVector<int>{7, 3, 6, 2, 5, 1, 4, 0}));
}

QTEST_GUILESS_MAIN(TestFrameRotation)
#include "framerotation_test.moc"
