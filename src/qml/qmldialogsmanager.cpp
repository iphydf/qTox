/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "qmldialogsmanager.h"

#include <tuple>

IDialogs* QmlDialogsManager::getFriendDialogs(const ToxPk& friendPk) const
{
    std::ignore = friendPk;
    return nullptr;
}

IDialogs* QmlDialogsManager::getConferenceDialogs(const ConferenceId& conferenceId) const
{
    std::ignore = conferenceId;
    return nullptr;
}

IDialogs* QmlDialogsManager::getGroupDialogs(const GroupId& groupId) const
{
    std::ignore = groupId;
    return nullptr;
}
