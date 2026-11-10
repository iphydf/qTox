/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/video/videoframe.h"

#include <QObject>
#include <QtTest/QtTest>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <QtConcurrent/QtConcurrent>

class TestVideoFrame : public QObject
{
    Q_OBJECT

private slots:
    void testToQImageEmptySize();
    void testToQImageEmptySource();
    void testToToxYUVFrameEmptySource();
    void testConcurrency();
    void testToQImageOutlivesFrame();
    void testToToxYUVFramePaddedChroma();
};

void TestVideoFrame::testConcurrency()
{
    for (int j = 0; j < 5; ++j) {
        AVFrame* frame = av_frame_alloc();
        frame->width = 100;
        frame->height = 100;
        frame->format = AV_PIX_FMT_YUV420P;
        av_image_alloc(frame->data, frame->linesize, 100, 100, AV_PIX_FMT_YUV420P, 1);

        auto videoFrame = VideoFrame::fromAVFrameUntracked(1, frame, true);

        QList<QFuture<void>> futures;
        for (int i = 0; i < 200; ++i) {
            futures.append(QtConcurrent::run([videoFrame]() {
                for (int k = 0; k < 100; ++k) {
                    QSize size = (k % 2 == 0) ? QSize(50, 50) : QSize(80, 80);
                    const QImage img = videoFrame->toQImage(size);
                    Q_UNUSED(img);
                }
            }));
        }

        for (auto& future : futures) {
            future.waitForFinished();
        }
    }
}

void TestVideoFrame::testToQImageEmptySize()
{
    AVFrame* frame = av_frame_alloc();
    frame->width = 100;
    frame->height = 100;
    frame->format = AV_PIX_FMT_YUV420P;
    av_image_alloc(frame->data, frame->linesize, 100, 100, AV_PIX_FMT_YUV420P, 1);

    // VideoFrame takes ownership and will free the frame if we pass true
    auto videoFrame = VideoFrame::fromAVFrameUntracked(1, frame, true);

    // This should not crash. Since QSize(0,0) is empty, it should fall back to source size (100x100)
    const QImage img = videoFrame->toQImage(QSize(0, 0));
    QCOMPARE(img.size(), QSize(100, 100));
}

void TestVideoFrame::testToQImageEmptySource()
{
    AVFrame* frame = av_frame_alloc();
    frame->width = 0;
    frame->height = 0;
    frame->format = AV_PIX_FMT_YUV420P;

    auto videoFrame = VideoFrame::fromAVFrameUntracked(1, frame, true);

    // Both requested size and source size are empty, should return null image
    const QImage img = videoFrame->toQImage(QSize(0, 0));
    QVERIFY(img.isNull());
}

void TestVideoFrame::testToToxYUVFrameEmptySource()
{
    AVFrame* frame = av_frame_alloc();
    frame->width = 0;
    frame->height = 0;
    frame->format = AV_PIX_FMT_YUV420P;

    auto videoFrame = VideoFrame::fromAVFrameUntracked(1, frame, true);

    auto [toxFrame, locker] = videoFrame->toToxYUVFrame();
    QVERIFY(!toxFrame.isValid());
}

void TestVideoFrame::testToQImageOutlivesFrame()
{
    AVFrame* frame = av_frame_alloc();
    frame->width = 16;
    frame->height = 16;
    frame->format = AV_PIX_FMT_YUV420P;
    av_image_alloc(frame->data, frame->linesize, 16, 16, AV_PIX_FMT_YUV420P, 1);
    // White.
    memset(frame->data[0], 255, static_cast<size_t>(frame->linesize[0]) * 16);
    memset(frame->data[1], 128, static_cast<size_t>(frame->linesize[1]) * 8);
    memset(frame->data[2], 128, static_cast<size_t>(frame->linesize[2]) * 8);
    auto videoFrame = VideoFrame::fromAVFrameUntracked(1, frame, true);

    const QImage img = videoFrame->toQImage();
    // As closing the camera does to its frames.
    videoFrame->releaseFrame();
    // Read here, not in Qt, which sanitisers don't check.
    QCOMPARE(img.constScanLine(8)[8 * 3], 255);
}

void TestVideoFrame::testToToxYUVFramePaddedChroma()
{
    // Packed luma rows and padded chroma rows, as av_frame_get_buffer() makes 480 pixels wide.
    constexpr int width = 480;
    constexpr int height = 4;
    constexpr int chromaStride = 256;
    AVFrame* frame = av_frame_alloc();
    frame->width = width;
    frame->height = height;
    frame->format = AV_PIX_FMT_YUV420P;
    // One allocation, which VideoFrame frees through the first plane.
    auto* const buffer =
        static_cast<uint8_t*>(av_mallocz(width * height + 2 * chromaStride * (height / 2)));
    frame->data[0] = buffer;
    frame->data[1] = buffer + width * height;
    frame->data[2] = frame->data[1] + chromaStride * (height / 2);
    frame->linesize[0] = width;
    frame->linesize[1] = chromaStride;
    frame->linesize[2] = chromaStride;
    // Each chroma row's pixels say which row they are, from 1.
    for (int row = 0; row < height / 2; ++row) {
        memset(frame->data[1] + row * chromaStride, row + 1, width / 2);
    }
    auto videoFrame = VideoFrame::fromAVFrameUntracked(1, frame, true);

    auto [toxFrame, locker] = videoFrame->toToxYUVFrame();
    QVERIFY(toxFrame.isValid());
    // toxav reads the rows without padding.
    QCOMPARE(toxFrame.u[width / 2], 2);
}

QTEST_GUILESS_MAIN(TestVideoFrame)
#include "videoframe_test.moc"
