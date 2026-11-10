/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/core/toxpk.h"
#include "src/model/ichatlog.h"
#include "src/persistence/settings.h"

#include <QAbstractListModel>

class ChatLogItem;

/**
 * @brief One chat's messages, oldest first, as QML lists them.
 */
class ChatModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        KindRole = Qt::UserRole + 1,
        TextRole,
        SenderRole,
        IsSelfRole,
        TimeRole,
        PendingRole,
        BrokenRole,
        IsActionRole,
        // Its local date, as yyyy-MM-dd.
        DayRole,
        // Files only: waiting, transmitting, paused, broken, canceled or finished.
        FileStatusRole,
        FileSizeRole,
        FileTransferredRole,
        // What the service's file calls take.
        FileNumRole,
        // Where it is or goes on this device.
        FilePathRole,
        // Whether the user paused it, rather than only the friend.
        FilePausedByUserRole,
        // Messages only: the text with links and the text style, as StyledText.
        HtmlRole,
        // Groups only: whether one peer sent it to another, and to whom the user sent theirs.
        IsPrivateRole,
        RecipientRole,
    };

    /** How messages look: the Widgets UI's settings. */
    struct Style
    {
        Settings::StyleType markdown;
        bool hidePostNullSuffix;
    };

    /** @param first The chat log's first item to show: earlier ones were cleared. */
    ChatModel(IChatLog& chatLog, ToxPk selfPk, ChatLogIdx first, Style style,
              QObject* parent = nullptr);

    /** Shows the chat log from @p first on: the user cleared the chat. */
    void startAt(ChatLogIdx first);

    /** What the chat shows of @p item: a message's text, a file's name, or a system message. */
    static QString text(const ChatLogItem& item);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    void onItemUpdated(size_t idx);
    QString html(const QString& message) const;

    IChatLog& chatLog;
    const ToxPk selfPk;
    const Style style;
    size_t first;
    size_t rows{0};
};
