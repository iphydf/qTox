/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/status.h"

#include <QObject>

#include <QtQml/qqmlregistration.h>

/**
 * @brief Status::Status for QML, which can't use that namespace's enum.
 */
namespace UserStatus {
Q_NAMESPACE
QML_ELEMENT

enum class Status
{
    Online = static_cast<int>(::Status::Status::Online),
    Away = static_cast<int>(::Status::Status::Away),
    Busy = static_cast<int>(::Status::Status::Busy),
    Offline = static_cast<int>(::Status::Status::Offline),
};
Q_ENUM_NS(Status)
} // namespace UserStatus
