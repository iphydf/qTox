/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "qrprovider.h"

#include <QUrl>

#include <algorithm>
#include <memory>
#include <qrencode.h>

namespace {
// The blank border that scanners need, in modules.
constexpr int quietZone = 4;
} // namespace

QrProvider::QrProvider()
    : QQuickImageProvider{QQuickImageProvider::Image}
{
}

QImage QrProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    // As the Widgets UI's QRWidget encodes it.
    const QByteArray text = QUrl::fromPercentEncoding(id.toUtf8()).toUtf8();
    const std::unique_ptr<QRcode, decltype(&QRcode_free)> code{QRcode_encodeString(text.constData(),
                                                                                   0, QR_ECLEVEL_M,
                                                                                   QR_MODE_8, 1),
                                                               QRcode_free};
    if (code == nullptr) {
        return {};
    }

    // Whole pixels per module, so that every module is as big as the others.
    const int modules = code->width + 2 * quietZone;
    const int requested = std::min(requestedSize.width(), requestedSize.height());
    const int scale = std::max(1, requested / modules);
    QImage image{modules * scale, modules * scale, QImage::Format_Mono};
    image.fill(0);
    for (int y = 0; y < code->width; ++y) {
        for (int x = 0; x < code->width; ++x) {
            if ((code->data[y * code->width + x] & 1) == 0) {
                continue;
            }
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    image.setPixel((x + quietZone) * scale + dx, (y + quietZone) * scale + dy, 1);
                }
            }
        }
    }
    image.setColorTable({qRgb(255, 255, 255), qRgb(0, 0, 0)});
    *size = image.size();
    return image;
}
