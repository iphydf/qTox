/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

namespace Platform {
/**
 * @brief Keeps Tox connected while other apps are in front, as long as it exists.
 *
 * Android freezes apps in the background otherwise. Its ongoing notification shows what
 * INotifier::showStatus() says. Does nothing elsewhere.
 */
class KeepAlive
{
public:
    KeepAlive();
    ~KeepAlive();
    KeepAlive(const KeepAlive&) = delete;
    KeepAlive& operator=(const KeepAlive&) = delete;
    KeepAlive(KeepAlive&&) = delete;
    KeepAlive& operator=(KeepAlive&&) = delete;
};
} // namespace Platform
