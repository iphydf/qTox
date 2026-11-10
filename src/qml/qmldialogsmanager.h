/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/dialogs/idialogsmanager.h"

/**
 * @brief The QML UI shows every chat in its one window: there are no dialogs.
 */
class QmlDialogsManager : public IDialogsManager
{
public:
    IDialogs* getFriendDialogs(const ToxPk& friendPk) const override;
    IDialogs* getConferenceDialogs(const ConferenceId& conferenceId) const override;
    IDialogs* getGroupDialogs(const GroupId& groupId) const override;
};
