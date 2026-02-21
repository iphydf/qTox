/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2025-2026 The TokTok team.
 */

#include "src/platform/stacktrace.h"

#include <QList>
#include <QString>
#include <QtTest/QtTest>

class TestStacktrace : public QObject
{
    Q_OBJECT

private slots:
    void testEphemeralStacktrace();
};

void TestStacktrace::testEphemeralStacktrace()
{
    QList<QString> trace;
    Stacktrace::process([&trace](const Stacktrace::Frame& frame) {
        qDebug() << frame;
        trace.push_back(QString::fromLatin1(frame.function));
    });
}

QTEST_GUILESS_MAIN(TestStacktrace)
#include "stacktrace_test.moc"
