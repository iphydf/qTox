/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/core/icoregroupquery.h"

#include <QObject>

#include <QtQml/qqmlregistration.h>

/**
 * @brief GroupRole for QML, which can only use enums in namespaces like this.
 */
namespace PeerRole {
Q_NAMESPACE
QML_ELEMENT

enum class Role
{
    Founder = static_cast<int>(GroupRole::Founder),
    Moderator = static_cast<int>(GroupRole::Moderator),
    User = static_cast<int>(GroupRole::User),
    Observer = static_cast<int>(GroupRole::Observer),
    Unknown = static_cast<int>(GroupRole::Unknown),
};
Q_ENUM_NS(Role)
} // namespace PeerRole
