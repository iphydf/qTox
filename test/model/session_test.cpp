/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/model/session.h"

#include "src/core/core.h"
#include "src/core/toxid.h"
#include "src/friendlist.h"
#include "src/model/callmanager.h"
#include "src/model/chatmanager.h"
#include "src/model/conferenceinvite.h"
#include "src/model/filemanager.h"
#include "src/model/friend.h"
#include "src/model/friendrequestmanager.h"
#include "src/model/imessageboxmanager.h"
#include "src/model/invitemanager.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"
#include "src/video/camerasource.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>
#include <tox/tox.h>

namespace {
/** Nobody to ask: every answer is no. */
class NoMessageBoxes : public IMessageBoxManager
{
public:
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
    }
    bool askQuestion(const QString& title, const QString& msg, bool defaultAns, bool warning,
                     bool yesno) override
    {
        std::ignore = title;
        std::ignore = msg;
        std::ignore = defaultAns;
        std::ignore = warning;
        std::ignore = yesno;
        return false;
    }
    bool askQuestion(const QString& title, const QString& msg, const QString& button1,
                     const QString& button2, bool defaultAns, bool warning) override
    {
        std::ignore = title;
        std::ignore = msg;
        std::ignore = button1;
        std::ignore = button2;
        std::ignore = defaultAns;
        std::ignore = warning;
        return false;
    }
    void confirmExecutableOpen(const QFileInfo& file) override
    {
        std::ignore = file;
    }
};

/** Any Tox has an address. */
ToxId someAddress()
{
    Tox* other = tox_new(nullptr, nullptr);
    QByteArray address(TOX_ADDRESS_SIZE, 0);
    tox_self_get_address(other, reinterpret_cast<uint8_t*>(address.data()));
    tox_kill(other);
    return ToxId{address};
}
} // namespace

/**
 * What a login holds, without a user interface: the rules that both user interfaces share.
 * Toxcore's events are emitted by hand, so nothing waits for the network.
 */
class TestSession : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void loginErrors();
    void friendRequests();
    void invites();
    void messages();
    void uniquePaths();

private:
    QTemporaryDir dir;
    NoMessageBoxes messageBoxes;
    std::unique_ptr<Settings> settings;
    std::unique_ptr<CameraSource> cameraSource;
    std::unique_ptr<Session> session;
    Core* core = nullptr;
    const Friend* f = nullptr;
};

void TestSession::initTestCase()
{
    QVERIFY(dir.isValid());
    settings = std::make_unique<Settings>(messageBoxes, Paths::Portable::Portable, dir.path());
    cameraSource = std::make_unique<CameraSource>(*settings);

    Session::LoginResult login =
        Session::create("alice", "secret", *settings, *cameraSource, messageBoxes);
    QCOMPARE(login.error, Session::LoginError::None);
    session = std::move(login.session);
    session->start();
    core = &session->getProfile().getCore();

    core->requestFriendship(someAddress(), "Hello");
    QTRY_COMPARE(session->getFriendList().getAllFriends().size(), 1);
    f = session->getFriendList().getAllFriends().first();
}

void TestSession::cleanupTestCase()
{
    session.reset();
}

void TestSession::loginErrors()
{
    QCOMPARE(Session::create("alice", {}, *settings, *cameraSource, messageBoxes).error,
             Session::LoginError::AlreadyExists);
    // The session above holds it.
    QCOMPARE(Session::load("alice", "secret", *settings, *cameraSource, messageBoxes).error,
             Session::LoginError::InUse);
}

void TestSession::friendRequests()
{
    FriendRequestManager& requests = session->getFriendRequestManager();
    QSignalSpy received(&requests, &FriendRequestManager::received);
    QSignalSpy removed(&requests, &FriendRequestManager::removed);
    const ToxPk stranger = someAddress().getPublicKey();

    emit core->friendRequestReceived(stranger, "Let me in");
    QCOMPARE(received.count(), 1);
    QCOMPARE(settings->getFriendRequestSize(), 1);

    requests.reject(stranger);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(settings->getFriendRequestSize(), 0);
}

void TestSession::invites()
{
    InviteManager& invites = session->getInviteManager();
    int added = 0;
    InviteManager::Invite last;
    connect(&invites, &InviteManager::added, this,
            [&](const InviteManager::Invite& invite, const ToxPk& friendPk) {
                QCOMPARE(friendPk, f->getPublicKey());
                last = invite;
                ++added;
            });

    const ConferenceInvite invite{f->getId(), TOX_CONFERENCE_TYPE_TEXT, "cookie"};
    emit core->conferenceInviteReceived(invite);
    QCOMPARE(added, 1);

    // Toxcore sends an invite again until it is answered.
    emit core->conferenceInviteReceived(invite);
    QCOMPARE(added, 1);

    // Not from someone who isn't a friend.
    emit core->conferenceInviteReceived(
        ConferenceInvite{f->getId() + 1, TOX_CONFERENCE_TYPE_TEXT, "other"});
    QCOMPARE(added, 1);

    // Once answered, the same invite counts as new.
    invites.reject(last);
    emit core->conferenceInviteReceived(invite);
    QCOMPARE(added, 2);
    invites.reject(last);
}

void TestSession::messages()
{
    int notified = 0;
    connect(&session->getChatManager(), &ChatManager::messageReceived, this,
            [&](Chat& chat, const ToxPk& sender, const Message& message, bool notify) {
                QCOMPARE(&chat, static_cast<const Chat*>(f));
                QCOMPARE(sender, f->getPublicKey());
                QCOMPARE(message.content, QStringLiteral("Hello"));
                notified += notify ? 1 : 0;
            });

    emit core->friendMessageReceived(f->getId(), "Hello", false);
    QCOMPARE(notified, 1);
    QCOMPARE(session->getCallManager().state(f->getPublicKey()), CallManager::State::None);
}

void TestSession::uniquePaths()
{
    const QString first = FileManager::uniquePath(dir.path(), "note.txt");
    QCOMPARE(first, dir.filePath("note.txt"));
    QFile file{first};
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QCOMPARE(FileManager::uniquePath(dir.path(), "note.txt"), dir.filePath("note (1).txt"));
}

QTEST_MAIN(TestSession)
#include "session_test.moc"
