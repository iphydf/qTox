/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "previewprovider.h"

#include "src/model/previewimage.h"

#include <QUrl>

PreviewProvider::PreviewProvider()
    : QQuickImageProvider{QQuickImageProvider::Image}
{
}

QImage PreviewProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    QImage image = loadPreviewImage(QUrl::fromPercentEncoding(id.toUtf8()));
    // Only as big as shown: a photo takes a lot of memory.
    if (!image.isNull() && requestedSize.isValid()
        && (image.width() > requestedSize.width() || image.height() > requestedSize.height())) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    *size = image.size();
    return image;
}
