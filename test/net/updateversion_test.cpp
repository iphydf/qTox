/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/net/updateversion.h"

#include <QtTest/QtTest>

class TestUpdateVersion : public QObject
{
    Q_OBJECT
private slots:
    void testTagToVersion();
    void testIsUpdateAvailable();
    void testIsVersionStable();
};

void TestUpdateVersion::testTagToVersion()
{
    QCOMPARE(tagToVersion("v1.2.3"), (std::optional<Version>{{1, 2, 3}}));
    QCOMPARE(tagToVersion("v0.0.0"), (std::optional<Version>{{0, 0, 0}}));
    QCOMPARE(tagToVersion("v10.20.30"), (std::optional<Version>{{10, 20, 30}}));

    QCOMPARE(tagToVersion("1.2.3"), std::nullopt);
    QCOMPARE(tagToVersion("v1.2"), std::nullopt);
    QCOMPARE(tagToVersion("v1.2.3-rc1"), std::nullopt);
    QCOMPARE(tagToVersion("v1.2.3.4"), std::nullopt);
    QCOMPARE(tagToVersion("va.b.c"), std::nullopt);
}

void TestUpdateVersion::testIsUpdateAvailable()
{
    const Version v123{1, 2, 3};
    const Version v124{1, 2, 4};
    const Version v130{1, 3, 0};
    const Version v200{2, 0, 0};

    QVERIFY(isUpdateAvailable(v123, v124));
    QVERIFY(isUpdateAvailable(v123, v130));
    QVERIFY(isUpdateAvailable(v123, v200));

    QVERIFY(!isUpdateAvailable(v124, v123));
    QVERIFY(!isUpdateAvailable(v130, v123));
    QVERIFY(!isUpdateAvailable(v200, v123));

    QVERIFY(!isUpdateAvailable(v123, v123));
}

void TestUpdateVersion::testIsVersionStable()
{
    QVERIFY(isVersionStable("v1.2.3"));
    QVERIFY(isVersionStable("v0.0.0"));
    QVERIFY(isVersionStable("v10.20.30"));

    QVERIFY(!isVersionStable("1.2.3"));
    QVERIFY(!isVersionStable("v1.2"));
    QVERIFY(!isVersionStable("v1.2.3-rc1"));
    QVERIFY(!isVersionStable("v1.2.3-4-g1234567"));
}

QTEST_MAIN(TestUpdateVersion)
#include "updateversion_test.moc"
