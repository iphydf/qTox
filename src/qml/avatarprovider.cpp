/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "avatarprovider.h"

#include "src/qml/qmlapp.h"

#include <QPainter>
#include <QPainterPath>

AvatarProvider::AvatarProvider(const QmlApp& qtox_)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , qtox{qtox_}
{
}

QString AvatarProvider::source(ChatKind::Kind kind, const QString& id, const QString& hash)
{
    // Until the replica has it.
    if (id.isEmpty()) {
        return {};
    }
    return QStringLiteral("image://avatar/%1/%2/%3").arg(static_cast<int>(kind)).arg(id, hash);
}

QImage AvatarProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    const QImage picture = qtox.avatar(id);
    if (picture.isNull()) {
        return {};
    }

    // The middle of the picture, in a circle.
    const int side = std::min(picture.width(), picture.height());
    const QRect middle{(picture.width() - side) / 2, (picture.height() - side) / 2, side, side};
    const int target =
        requestedSize.isValid() ? std::min(requestedSize.width(), requestedSize.height()) : side;
    QImage round{target, target, QImage::Format_ARGB32_Premultiplied};
    round.fill(Qt::transparent);
    QPainter painter{&round};
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QPainterPath circle;
    circle.addEllipse(round.rect());
    painter.setClipPath(circle);
    painter.drawImage(round.rect(), picture, middle);
    painter.end();

    if (size != nullptr) {
        *size = round.size();
    }
    return round;
}
