/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "callvideoprovider.h"

#include "src/qml/qmlapp.h"

#include <tuple>

CallVideoProvider::CallVideoProvider(const QmlApp& qtox_)
    : QQuickImageProvider{QQuickImageProvider::Image}
    , qtox{qtox_}
{
}

QImage CallVideoProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    // The latest frame: the number in the ID only makes each a new source.
    std::ignore = requestedSize;
    const QImage frame =
        id.startsWith(QStringLiteral("self/")) ? qtox.selfVideoFrame() : qtox.callVideoFrame();
    *size = frame.size();
    return frame;
}
