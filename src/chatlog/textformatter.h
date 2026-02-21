/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2017-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

class QString;

namespace TextFormatter {
QString highlightURI(const QString& message);

QString applyMarkdown(const QString& message, bool showFormattingSymbols);

QString processPostNullSuffix(QString message, bool html);
} // namespace TextFormatter
