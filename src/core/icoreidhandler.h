/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "toxid.h"
#include "toxpk.h"

class ICoreIdHandler
{

public:
    ICoreIdHandler() = default;
    virtual ~ICoreIdHandler();
    ICoreIdHandler(const ICoreIdHandler&) = default;
    ICoreIdHandler& operator=(const ICoreIdHandler&) = default;
    ICoreIdHandler(ICoreIdHandler&&) = default;
    ICoreIdHandler& operator=(ICoreIdHandler&&) = default;

    virtual ToxId getSelfId() const = 0;
    virtual ToxPk getSelfPublicKey() const = 0;
    virtual QString getUsername() const = 0;
};
