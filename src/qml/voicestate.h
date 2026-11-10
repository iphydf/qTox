/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/core/icoregroupquery.h"

#include <QObject>

#include <QtQml/qqmlregistration.h>

/**
 * @brief GroupVoiceState for QML: who may write in a group.
 */
namespace VoiceState {
Q_NAMESPACE
QML_ELEMENT

enum class State
{
    All = static_cast<int>(GroupVoiceState::All),
    Moderators = static_cast<int>(GroupVoiceState::Moderator),
    Founder = static_cast<int>(GroupVoiceState::Founder),
};
Q_ENUM_NS(State)
} // namespace VoiceState
