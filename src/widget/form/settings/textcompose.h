/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

class QString;
class Style;

/**
 * Helper functions for creating label text.
 */
namespace TextCompose {
QString urlEncode(const QString& str);
QString createLink(Style& style, QString path, QString text);
} // namespace TextCompose
