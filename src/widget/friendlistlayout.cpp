/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "friendlistlayout.h"

#include "friendlistwidget.h"
#include "friendwidget.h"

#include "src/friendlist.h"
#include "src/model/friend.h"
#include "src/model/status.h"

FriendListLayout::FriendListLayout()
    : QVBoxLayout()
{
    init();
}

FriendListLayout::FriendListLayout(QWidget* parent)
    : QVBoxLayout(parent)
{
    init();
}

void FriendListLayout::init()
{
    setSpacing(0);
    setContentsMargins(0, 0, 0, 0);

    friendOnlineLayout.getLayout()->setSpacing(0);
    friendOnlineLayout.getLayout()->setContentsMargins(0, 0, 0, 0);

    friendOfflineLayout.getLayout()->setSpacing(0);
    friendOfflineLayout.getLayout()->setContentsMargins(0, 0, 0, 0);

    addLayout(friendOnlineLayout.getLayout());
    addLayout(friendOfflineLayout.getLayout());
}

void FriendListLayout::addFriendWidget(FriendWidget* w, Status::Status s)
{
    friendOfflineLayout.removeSortedWidget(w);
    friendOnlineLayout.removeSortedWidget(w);

    if (s == Status::Status::Offline) {
        friendOfflineLayout.addSortedWidget(w);
        return;
    }

    friendOnlineLayout.addSortedWidget(w);
}

void FriendListLayout::removeFriendWidget(FriendWidget* widget, Status::Status s)
{
    if (s == Status::Status::Offline)
        friendOfflineLayout.removeSortedWidget(widget);
    else
        friendOnlineLayout.removeSortedWidget(widget);
}

int FriendListLayout::indexOfFriendWidget(GenericChatItemWidget* widget, bool online) const
{
    if (online)
        return friendOnlineLayout.indexOfSortedWidget(widget);
    return friendOfflineLayout.indexOfSortedWidget(widget);
}

void FriendListLayout::moveFriendWidgets(FriendListWidget* listWidget)
{
    while (!friendOnlineLayout.getLayout()->isEmpty()) {
        QWidget* getWidget = friendOnlineLayout.getLayout()->takeAt(0)->widget();

        auto* friendWidget = qobject_cast<FriendWidget*>(getWidget);
        const Friend* f = friendWidget->getFriend();
        listWidget->moveWidget(friendWidget, f->getStatus(), true);
    }
    while (!friendOfflineLayout.getLayout()->isEmpty()) {
        QWidget* getWidget = friendOfflineLayout.getLayout()->takeAt(0)->widget();

        auto* friendWidget = qobject_cast<FriendWidget*>(getWidget);
        const Friend* f = friendWidget->getFriend();
        listWidget->moveWidget(friendWidget, f->getStatus(), true);
    }
}

int FriendListLayout::friendOnlineCount() const
{
    return friendOnlineLayout.getLayout()->count();
}

int FriendListLayout::friendTotalCount() const
{
    return friendOfflineLayout.getLayout()->count() + friendOnlineCount();
}

bool FriendListLayout::hasChatRooms() const
{
    return !(friendOfflineLayout.getLayout()->isEmpty() && friendOnlineLayout.getLayout()->isEmpty());
}

void FriendListLayout::searchChatRooms(const QString& searchString, bool hideOnline, bool hideOffline)
{
    friendOnlineLayout.search(searchString, hideOnline);
    friendOfflineLayout.search(searchString, hideOffline);
}

QLayout* FriendListLayout::getLayoutOnline() const
{
    return friendOnlineLayout.getLayout();
}

QLayout* FriendListLayout::getLayoutOffline() const
{
    return friendOfflineLayout.getLayout();
}

QLayout* FriendListLayout::getFriendLayout(Status::Status s) const
{
    return s == Status::Status::Offline ? friendOfflineLayout.getLayout()
                                        : friendOnlineLayout.getLayout();
}
