/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2020 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "exiftransform.h"

#include <QDebug>

#include <libexif/exif-loader.h>

namespace ExifTransform {
Orientation getOrientation(QByteArray imageData)
{
    const auto* data = imageData.constData();
    auto size = imageData.size();

    ExifData* exifData = exif_data_new_from_data(reinterpret_cast<const unsigned char*>(data), size);

    if (exifData == nullptr) {
        return Orientation::TopLeft;
    }

    const ExifByteOrder byteOrder = exif_data_get_byte_order(exifData);
    const ExifEntry* const exifEntry = exif_data_get_entry(exifData, EXIF_TAG_ORIENTATION);

    if (exifEntry == nullptr) {
        exif_data_free(exifData);
        return Orientation::TopLeft;
    }

    if (exifEntry->size < 2) {
        exif_data_free(exifData);
        return Orientation::TopLeft;
    }

    const int orientation = exif_get_short(exifEntry->data, byteOrder);
    exif_data_free(exifData);

    switch (orientation) {
    case 1:
        return Orientation::TopLeft;
    case 2:
        return Orientation::TopRight;
    case 3:
        return Orientation::BottomRight;
    case 4:
        return Orientation::BottomLeft;
    case 5:
        return Orientation::LeftTop;
    case 6:
        return Orientation::RightTop;
    case 7:
        return Orientation::RightBottom;
    case 8:
        return Orientation::LeftBottom;
    default:
        qWarning() << "Invalid exif orientation" << orientation;
        return Orientation::TopLeft;
    }
}

QImage applyTransformation(QImage image, Orientation orientation)
{
    QTransform exifTransform;
    switch (orientation) {
    case Orientation::TopLeft:
        break;
    case Orientation::TopRight:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        image = image.flipped(Qt::Horizontal);
#else
        image = image.mirrored(true, false);
#endif
        break;
    case Orientation::BottomRight:
        exifTransform.rotate(180);
        break;
    case Orientation::BottomLeft:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        image = image.flipped(Qt::Vertical);
#else
        image = image.mirrored(false, true);
#endif
        break;
    case Orientation::LeftTop:
        exifTransform.rotate(-90);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        image = image.flipped(Qt::Horizontal);
#else
        image = image.mirrored(true, false);
#endif
        break;
    case Orientation::RightTop:
        exifTransform.rotate(90);
        break;
    case Orientation::RightBottom:
        exifTransform.rotate(90);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        image = image.flipped(Qt::Horizontal);
#else
        image = image.mirrored(true, false);
#endif
        break;
    case Orientation::LeftBottom:
        exifTransform.rotate(-90);
        break;
    }
    image = image.transformed(exifTransform);
    return image;
}
} // namespace ExifTransform
