/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2025 The TokTok team.
 */

#include "util/network.h"

#include <QHostAddress>
#include <QHostInfo>

QList<QHostAddress> NetworkUtil::ipAddresses(const QHostInfo& hostInfo, bool enableIPv6)
{
    QList<QHostAddress> addresses;
    for (const QHostAddress& address : hostInfo.addresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            addresses.append(address);
        } else if (enableIPv6 && address.protocol() == QAbstractSocket::IPv6Protocol) {
            addresses.append(address);
        }
    }
    return addresses;
}
