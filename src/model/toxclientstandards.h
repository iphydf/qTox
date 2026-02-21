/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2020 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <cstdint>

namespace ToxClientStandards {
// From TCS 2.2.4, max valid avatar size is 64KiB
constexpr static uint64_t MaxAvatarSize = 64 * 1024;
constexpr bool IsValidAvatarSize(uint64_t fileSize)
{
    return fileSize <= MaxAvatarSize;
}
} // namespace ToxClientStandards
