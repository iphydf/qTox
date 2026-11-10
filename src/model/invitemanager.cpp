/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "invitemanager.h"

#include "src/core/core.h"
#include "src/core/groupid.h"
#include "src/friendlist.h"
#include "src/persistence/settings.h"

#include <QDebug>

#include <algorithm>
#include <limits>
#include <tox/tox.h>

namespace {
/** An invite's time is when it came, so the same invite sent again differs in that only. */
bool sameInvite(const InviteManager::Invite& a, const InviteManager::Invite& b)
{
    if (const auto* conference = std::get_if<ConferenceInvite>(&a)) {
        const auto* other = std::get_if<ConferenceInvite>(&b);
        return other != nullptr && conference->getInvite() == other->getInvite();
    }
    const auto& group = std::get<GroupInvite>(a);
    const auto* other = std::get_if<GroupInvite>(&b);
    return other != nullptr && group.getFriendId() == other->getFriendId()
           && group.getInviteData() == other->getInviteData();
}
} // namespace

InviteManager::InviteManager(FriendList& friendList_, Settings& settings_, QObject* parent)
    : QObject{parent}
    , friendList{friendList_}
    , settings{settings_}
{
}

void InviteManager::connectToCore(Core& core_)
{
    core = &core_;
    connect(core, &Core::conferenceInviteReceived, this, &InviteManager::onConferenceInvite);
    connect(core, &Core::groupInviteReceived, this, &InviteManager::onGroupInvite);
}

void InviteManager::accept(const Invite& invite, const QString& password)
{
    reject(invite);
    if (core == nullptr) {
        return;
    }
    if (const auto* conference = std::get_if<ConferenceInvite>(&invite)) {
        if (core->joinConference(*conference) == std::numeric_limits<uint32_t>::max()) {
            qWarning() << "Unable to accept conference invite";
        }
        return;
    }

    const auto& group = std::get<GroupInvite>(invite);
    if (core->joinGroup(group, password.toUtf8()) == std::numeric_limits<uint32_t>::max()) {
        qWarning() << "Unable to accept group invite";
    }
}

void InviteManager::reject(const Invite& invite)
{
    waiting.removeIf([&invite](const Invite& other) { return sameInvite(invite, other); });
}

void InviteManager::onConferenceInvite(const ConferenceInvite& invite)
{
    const ToxPk friendPk = friendList.id2Key(invite.getFriendId());
    if (friendList.findFriend(friendPk) == nullptr) {
        return;
    }
    emit received(friendPk);

    const uint8_t type = invite.getType();
    if (type != TOX_CONFERENCE_TYPE_TEXT && type != TOX_CONFERENCE_TYPE_AV) {
        qWarning() << "Unknown conference type:" << type;
        return;
    }
    if (settings.getAutoConferenceInvite(friendPk)) {
        core->joinConference(invite);
        return;
    }
    add(invite, friendPk);
}

void InviteManager::onGroupInvite(const GroupInvite& invite)
{
    const ToxPk friendPk = friendList.id2Key(invite.getFriendId());
    if (friendList.findFriend(friendPk) == nullptr) {
        return;
    }
    emit received(friendPk);

    // Without a password: if the group has one, Tox rejects it.
    if (settings.getAutoGroupInvite(friendPk)) {
        core->joinGroup(invite);
        return;
    }
    add(invite, friendPk);
}

void InviteManager::add(const Invite& invite, const ToxPk& friendPk)
{
    if (std::ranges::any_of(waiting,
                            [&invite](const Invite& other) { return sameInvite(invite, other); })) {
        return;
    }
    waiting.append(invite);
    emit added(invite, friendPk);
}
