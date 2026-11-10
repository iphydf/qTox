/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2020 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "previewimage.h"

#include "src/core/untrustedimage.h"
#include "src/model/exiftransform.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

QImage loadPreviewImage(const QString& filename)
{
    static const QStringList previewExtensions = {
        "gif",  // Graphics Interchange Format
        "jpeg", // Joint Photographic Experts Group
        "jpg",  // Joint Photographic Experts Group
        "png",  // Portable Network Graphics
        "qoi",  // Quite OK Image Format
        "svg",  // Scalable Vector Graphics
        "webp", // WebP
    };

    if (!previewExtensions.contains(QFileInfo(filename).suffix().toLower())) {
        return {};
    }

    QFile imageFile(filename);
    if (!imageFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray imageFileData = imageFile.readAll();
    QImage image = UntrustedImage::decode(imageFileData);
    auto orientation = ExifTransform::getOrientation(imageFileData);
    return ExifTransform::applyTransformation(image, orientation);
}
