/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "callduration.h"

#include <QCoreApplication>

QString secondsToDHMS(quint32 duration)
{
    // The chat form's, where this came from.
    const QString cD = QCoreApplication::translate("ChatForm", "Call duration: ");
    const quint32 seconds = duration % 60;
    duration /= 60;
    const quint32 minutes = duration % 60;
    duration /= 60;
    const quint32 hours = duration % 24;
    const quint32 days = duration / 24;

    // I assume no one will ever have call longer than a month
    if (days != 0u) {
        return cD + QString::asprintf("%ud%02uh %02um %02us", days, hours, minutes, seconds);
    }

    if (hours != 0u) {
        return cD + QString::asprintf("%02uh %02um %02us", hours, minutes, seconds);
    }

    if (minutes != 0u) {
        return cD + QString::asprintf("%02um %02us", minutes, seconds);
    }

    return cD + QString::asprintf("%02us", seconds);
}
