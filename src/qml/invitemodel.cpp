/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "invitemodel.h"

#include <algorithm>
#include <utility>

ChatKind::Kind InviteModel::Invite::kind() const
{
    return std::holds_alternative<GroupInvite>(invite) ? ChatKind::Kind::Group
                                                       : ChatKind::Kind::Conference;
}

int InviteModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(invites.size());
}

QVariant InviteModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= invites.size()) {
        return {};
    }

    const Invite& invite = invites.at(index.row());
    switch (role) {
    case IdRole:
        return invite.id;
    case KindRole:
        return static_cast<int>(invite.kind());
    case FriendNameRole:
        return invite.friendName;
    case NameRole: {
        const auto* group = std::get_if<GroupInvite>(&invite.invite);
        return group != nullptr ? group->getGroupName() : QString{};
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> InviteModel::roleNames() const
{
    return {
        {IdRole, "inviteId"},
        {KindRole, "kind"},
        {FriendNameRole, "friendName"},
        {NameRole, "name"},
    };
}

void InviteModel::add(std::variant<ConferenceInvite, GroupInvite> invite, const QString& friendName)
{
    if (std::ranges::any_of(invites,
                            [&invite](const Invite& existing) { return existing.invite == invite; })) {
        return;
    }

    const auto row = static_cast<int>(invites.size());
    beginInsertRows(QModelIndex(), row, row);
    invites.append({QString::number(nextId++), std::move(invite), friendName});
    endInsertRows();
}

std::optional<InviteModel::Invite> InviteModel::take(const QString& id)
{
    for (int row = 0; row < invites.size(); ++row) {
        if (invites.at(row).id == id) {
            beginRemoveRows(QModelIndex(), row, row);
            Invite invite = invites.takeAt(row);
            endRemoveRows();
            return invite;
        }
    }
    return std::nullopt;
}

void InviteModel::clear()
{
    beginResetModel();
    invites.clear();
    endResetModel();
}
