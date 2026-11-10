/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "destination.h"

#include "src/conferencelist.h"
#include "src/core/conferenceid.h"
#include "src/core/groupid.h"
#include "src/core/toxpk.h"
#include "src/friendlist.h"
#include "src/grouplist.h"
#include "src/model/chat.h"
#include "src/model/conference.h"
#include "src/model/friend.h"
#include "src/model/group.h"

QString Destination::chat(const Chat& chat)
{
    return Destination::chat(ChatKind::of(chat), chat.getPersistentId().toString());
}

Chat* Destination::findChat(const QString& name, FriendList& friendList,
                            ConferenceList& conferenceList, GroupList& groupList)
{
    const auto chat = parseChat(name);
    if (!chat) {
        return nullptr;
    }
    const auto& [kind, id] = *chat;
    switch (kind) {
    case ChatKind::Kind::Friend:
        return friendList.findFriend(ToxPk{id});
    case ChatKind::Kind::Conference:
        return conferenceList.findConference(ConferenceId::fromString(id));
    case ChatKind::Kind::Group:
        return groupList.findGroup(GroupId::fromString(id));
    }
    return nullptr;
}
