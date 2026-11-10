/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/chatkind.h"

#include <QtQml/qqmlregistration.h>

/**
 * @brief Makes ChatKind the QML namespace ChatKind.
 */
namespace ChatKindForeign {
Q_NAMESPACE
QML_FOREIGN_NAMESPACE(ChatKind)
QML_NAMED_ELEMENT(ChatKind)
} // namespace ChatKindForeign
