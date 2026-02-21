/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#ifdef QTOX_PLATFORM_EXT

class Settings;
namespace Platform {
bool setAutorun(const Settings& settings, bool on);
bool getAutorun(const Settings& settings);
} // namespace Platform

#endif // QTOX_PLATFORM_EXT
