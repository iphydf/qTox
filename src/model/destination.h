/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/chatkind.h"

#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

class Chat;
class ConferenceList;
class FriendList;
class GroupList;

/**
 * @brief The names of what a user interface shows: a chat, the friend requests or the invites.
 *
 * Notifications carry the name of what they're about, and tapping one opens it.
 */
namespace Destination {
inline const QString friendRequests = QStringLiteral("friendRequests");
inline const QString invites = QStringLiteral("invites");

/** A chat: @p id is a friend's public key, or a conference's or a group's persistent ID. */
inline QString chat(ChatKind::Kind kind, const QString& id)
{
    return QStringLiteral("chat/%1/%2").arg(static_cast<int>(kind)).arg(id);
}

QString chat(const Chat& chat);

/** The chat a chat() name names, if there is one. */
Chat* findChat(const QString& name, FriendList& friendList, ConferenceList& conferenceList,
               GroupList& groupList);

/** The kind and ID in a chat() name, if it is one. */
inline std::optional<std::pair<ChatKind::Kind, QString>> parseChat(const QString& name)
{
    const QStringList parts = name.split(u'/');
    bool isNumber = false;
    const int kind = parts.size() == 3 && parts[0] == u"chat" ? parts[1].toInt(&isNumber) : 0;
    const std::optional<ChatKind::Kind> chatKind = isNumber ? ChatKind::fromInt(kind) : std::nullopt;
    if (!chatKind || parts[2].isEmpty()) {
        return std::nullopt;
    }
    return std::pair{*chatKind, parts[2]};
}
} // namespace Destination
