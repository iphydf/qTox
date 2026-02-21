/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QObject>
#include <QPixmap>
#include <QRect>

class AbstractScreenshotGrabber : public QObject
{
    Q_OBJECT

public:
    explicit AbstractScreenshotGrabber(QObject* parent);
    ~AbstractScreenshotGrabber() override;

    virtual void showGrabber() = 0;

signals:
    void screenshotTaken(const QPixmap& pixmap);
    void regionChosen(QRect region);
    void rejected();
};
