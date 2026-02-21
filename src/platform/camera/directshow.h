/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */


#pragma once

#include <QPair>
#include <QString>
#include <QVector>

#ifdef Q_OS_WIN
struct VideoMode;

namespace DirectShow {
QVector<QPair<QString, QString>> getDeviceList();
QVector<VideoMode> getDeviceModes(QString devName);
} // namespace DirectShow
#endif
