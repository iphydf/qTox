/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QObject>

#include <QtQml/qqmlregistration.h>

/**
 * @brief Where the user's call with a friend is.
 */
namespace CallState {
Q_NAMESPACE
QML_ELEMENT

enum class State
{
    None,
    // The friend calls: the user can answer.
    Incoming,
    // The user calls: the friend can answer.
    Outgoing,
    Active,
};
Q_ENUM_NS(State)
} // namespace CallState
