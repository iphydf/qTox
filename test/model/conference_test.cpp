/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/model/conference.h"

#include "mock/mockconferencequery.h"
#include "mock/mockcoreidhandler.h"
#include "src/friendlist.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class TestConference : public QObject
{
    Q_OBJECT

private slots:
    void nameOfPeerWhoLeft();
};

void TestConference::nameOfPeerWhoLeft()
{
    MockConferenceQuery query;
    MockCoreIdHandler idHandler;
    FriendList friendList;
    Conference conference{0, ConferenceId(), "Test", false, "me", query, idHandler, friendList};
    const QMap<ToxPk, QString> peers = conference.getPeerList();
    const QSignalSpy renamed{&conference, &Conference::peerNameChanged};

    // The core's thread is ahead of the peer list: a new name can come after the peer left.
    const uint8_t key[ToxPk::size] = {9};
    conference.updateUsername(ToxPk{key}, "Gone");

    QCOMPARE(conference.getPeerList(), peers);
    QVERIFY(renamed.isEmpty());
}

QTEST_GUILESS_MAIN(TestConference)
#include "conference_test.moc"
