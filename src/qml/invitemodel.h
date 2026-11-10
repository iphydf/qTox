/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/conferenceinvite.h"
#include "src/model/groupinvite.h"
#include "src/qml/chatkind.h"

#include <QAbstractListModel>
#include <QList>

#include <optional>
#include <variant>

/**
 * @brief The invitations to conferences and groups waiting for an answer.
 *
 * Kept only while the service runs, as the Widgets UI does.
 */
class InviteModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        // A ChatKind::Kind.
        KindRole,
        FriendNameRole,
        // Groups only: the name the invite gives.
        NameRole,
    };

    struct Invite
    {
        QString id;
        std::variant<ConferenceInvite, GroupInvite> invite;
        QString friendName;

        ChatKind::Kind kind() const;
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Does nothing if the friend sent the same invite before. */
    void add(std::variant<ConferenceInvite, GroupInvite> invite, const QString& friendName);
    std::optional<Invite> take(const QString& id);
    void clear();

private:
    QList<Invite> invites;
    quint64 nextId{0};
};
