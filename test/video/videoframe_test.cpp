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

QTEST_GUILESS_MAIN(TestVideoFrame)
#include "videoframe_test.moc"
