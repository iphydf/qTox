/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/core/toxpk.h"
#include "src/model/conferenceinvite.h"
#include "src/model/groupinvite.h"

#include <QList>
#include <QObject>

#include <variant>

class Core;
class FriendList;
class Settings;

/**
 * @brief The invites to conferences and groups: which ones we join at once, and which ones wait
 * for the user's answer. A user interface shows those and tells the answer.
 */
class InviteManager : public QObject
{
    Q_OBJECT

public:
    using Invite = std::variant<ConferenceInvite, GroupInvite>;

    InviteManager(FriendList& friendList, Settings& settings, QObject* parent = nullptr);

    void connectToCore(Core& core);

    /** Joins. @param password of a group that has one. */
    void accept(const Invite& invite, const QString& password = {});
    void reject(const Invite& invite);

signals:
    /** A friend sent an invite, whether it waits for an answer or not. */
    void received(const ToxPk& friendPk);
    /** An invite waits for the user's answer. */
    void added(const InviteManager::Invite& invite, const ToxPk& friendPk);

private:
    void onConferenceInvite(const ConferenceInvite& invite);
    void onGroupInvite(const GroupInvite& invite);
    void add(const Invite& invite, const ToxPk& friendPk);

    FriendList& friendList;
    Settings& settings;
    Core* core = nullptr;
    QList<Invite> waiting;
};
