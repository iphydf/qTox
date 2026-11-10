/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "friendrequestmodel.h"

#include "src/persistence/settings.h"

FriendRequestModel::FriendRequestModel(Settings& settings_, QObject* parent)
    : QAbstractListModel(parent)
    , settings{settings_}
{
}

int FriendRequestModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : settings.getFriendRequestSize();
}

QVariant FriendRequestModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= settings.getFriendRequestSize()) {
        return {};
    }

    const Settings::Request request = settings.getFriendRequest(index.row());
    switch (role) {
    case PublicKeyRole:
        return request.address;
    case MessageRole:
        return request.message;
    default:
        return {};
    }
}

QHash<int, QByteArray> FriendRequestModel::roleNames() const
{
    return {
        {PublicKeyRole, "publicKey"},
        {MessageRole, "message"},
    };
}

void FriendRequestModel::reload()
{
    beginResetModel();
    endResetModel();
}
