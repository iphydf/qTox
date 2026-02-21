/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QDialog>

class Style;

class ActivateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ActivateDialog(Style& style, QWidget* parent = nullptr,
                            Qt::WindowFlags f = Qt::WindowFlags());
    bool event(QEvent* event) override;

public slots:
    virtual void reloadTheme() {}

signals:
    void windowStateChanged(Qt::WindowStates state);
};
