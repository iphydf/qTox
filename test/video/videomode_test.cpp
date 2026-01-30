/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/video/videomode.h"

#include <QObject>
#include <QtTest/QtTest>

class TestVideoMode : public QObject
{
    Q_OBJECT

private slots:
    void testEquality();
    void testNorm();
    void testTolerance();
    void testIsUnspecified();
    void testToRect();
};

void TestVideoMode::testEquality()
{
    const VideoMode m1(640, 480, 0, 0, 30.0f);
    const VideoMode m2(640, 480, 0, 0, 30.0f);
    const VideoMode m3(800, 600, 0, 0, 30.0f);
    const VideoMode m4(640, 480, 10, 0, 30.0f);
    const VideoMode m5(640, 480, 0, 0, 60.0f);

    QCOMPARE(m1, m2);
    QVERIFY(!(m1 == m3));
    QVERIFY(!(m1 == m4));
    QVERIFY(!(m1 == m5));
}

void TestVideoMode::testNorm()
{
    const VideoMode m1(640, 480);
    const VideoMode m2(800, 600);

    // norm = abs(640-800) + abs(480-600) = 160 + 120 = 280
    QCOMPARE(m1.norm(m2), 280u);
    QCOMPARE(m2.norm(m1), 280u);
    QCOMPARE(m1.norm(m1), 0u);
}

void TestVideoMode::testTolerance()
{
    // tolerance = std::max((width + height) / 10, 300)

    const VideoMode m1(640, 480);
    // (640 + 480) / 10 = 112. max(112, 300) = 300
    QCOMPARE(m1.tolerance(), 300u);

    const VideoMode m2(1920, 1080);
    // (1920 + 1080) / 10 = 300. max(300, 300) = 300
    QCOMPARE(m2.tolerance(), 300u);

    const VideoMode m3(3840, 2160);
    // (3840 + 2160) / 10 = 600. max(600, 300) = 600
    QCOMPARE(m3.tolerance(), 600u);
}

void TestVideoMode::testIsUnspecified()
{
    QVERIFY(VideoMode(0, 480, 0, 0, 30.0f).isUnspecified());
    QVERIFY(VideoMode(640, 0, 0, 0, 30.0f).isUnspecified());
    QVERIFY(VideoMode(640, 480, 0, 0, 0.0f).isUnspecified());
    QVERIFY(!VideoMode(640, 480, 0, 0, 30.0f).isUnspecified());
}

void TestVideoMode::testToRect()
{
    const VideoMode m(640, 480, 10, 20);
    const QRect r = m.toRect();
    QCOMPARE(r.width(), 640);
    QCOMPARE(r.height(), 480);
    QCOMPARE(r.x(), 10);
    QCOMPARE(r.y(), 20);

    const VideoMode m2(r);
    QCOMPARE(m2.width, 640);
    QCOMPARE(m2.height, 480);
    QCOMPARE(m2.x, 10);
    QCOMPARE(m2.y, 20);
}

QTEST_GUILESS_MAIN(TestVideoMode)
#include "videomode_test.moc"
