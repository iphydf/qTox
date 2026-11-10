/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QQuickImageProvider>

/**
 * @brief Draws the text that an image://qr/ source names, percent-encoded, as a QR code.
 */
class QrProvider : public QQuickImageProvider
{
public:
    QrProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
