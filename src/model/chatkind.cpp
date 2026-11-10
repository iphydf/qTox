/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "chatkind.h"

#include "src/model/conference.h"
#include "src/model/friend.h"

ChatKind::Kind ChatKind::of(const Chat& chat)
{
    if (qobject_cast<const Friend*>(&chat) != nullptr) {
        return Kind::Friend;
    }
    if (qobject_cast<const Conference*>(&chat) != nullptr) {
        return Kind::Conference;
    }
    return Kind::Group;
}
