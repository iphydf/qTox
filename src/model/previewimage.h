/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2020 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QImage>

class QString;

/** A received or sent picture to show in the chat, turned as it says: null if it's none. */
QImage loadPreviewImage(const QString& filename);
