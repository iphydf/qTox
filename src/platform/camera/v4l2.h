/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */
#pragma once

#include <QPair>
#include <QString>
#include <QVector>

#if (defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)) && !defined(ANDROID)
struct VideoMode;

#define USING_V4L 1
namespace v4l2 {
QVector<VideoMode> getDeviceModes(QString devName);
QVector<QPair<QString, QString>> getDeviceList();
QString getPixelFormatString(uint32_t pixel_format);
bool betterPixelFormat(uint32_t a, uint32_t b);
} // namespace v4l2
#endif
