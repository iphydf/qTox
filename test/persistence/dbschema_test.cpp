/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "dbutility/dbutility.h"
#include "src/core/toxfile.h"
#include "src/persistence/db/rawdatabase.h"
#include "src/persistence/db/upgrades/dbupgrader.h"
#include "src/widget/tool/imessageboxmanager.h"

#include <QString>
#include <QTemporaryFile>
#include <QtTest/QtTest>

#include <memory>

namespace {
bool insertFileId(RawDatabase& db, int row, bool valid)
{
    QByteArray validResumeId(32, 1);
    QByteArray invalidResumeId;

    QByteArray resumeId;
    if (valid) {
        resumeId = validResumeId;
    } else {
        resumeId = invalidResumeId;
    }

    std::vector<RawDatabase::Query> upgradeQueries;
    upgradeQueries.emplace_back( //
        QStringLiteral(          //
            "INSERT INTO file_transfers "
            "    (id, message_type, sender_alias, "
            "    file_restart_id, file_name, file_path, "
            "    file_hash, file_size, direction, file_state) "
            "VALUES ( "
            "    %1, "
            "    'F', "
            "    1, "
            "    ?, "
            "    %2, "
            "    %3, "
            "    %4, "
            "    1, "
            "    1, "
            "    %5 "
            ");")
            .arg(row)
            .arg("\"fooName\"")
            .arg("\"foo/path\"")
            .arg("\"fooHash\"")
            .arg(ToxFile::CANCELED),
        QVector<QByteArray>{resumeId});
    return db.execNow(std::move(upgradeQueries));
}

class MockMessageBoxManager : public IMessageBoxManager
{
public:
    ~MockMessageBoxManager() override = default;
    void showInfo(const QString& title, const QString& msg) override
    {
        std::ignore = title;
        std::ignore = msg;
    }
    void showWarning(const QString& title, const QString& msg) override
    {
        std::ignore = title;
        std::ignore = msg;
    }
    void showError(const QString& title, const QString& msg) override
    {
        std::ignore = title;
        std::ignore = msg;
        ++errorsShown;
    }
    bool askQuestion(const QString& title, const QString& msg, bool defaultAns = false,
                     bool warning = true, bool yesno = true) override
    {
        std::ignore = title;
        std::ignore = msg;
        std::ignore = warning;
        std::ignore = yesno;
        return defaultAns;
    }
    bool askQuestion(const QString& title, const QString& msg, const QString& button1,
                     const QString& button2, bool defaultAns = false, bool warning = true) override
    {
        std::ignore = title;
        std::ignore = msg;
        std::ignore = button1;
        std::ignore = button2;
        std::ignore = warning;
        return defaultAns;
    }
    void confirmExecutableOpen(const QFileInfo& file) override
    {
        std::ignore = file;
    }
    int getErrorsShown() const
    {
        return errorsShown;
    }

private:
    int errorsShown = 0;
};

} // namespace

class TestDbSchema : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void testCreation();
    void testIsNewDb();
    void testNewerDb();
    void test0to1();
    void test1to2();
    void test2to3();
    void test3to4();
    void test4to5();
    void test5to6();
    void test6to7();
    // test7to8 omitted, version only upgrade, versions are not verified in this
    // test8to9 omitted, data corruption correction upgrade with no schema change
    void test9to10();
    // test10to11 handled in dbTo11_test
    // test suite

private:
    std::unique_ptr<QTemporaryFile> testDatabaseFile;
};

void TestDbSchema::init()
{
    testDatabaseFile = std::make_unique<QTemporaryFile>();
    // fileName is only defined once the file is opened. Since RawDatabase
    // will be opening the file itself not using QFile, open and close it now.
    QVERIFY(testDatabaseFile->open());
    testDatabaseFile->close();
}

void TestDbSchema::cleanup()
{
    testDatabaseFile.reset();
}

void TestDbSchema::testCreation()
{
    QVector<RawDatabase::Query> queries;
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    QVERIFY(DbUpgrader::createCurrentSchema(*db));
    DbUtility::verifyDb(db, DbUtility::schema11);
}

void TestDbSchema::testIsNewDb()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    bool success = false;
    bool newDb = DbUpgrader::isNewDb(db, success);
    QVERIFY(success);
    QVERIFY(newDb == true);
    db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema0);
    newDb = DbUpgrader::isNewDb(db, success);
    QVERIFY(success);
    QVERIFY(newDb == false);
}

void TestDbSchema::testNewerDb()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema0);
    int futureSchemaVersion = 1000000;
    db->execNow(
        RawDatabase::Query(QStringLiteral("PRAGMA user_version = %1").arg(futureSchemaVersion)));
    MockMessageBoxManager messageBoxManager;
    bool success = DbUpgrader::dbSchemaUpgrade(db, messageBoxManager);
    QVERIFY(success == false);
    QVERIFY(messageBoxManager.getErrorsShown() == 1);
}

