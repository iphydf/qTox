/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/core/toxpk.h"
#include "src/qml/chatkind.h"

#include <QAbstractListModel>
#include <QList>

class Chat;
class Conference;
class Friend;
class Group;
class IChatLog;

/**
 * @brief The user's chats with friends, in conferences and in groups, as QML lists them.
 */
class ChatListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        // A ChatKind::Kind.
        KindRole = Qt::UserRole + 1,
        // A friend's public key, or a conference's or a group's persistent ID.
        IdRole,
        // The alias, if the user gave one, else the chat's own name.
        NameRole,
        // Friends only.
        UserNameRole,
        AliasRole,
        StatusMessageRole,
        OnlineRole,
        // A Status::Status.
        StatusRole,
        LastMessageRole,
        // Who sent the last message: empty for the user and for system messages.
        LastSenderRole,
        LastIsSelfRole,
        // Invalid when the chat has no messages.
        LastTimeRole,
        // The messages that came in since the user last looked at the chat.
        UnreadRole,
        // Friends only: their avatar as a PNG, as the profile saved it, and what changes with it.
        AvatarRole,
        AvatarHashRole,
        // Friends only: whether qTox joins the conferences they invite the user to, and saves the
        // files they send without asking.
        AutoAcceptInvitesRole,
        AutoAcceptFilesRole,
        // Friends only: whether they're writing to the user.
        TypingRole,
        // Friends only: what the user notes about them.
        NoteRole,
        // Conferences and groups: who's there, the user too, as {name, publicKey, self}, by name,
        // and in groups also {role}, a PeerRole::Role. A group's public keys are its own.
        PeersRole,
        // Conferences only: whether they have calls.
        AudioRole,
        // Groups only: what the group is about, and the user's name there, empty for the
        // profile's.
        TopicRole,
        NicknameRole,
        // Groups only: what the founder decided. Who may write is a VoiceState::State.
        PasswordSetRole,
        PrivateRole,
        PeerLimitRole,
        TopicLockedRole,
        VoiceStateRole,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Whose messages are the user's own: set before adding chats. */
    void setSelf(const ToxPk& selfPk);
    void addFriend(const Friend* f, const IChatLog& chatLog);
    void addConference(const Conference* conference, const IChatLog& chatLog);
    void addGroup(const Group* group, const IChatLog& chatLog);
    /** Empty for none. */
    void setAvatar(const QString& publicKey, const QByteArray& avatar);
    void setAutoAcceptInvites(const QString& publicKey, bool accept);
    void setAutoAcceptFiles(const QString& publicKey, bool accept);
    void setTyping(const QString& publicKey, bool typing);
    void setNote(const QString& publicKey, const QString& note);
    void removeChat(ChatKind::Kind kind, const QString& id);
    void addUnread(ChatKind::Kind kind, const QString& id);
    void markRead(ChatKind::Kind kind, const QString& id);
    void clear();

    /** What changes when @p avatar does: empty for none. */
    static QString avatarHash(const QByteArray& avatar);

private:
    struct ChatRow
    {
        ChatKind::Kind kind;
        const Chat* chat;
        const IChatLog* chatLog;
        int unread;
        QByteArray avatar;
        bool autoAcceptInvites;
        bool autoAcceptFiles;
        bool typing;
        QString note;
    };

    void addChat(ChatKind::Kind kind, const Chat* chat, const IChatLog& chatLog);
    template <typename T>
    void setFriendField(const QString& publicKey, T ChatRow::* field, const T& value,
                        const QList<int>& roles);
    int findRow(ChatKind::Kind kind, const QString& id) const;
    int findRow(const Chat* chat) const;
    void chatChanged(const Chat* chat, const QList<int>& roles);

    ToxPk selfPk;
    QList<ChatRow> chats;
};
