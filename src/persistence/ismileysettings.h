/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2022 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "util/interface.h"

#include <QString>

class ISmileySettings
{
public:
    virtual ~ISmileySettings();
    virtual QString getSmileyPack() const = 0;
    DECLARE_SIGNAL(smileyPackChanged, const QString& name);
};