void TestDbSchema::test0to1()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema0);
    QVERIFY(DbUpgrader::dbSchema0to1(*db));
    DbUtility::verifyDb(db, DbUtility::schema1);
}

void TestDbSchema::test1to2()
{
    /*
    Due to a long standing bug, faux offline message have been able to become stuck
    going back years. Because of recent fixes to history loading, faux offline
    messages will correctly all be sent on connection, but this causes an issue of
    long stuck messages suddenly being delivered to a friend, out of context,
    creating a confusing interaction. To work around this, this upgrade moves any
    faux offline messages in a chat that are older than the last successfully
    delivered message, indicating they were stuck, to a new table,
    `broken_messages`, preventing them from ever being sent in the future.

    https://github.com/qTox/qTox/issues/5776
    */

    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema1);

    const QString myPk = "AC18841E56CCDEE16E93E10E6AB2765BE54277D67F1372921B5B418A6B330D3D";
    const QString friend1Pk = "FE34BC6D87B66E958C57BBF205F9B79B62BE0AB8A4EFC1F1BB9EC4D0D8FB0663";
    const QString friend2Pk = "2A1CBCE227549459C0C20F199DB86AD9BCC436D35BAA1825FFD4B9CA3290D200";

    std::vector<RawDatabase::Query> queries;
    queries.emplace_back(
        QStringLiteral("INSERT INTO peers (id, public_key) VALUES (%1, '%2')").arg(0).arg(myPk));
    queries.emplace_back(
        QStringLiteral("INSERT INTO peers (id, public_key) VALUES (%1, '%2')").arg(1).arg(friend1Pk));
    queries.emplace_back(
        QStringLiteral("INSERT INTO peers (id, public_key) VALUES (%1, '%2')").arg(2).arg(friend2Pk));

    // friend 1
    // first message in chat is pending - but the second is delivered. This message is "broken"
    queries.emplace_back(QStringLiteral("INSERT INTO history (id, timestamp, chat_id, message, "
                                        "sender_alias) VALUES (1, 1, 1, ?, 0)"),
                         QVector<QByteArray>{"first message in chat, pending and stuck"});
    queries.emplace_back(QStringLiteral("INSERT INTO faux_offline_pending (id) VALUES ("
                                        "    last_insert_rowid()"
                                        ");"));
    // second message is delivered, causing the first to be considered broken
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (2, 2, 1, ?, 0)",
                         QVector<QByteArray>{"second message in chat, delivered"});

    // third message is pending - this is a normal pending message. It should be untouched.
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (3, 3, 1, ?, 0)",
                         QVector<QByteArray>{"third message in chat, pending"});
    queries.emplace_back("INSERT INTO faux_offline_pending (id) VALUES ("
                         "    last_insert_rowid()"
                         ");");

    // friend 2
    // first message is delivered.
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (4, 4, 2, ?, 2)",
                         QVector<QByteArray>{"first message by friend in chat, delivered"});

    // second message is also delivered.
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (5, 5, 2, ?, 0)",
                         QVector<QByteArray>{"first message by us in chat, delivered"});

    // third message is pending, but not broken since there are no delivered messages after it.
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (6, 6, 2, ?, 0)",
                         QVector<QByteArray>{"last message in chat, by us, pending"});
    queries.emplace_back("INSERT INTO faux_offline_pending (id) VALUES ("
                         "    last_insert_rowid()"
                         ");");

    QVERIFY(db->execNow(std::move(queries)));
    QVERIFY(DbUpgrader::dbSchema1to2(*db));
    DbUtility::verifyDb(db, DbUtility::schema2);

    long brokenCount = -1;
    RawDatabase::Query brokenCountQuery = {"SELECT COUNT(*) FROM broken_messages;",
                                           [&](const QVector<QVariant>& row) {
                                               brokenCount = row[0].toLongLong();
                                           }};
    QVERIFY(db->execNow(std::move(brokenCountQuery)));
    QVERIFY(brokenCount == 1); // only friend 1's first message is "broken"

    int fauxOfflineCount = -1;
    RawDatabase::Query fauxOfflineCountQuery = {"SELECT COUNT(*) FROM faux_offline_pending;",
                                                [&](const QVector<QVariant>& row) {
                                                    fauxOfflineCount = row[0].toLongLong();
                                                }};
    QVERIFY(db->execNow(std::move(fauxOfflineCountQuery)));
    // both friend 1's third message and friend 2's third message should still be pending.
    // The broken message should no longer be pending.
    QVERIFY(fauxOfflineCount == 2);

    int totalHistoryCount = -1;
    RawDatabase::Query totalHistoryCountQuery = {"SELECT COUNT(*) FROM history;",
                                                 [&](const QVector<QVariant>& row) {
                                                     totalHistoryCount = row[0].toLongLong();
                                                 }};
    QVERIFY(db->execNow(std::move(totalHistoryCountQuery)));
    QVERIFY(totalHistoryCount == 6); // all messages should still be in history.
}

