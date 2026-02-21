/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2022 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

class SettingsSerializer;

namespace PersonalSettingsUpgrader {
bool doUpgrade(SettingsSerializer& settingsSerializer, int fromVer, int toVer);
}
