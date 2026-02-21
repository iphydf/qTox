/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */


#pragma once

#include <QByteArray>
#include <QMap>
#include <QObject>

class Core;
class AvatarBroadcaster : public QObject
{
    Q_OBJECT
public:
    explicit AvatarBroadcaster(Core& _core);

    void setAvatar(QByteArray data);
    void sendAvatarTo(uint32_t friendId);
    void enableAutoBroadcast(bool state = true);

private:
    Core& core;
    QByteArray avatarData;
    QMap<uint32_t, bool> friendsSentTo;
};
