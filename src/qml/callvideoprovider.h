/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QQuickImageProvider>

class QmlApp;

/**
 * @brief Shows QmlApp's callVideo and selfVideo: the friend's video in the call, and the user's.
 */
class CallVideoProvider : public QQuickImageProvider
{
public:
    explicit CallVideoProvider(const QmlApp& qtox);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    const QmlApp& qtox;
};
