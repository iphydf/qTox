/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/core/toxpk.h"

#include <QObject>
#include <QString>

class Core;
class Settings;

/**
 * @brief The requests to be the user's friend that wait for an answer. They are kept in the
 * settings, so they are still there after a restart.
 */
class FriendRequestManager : public QObject
{
    Q_OBJECT

public:
    explicit FriendRequestManager(Settings& settings, QObject* parent = nullptr);

    void connectToCore(Core& core);

    void accept(const ToxPk& friendPk);
    void reject(const ToxPk& friendPk);

signals:
    /** Someone new asks. A request from someone who asked before only gets its new message. */
    void received(const ToxPk& friendPk, const QString& message);
    /** A request went: answered, or the user asked that person themselves. */
    void removed(const ToxPk& friendPk);

private:
    void onRequestReceived(const ToxPk& friendPk, const QString& message);
    bool remove(const ToxPk& friendPk);

    Settings& settings;
    Core* core = nullptr;
};