void TestDbSchema::test2to3()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema2);

    // since we don't enforce foreign key constraints in the db, we can stick in IDs to other tables
    // to avoid generating proper entries for peers and aliases tables, since they aren't actually
    // relevant for the test.

    std::vector<RawDatabase::Query> queries;
    // pending message, should be moved out
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (1, 1, 0, ?, 0)",
                         QVector<QByteArray>{"/me "});
    queries.emplace_back("INSERT INTO faux_offline_pending (id) VALUES ("
                         "    last_insert_rowid()"
                         ");");

    // non pending message with the content "/me ". Maybe it was sent by a friend using a different client.
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (2, 2, 0, ?, 2)",
                         QVector<QByteArray>{"/me "});

    // non pending message sent by us
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (3, 3, 0, ?, 1)",
                         QVector<QByteArray>{"a normal message"});

    // pending normal message sent by us
    queries.emplace_back("INSERT INTO history (id, timestamp, chat_id, message, "
                         "sender_alias) VALUES (4, 3, 0, ?, 1)",
                         QVector<QByteArray>{"a normal faux offline message"});
    queries.emplace_back("INSERT INTO faux_offline_pending (id) VALUES ("
                         "    last_insert_rowid()"
                         ");");
    QVERIFY(db->execNow(std::move(queries)));
    QVERIFY(DbUpgrader::dbSchema2to3(*db));

    long brokenCount = -1;
    RawDatabase::Query brokenCountQuery = {"SELECT COUNT(*) FROM broken_messages;",
                                           [&](const QVector<QVariant>& row) {
                                               brokenCount = row[0].toLongLong();
                                           }};
    QVERIFY(db->execNow(std::move(brokenCountQuery)));
    QVERIFY(brokenCount == 1);

    int fauxOfflineCount = -1;
    RawDatabase::Query fauxOfflineCountQuery = {"SELECT COUNT(*) FROM faux_offline_pending;",
                                                [&](const QVector<QVariant>& row) {
                                                    fauxOfflineCount = row[0].toLongLong();
                                                }};
    QVERIFY(db->execNow(std::move(fauxOfflineCountQuery)));
    QVERIFY(fauxOfflineCount == 1);

    int totalHistoryCount = -1;
    RawDatabase::Query totalHistoryCountQuery = {"SELECT COUNT(*) FROM history;",
                                                 [&](const QVector<QVariant>& row) {
                                                     totalHistoryCount = row[0].toLongLong();
                                                 }};
    QVERIFY(db->execNow(std::move(totalHistoryCountQuery)));
    QVERIFY(totalHistoryCount == 4);

    DbUtility::verifyDb(db, DbUtility::schema3);
}

void TestDbSchema::test3to4()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema3);
    QVERIFY(DbUpgrader::dbSchema3to4(*db));
    DbUtility::verifyDb(db, DbUtility::schema4);
}

void TestDbSchema::test4to5()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema4);
    QVERIFY(DbUpgrader::dbSchema4to5(*db));
    DbUtility::verifyDb(db, DbUtility::schema5);
}

void TestDbSchema::test5to6()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema5);
    QVERIFY(DbUpgrader::dbSchema5to6(*db));
    DbUtility::verifyDb(db, DbUtility::schema6);
}

void TestDbSchema::test6to7()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    // foreign_keys are enabled by History constructor and required for this upgrade to work on older sqlite versions
    db->execNow("PRAGMA foreign_keys = ON;");
    createSchemaAtVersion(db, DbUtility::schema6);
    QVERIFY(DbUpgrader::dbSchema6to7(*db));
    DbUtility::verifyDb(db, DbUtility::schema7);
}

void TestDbSchema::test9to10()
{
    auto db = RawDatabase::open(testDatabaseFile->fileName(), {}, {});
    createSchemaAtVersion(db, DbUtility::schema9);

    QVERIFY(insertFileId(*db, 1, true));
    QVERIFY(insertFileId(*db, 2, true));
    QVERIFY(insertFileId(*db, 3, false));
    QVERIFY(insertFileId(*db, 4, true));
    QVERIFY(insertFileId(*db, 5, false));
    QVERIFY(DbUpgrader::dbSchema9to10(*db));
    int numHealed = 0;
    int numUnchanged = 0;
    QVERIFY(db->execNow(RawDatabase::Query("SELECT file_restart_id from file_transfers;",
                                           [&](const QVector<QVariant>& row) {
                                               auto resumeId = row[0].toByteArray();
                                               if (resumeId == QByteArray(32, 0)) {
                                                   ++numHealed;
                                               } else if (resumeId == QByteArray(32, 1)) {
                                                   ++numUnchanged;
                                               } else {
                                                   QFAIL("Invalid file_restart_id");
                                               }
                                           })));
    QVERIFY(numHealed == 2);
    QVERIFY(numUnchanged == 3);
    verifyDb(db, DbUtility::schema10);
}

QTEST_GUILESS_MAIN(TestDbSchema)
#include "dbschema_test.moc"
