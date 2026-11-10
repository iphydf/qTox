/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QQuickImageProvider>

/**
 * @brief Shows the pictures that friends send, as the Widgets UI previews them.
 *
 * An image://preview/ source names the file's path, percent-encoded.
 */
class PreviewProvider : public QQuickImageProvider
{
public:
    PreviewProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
