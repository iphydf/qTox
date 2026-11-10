/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/platform/systeminsets.h"

#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/**
 * @brief Makes the one SystemInsets the QML singleton SystemInsets.
 */
struct SystemInsetsForeign
{
    Q_GADGET
    QML_FOREIGN(SystemInsets)
    QML_SINGLETON
    QML_NAMED_ELEMENT(SystemInsets)

public:
    static SystemInsets* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
};
