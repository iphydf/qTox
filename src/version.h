/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2025-2026 The TokTok team.
 */

#pragma once

class QString;

namespace VersionInfo {
QString gitDescribe();
QString gitVersion();
QString gitDescribeExact();
} // namespace VersionInfo
