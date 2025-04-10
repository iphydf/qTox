/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2017-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "src/core/chatid.h"

#include "src/core/conferenceid.h"
#include "src/core/toxpk.h"

#include <QByteArray>
#include <QString>
#include <QtTest/QtTest>

const uint8_t testPkArray[32] = {
    0xC7, 0x71, 0x9C, 0x68, 0x08, 0xC1, 0x4B, 0x77, 0x34, 0x80, 0x04, 0x95, 0x6D, 0x1D, 0x98, 0x04,
    0x6C, 0xE0, 0x9A, 0x34, 0x37, 0x0E, 0x76, 0x08, 0x15, 0x0E, 0xAD, 0x74, 0xC3, 0x81, 0x5D, 0x30,
};

const QString testStr =
    QStringLiteral("C7719C6808C14B77348004956D1D98046CE09A34370E7608150EAD74C3815D30");
const QByteArray testPk = QByteArray::fromHex(testStr.toLatin1());

const QString echoStr =
    QStringLiteral("76518406F6A9F2217E8DC487CC783C25CC16A15EB36FF32E335A235342C48A39");
const QByteArray echoPk = QByteArray::fromHex(echoStr.toLatin1());

class TestChatId : public QObject
{
    Q_OBJECT
private slots:
    void toStringTest();
    void equalTest();
    void clearTest();
    void copyTest();
    void dataTest();
    void sizeTest();
    void hashableTest();
};

void TestChatId::toStringTest()
{
    QCOMPARE(testPk.size(), ToxPk::size);
    ToxPk pk(testPk);
    QVERIFY(testStr == pk.toString());
}

void TestChatId::equalTest()
{
    ToxPk pk1(testPk);
    ToxPk pk2(testPk);
    ToxPk pk3(echoPk);
    QVERIFY(pk1 == pk2);
    QVERIFY(pk1 != pk3);
    QVERIFY(!(pk1 != pk2));
}

void TestChatId::clearTest()
{
    ToxPk empty;
    ToxPk pk(testPk);
    QVERIFY(empty.isEmpty());
    QVERIFY(!pk.isEmpty());
}

void TestChatId::copyTest()
{
    ToxPk src(testPk);
    ToxPk copy = src;
    QVERIFY(copy == src);
}

void TestChatId::dataTest()
{
    ToxPk pk(testPk);
    QVERIFY(testPk == pk.getByteArray());
    for (int i = 0; i < pk.getSize(); i++) {
        QVERIFY(testPkArray[i] == pk.getData()[i]);
    }
}

void TestChatId::sizeTest()
{
    ToxPk pk;
    ConferenceId id;
    QVERIFY(pk.getSize() == ToxPk::size);
    QVERIFY(id.getSize() == ConferenceId::size);
}

void TestChatId::hashableTest()
{
    ToxPk pk1{testPkArray};
    ToxPk pk2{testPk};
    QVERIFY(qHash(pk1) == qHash(pk2));
    ToxPk pk3{echoPk};
    QVERIFY(qHash(pk1) != qHash(pk3));
}

QTEST_GUILESS_MAIN(TestChatId)
#include "chatid_test.moc"
