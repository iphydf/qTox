/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

class QString;
class QWidget;

namespace Popup {
QString getAutoAcceptDir(QWidget* parent, const QString& dir);
} // namespace Popup
