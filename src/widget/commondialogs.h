/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QObject>

class CommonDialogs : public QObject
{
    Q_OBJECT

public:
    CommonDialogs() = delete;
    ~CommonDialogs() override;

    // Title and text of the dialog.
    using Dialog = QPair<QString, QString>;

    // Dialogs for errors and warnings.
    static Dialog NoWritePermission();
};
