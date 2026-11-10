/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QPair>
#include <QString>
#include <QVector>

#ifdef Q_OS_ANDROID
struct VideoMode;

/**
 * @brief The cameras that FFmpeg's android_camera input opens. A camera's name is its
 *        camera_index: its place in Android's list of cameras.
 */
namespace androidcamera {
// Front cameras first, then back ones, then the others.
QVector<QPair<QString, QString>> getDeviceList();
QVector<VideoMode> getDeviceModes(const QString& devName);
} // namespace androidcamera
#endif
