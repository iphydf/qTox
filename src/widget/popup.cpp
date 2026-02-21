/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "popup.h"

#include <QFileDialog>

QString Popup::getAutoAcceptDir(QWidget* parent, const QString& dir)
{
    const QString title = QFileDialog::tr("Choose an auto-accept directory", "popup title");
    return QFileDialog::getExistingDirectory(parent, title, dir);
}
