/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QByteArray>

int dataToVInt(const QByteArray& data);
QByteArray vintToData(int num);
