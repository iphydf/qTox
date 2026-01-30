/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/core/toxfile.h"

#include <QObject>
#include <QTemporaryFile>
#include <QtTest/QtTest>

class TestToxFile : public QObject
{
    Q_OBJECT
private slots:
    void testFileHandling();
    void testEquality();
};

void TestToxFile::testFileHandling()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("test data");
    tempFile.close();

    ToxFile file(1, 2, "test.txt", tempFile.fileName(), 9, ToxFile::SENDING);
    QCOMPARE(file.fileNum, 1u);
    QCOMPARE(file.friendId, 2u);
    QCOMPARE(file.fileName, QString("test.txt"));
    QCOMPARE(file.filePath, tempFile.fileName());
    QCOMPARE(file.status, ToxFile::INITIALIZING);

    QVERIFY(file.open(false)); // ReadOnly
    QCOMPARE(file.file->readAll(), QByteArray("test data"));
    file.file->close();

    file.setFilePath("new_path.txt");
    QCOMPARE(file.filePath, QString("new_path.txt"));
}

void TestToxFile::testEquality()
{
    const ToxFile f1(1, 2, "a", "p1", 10, ToxFile::SENDING);
    const ToxFile f2(1, 2, "b", "p2", 20, ToxFile::SENDING);
    const ToxFile f3(2, 2, "a", "p1", 10, ToxFile::SENDING);
    const ToxFile f4(1, 3, "a", "p1", 10, ToxFile::SENDING);
    const ToxFile f5(1, 2, "a", "p1", 10, ToxFile::RECEIVING);

    QCOMPARE(f1, f2); // Equality only checks fileNum, friendId, and direction
    QVERIFY(f1 != f3);
    QVERIFY(f1 != f4);
    QVERIFY(f1 != f5);
}

QTEST_GUILESS_MAIN(TestToxFile)
#include "toxfile_test.moc"
