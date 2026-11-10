/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "friendrequestmanager.h"

#include "src/core/core.h"
#include "src/persistence/settings.h"

FriendRequestManager::FriendRequestManager(Settings& settings_, QObject* parent)
    : QObject{parent}
    , settings{settings_}
{
}

void FriendRequestManager::connectToCore(Core& core_)
{
    core = &core_;
    connect(core, &Core::friendRequestReceived, this, &FriendRequestManager::onRequestReceived);
    // Asking someone who asked us answers their request.
    connect(core, &Core::requestSent, this, [this](const ToxPk& friendPk) {
        if (remove(friendPk)) {
            emit removed(friendPk);
        }
    });
}

void FriendRequestManager::accept(const ToxPk& friendPk)
{
    if (core == nullptr || !remove(friendPk)) {
        return;
    }
    core->acceptFriendRequest(friendPk);
    emit removed(friendPk);
}

void FriendRequestManager::reject(const ToxPk& friendPk)
{
    if (remove(friendPk)) {
        emit removed(friendPk);
    }
}

void FriendRequestManager::onRequestReceived(const ToxPk& friendPk, const QString& message)
{
    if (settings.addFriendRequest(friendPk.toString(), message)) {
        emit received(friendPk, message);
    }
}

bool FriendRequestManager::remove(const ToxPk& friendPk)
{
    for (int row = 0; row < settings.getFriendRequestSize(); ++row) {
        if (ToxPk{settings.getFriendRequest(row).address} == friendPk) {
            settings.removeFriendRequest(row);
            settings.savePersonal();
            return true;
        }
    }
    return false;
}
