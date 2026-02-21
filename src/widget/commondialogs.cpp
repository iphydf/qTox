/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "commondialogs.h"

CommonDialogs::~CommonDialogs() = default;

CommonDialogs::Dialog CommonDialogs::NoWritePermission()
{
    return {
        tr("Location not writable", "Title of permissions popup"),
        tr("You do not have permission to write to that location. Choose "
           "another, or cancel the save dialog.",
           "text of permissions popup"),
    };
}
