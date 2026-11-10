/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/qml/chatkind.h"

#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

/**
 * @brief The names of the models the Tox service shares with the user interface.
 */
namespace RemoteModels {
inline const QString chats = QStringLiteral("chats");
inline const QString friendRequests = QStringLiteral("friendRequests");
inline const QString invites = QStringLiteral("invites");

/** A chat's messages: @p id is a friend's public key or a conference's persistent ID. */
inline QString chat(ChatKind::Kind kind, const QString& id)
{
    return QStringLiteral("chat/%1/%2").arg(static_cast<int>(kind)).arg(id);
}

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
} // namespace RemoteModels
