/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QAbstractListModel>

class Settings;

/**
 * @brief The friend requests waiting for an answer, which Settings stores.
 */
class FriendRequestModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        PublicKeyRole = Qt::UserRole + 1,
        MessageRole,
    };

    explicit FriendRequestModel(Settings& settings, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Call after Settings' requests changed. */
    void reload();

private:
    Settings& settings;
};
