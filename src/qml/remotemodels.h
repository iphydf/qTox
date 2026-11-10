/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/destination.h"

#include <QString>

/**
 * @brief The names of the models the Tox service shares with the user interface.
 */
namespace RemoteModels {
inline const QString chats = QStringLiteral("chats");

// The others are named after what they show.
using Destination::chat;
using Destination::friendRequests;
using Destination::invites;
using Destination::parseChat;
} // namespace RemoteModels
