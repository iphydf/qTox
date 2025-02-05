/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2025 The TokTok team.
 */

#pragma once

#include <QList>

class QHostAddress;
class QHostInfo;

namespace NetworkUtil {
QList<QHostAddress> ipAddresses(const QHostInfo& hostInfo, bool enableIPv6);
} // namespace NetworkUtil
