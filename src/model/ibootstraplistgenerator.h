/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2020 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QList>
struct DhtServer;

class IBootstrapListGenerator
{
public:
    IBootstrapListGenerator() = default;
    virtual ~IBootstrapListGenerator();
    IBootstrapListGenerator(const IBootstrapListGenerator&) = default;
    IBootstrapListGenerator& operator=(const IBootstrapListGenerator&) = default;
    IBootstrapListGenerator(IBootstrapListGenerator&&) = default;
    IBootstrapListGenerator& operator=(IBootstrapListGenerator&&) = default;

    virtual QList<DhtServer> getBootstrapNodes() const = 0;
};
