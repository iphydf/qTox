/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/widget/widget.h"

#include "src/core/core.h"
#include "src/core/toxid.h"
#include "src/ipc.h"
#include "src/model/friend.h"
#include "src/model/group.h"
#include "src/nexus.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"
#include "src/video/camerasource.h"
#include "src/widget/conferencewidget.h"
#include "src/widget/friendwidget.h"
#include "src/widget/groupwidget.h"
#include "src/widget/tool/messageboxmanager.h"

#include <QApplication>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>
#include <tox/tox.h>

class TestWidget : public QObject
{
    Q_OBJECT

private slots:
    void startsAndStops();
};

/**
 * The main window comes up for a new profile, hears of its Core, shows a chat of each kind and
 * goes away again. Nothing else builds the whole Widgets UI, so this finds what breaks when it
 * is put together.
 */
void TestWidget::startsAndStops()
{
    // A signal or slot that isn't there.
    QTest::failOnWarning(QRegularExpression{"QObject::connect"});

    const QTemporaryDir dir;
    QVERIFY(dir.isValid());
    MessageBoxManager messageBoxManager{nullptr};
    Settings settings{messageBoxManager, Paths::Portable::Portable, dir.path()};
    IPC ipc{settings.getCurrentProfileId()};
    CameraSource cameraSource{settings};

    Nexus nexus{settings, messageBoxManager, cameraSource, ipc};
    // Shows the main window.
    nexus.logIn(Session::create("alice", {}, settings, cameraSource, messageBoxManager));
    QVERIFY(nexus.getProfile() != nullptr);

    // The Core starts in its thread and tells the main window.
    Core& core = nexus.getProfile()->getCore();
    QTRY_VERIFY(core.getSelfPublicKey().toString().size() > 0);
    QTest::qWait(1000);

    // A chat of each kind gets its row in the chat list. They go again with the main window.
    QWidget* mainWindow = nullptr;
    for (QWidget* window : QApplication::topLevelWidgets()) {
        if (qobject_cast<Widget*>(window) != nullptr) {
            mainWindow = window;
        }
    }
    QVERIFY(mainWindow != nullptr);

    // Someone to add as a friend: any Tox has an address.
    Tox* other = tox_new(nullptr, nullptr);
    QVERIFY(other != nullptr);
    QByteArray address(TOX_ADDRESS_SIZE, 0);
    tox_self_get_address(other, reinterpret_cast<uint8_t*>(address.data()));
    tox_kill(other);
    core.requestFriendship(ToxId{address}, "Hello");
    QTRY_COMPARE(mainWindow->findChildren<FriendWidget*>().size(), 1);

    core.createConference(TOX_CONFERENCE_TYPE_TEXT);
    QTRY_COMPARE(mainWindow->findChildren<ConferenceWidget*>().size(), 1);

    const int groupNumber = core.createGroup("Test group");
    QTRY_COMPARE(mainWindow->findChildren<GroupWidget*>().size(), 1);

    // A message in a chat the user isn't looking at marks the chat.
    const Friend* f = mainWindow->findChildren<FriendWidget*>().first()->getFriend();
    QVERIFY(!f->getEventFlag());
    emit core.friendMessageReceived(f->getId(), "Hello", false);
    QTRY_VERIFY(f->getEventFlag());

    const Group* group = mainWindow->findChildren<GroupWidget*>().first()->getGroup();
    QVERIFY(!group->getEventFlag());
    emit core.groupMessageReceived(static_cast<uint32_t>(groupNumber), f->getPublicKey(),
                                   "Hello group", false);
    QTRY_VERIFY(group->getEventFlag());
}

QTEST_MAIN(TestWidget)
#include "widget_test.moc"
