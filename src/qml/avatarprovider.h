/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/qml/chatkind.h"

#include <QQuickImageProvider>

class QmlApp;

/**
 * @brief Draws the avatars that QmlApp names, round.
 */
class AvatarProvider : public QQuickImageProvider
{
public:
    explicit AvatarProvider(const QmlApp& qtox);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    /**
     * @brief The image source of a chat's avatar, whose ChatListModel::avatarHash() is @p hash.
     *
     * None until there's an @p id.
     */
    static QString source(ChatKind::Kind kind, const QString& id, const QString& hash);

private:
    const QmlApp& qtox;
};
