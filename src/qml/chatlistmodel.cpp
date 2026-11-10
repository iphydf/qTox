/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "chatlistmodel.h"

#include "src/model/chat.h"
#include "src/model/conference.h"
#include "src/model/friend.h"
#include "src/model/group.h"
#include "src/model/ichatlog.h"
#include "src/model/status.h"
#include "src/qml/chatmodel.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

namespace {
const ChatLogItem* lastItem(const IChatLog& chatLog)
{
    const size_t next = chatLog.getNextIdx().get();
    return next == chatLog.getFirstIdx().get() ? nullptr : &chatLog.at(ChatLogIdx(next - 1));
}

/** @param selfPk The user's key there: a group gives its peers keys of its own. */
QVariantList peers(const QMap<ToxPk, QString>& names, const ToxPk& selfPk, const Group* group)
{
    QList<ToxPk> keys = names.keys();
    std::sort(keys.begin(), keys.end(), [&names](const ToxPk& left, const ToxPk& right) {
        return QString::localeAwareCompare(names.value(left), names.value(right)) < 0;
    });
    QVariantList list;
    for (const ToxPk& key : keys) {
        QVariantMap peer{{"name", names.value(key)},
                         {"publicKey", key.toString()},
                         {"self", key == selfPk}};
        if (group != nullptr) {
            peer.insert("role", static_cast<int>(group->getPeerRole(key)));
        }
        list.append(peer);
    }
    return list;
}
} // namespace

int ChatListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(chats.size());
}

QVariant ChatListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= chats.size()) {
        return {};
    }

    const ChatRow& row = chats.at(index.row());
    const auto* f = qobject_cast<const Friend*>(row.chat);
    const auto* conference = qobject_cast<const Conference*>(row.chat);
    const auto* group = qobject_cast<const Group*>(row.chat);
    const ChatLogItem* last = lastItem(*row.chatLog);
    const bool lastIsMessage =
        last != nullptr && last->getContentType() != ChatLogItem::ContentType::systemMessage;
    const bool lastIsSelf = lastIsMessage && last->getSender() == selfPk;
    switch (role) {
    case KindRole:
        return static_cast<int>(row.kind);
    case IdRole:
        return row.chat->getPersistentId().toString();
    case NameRole:
        return row.chat->getDisplayedName();
    case UserNameRole:
        return f != nullptr ? f->getUserName() : QString{};
    case AliasRole:
        return f != nullptr && f->hasAlias() ? f->getDisplayedName() : QString{};
    case StatusMessageRole:
        return f != nullptr ? f->getStatusMessage() : QString{};
    case OnlineRole:
        return f != nullptr && Status::isOnline(f->getStatus());
    case StatusRole:
        return static_cast<int>(f != nullptr ? f->getStatus() : Status::Status::Offline);
    case LastMessageRole:
        return last != nullptr ? ChatModel::text(*last) : QString{};
    case LastSenderRole:
        return lastIsMessage && !lastIsSelf ? last->getDisplayName() : QString{};
    case LastIsSelfRole:
        return lastIsSelf;
    case LastTimeRole:
        return last != nullptr ? last->getTimestamp() : QDateTime{};
    case UnreadRole:
        return row.unread;
    case AvatarRole:
        return row.avatar;
    case AvatarHashRole:
        return avatarHash(row.avatar);
    case AutoAcceptInvitesRole:
        return row.autoAcceptInvites;
    case AutoAcceptFilesRole:
        return row.autoAcceptFiles;
    case TypingRole:
        return row.typing;
    case NoteRole:
        return row.note;
    case PeersRole:
        return conference != nullptr ? peers(conference->getPeerList(), selfPk, nullptr)
               : group != nullptr    ? peers(group->getPeerList(), group->getSelfPeerPk(), group)
                                     : QVariantList{};
    case AudioRole:
        return conference != nullptr && conference->isAvConference();
    case TopicRole:
        return group != nullptr ? group->getTopic() : QString{};
    case NicknameRole:
        return group != nullptr ? group->getGroupNickname() : QString{};
    case PasswordSetRole:
        return group != nullptr && group->isPasswordSet();
    case PrivateRole:
        return group != nullptr && group->getPrivacyState() == GroupPrivacyState::Private;
    case PeerLimitRole:
        return group != nullptr ? group->getPeerLimit() : 0;
    case TopicLockedRole:
        return group != nullptr && group->getTopicLock() == GroupTopicLock::Enabled;
    case VoiceStateRole:
        return static_cast<int>(group != nullptr ? group->getVoiceState() : GroupVoiceState::All);
    default:
        return {};
    }
}

QHash<int, QByteArray> ChatListModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {IdRole, "chatId"},
        {NameRole, "name"},
        {UserNameRole, "userName"},
        {AliasRole, "alias"},
        {StatusMessageRole, "statusMessage"},
        {OnlineRole, "online"},
        {StatusRole, "status"},
        {LastMessageRole, "lastMessage"},
        {LastSenderRole, "lastSender"},
        {LastIsSelfRole, "lastIsSelf"},
        {LastTimeRole, "lastTime"},
        {UnreadRole, "unread"},
        {AvatarRole, "avatar"},
        {AvatarHashRole, "avatarHash"},
        {AutoAcceptInvitesRole, "autoAcceptInvites"},
        {AutoAcceptFilesRole, "autoAcceptFiles"},
        {TypingRole, "typing"},
        {NoteRole, "note"},
        {PeersRole, "peers"},
        {AudioRole, "audio"},
        {TopicRole, "topic"},
        {NicknameRole, "nickname"},
        {PasswordSetRole, "passwordSet"},
        {PrivateRole, "isPrivate"},
        {PeerLimitRole, "peerLimit"},
        {TopicLockedRole, "topicLocked"},
        {VoiceStateRole, "voiceState"},
    };
}

void ChatListModel::setSelf(const ToxPk& selfPk_)
{
    selfPk = selfPk_;
}

void ChatListModel::addFriend(const Friend* f, const IChatLog& chatLog)
{
    addChat(ChatKind::Kind::Friend, f, chatLog);
    connect(f, &Friend::nameChanged, this, [this, f] { chatChanged(f, {NameRole, UserNameRole}); });
    // Friend says it before it changes the alias.
    connect(f, &Friend::aliasChanged, this, [this, f] { chatChanged(f, {NameRole, AliasRole}); }, Qt::QueuedConnection);
    connect(f, &Friend::statusChanged, this, [this, f] { chatChanged(f, {OnlineRole, StatusRole}); });
    connect(f, &Friend::statusMessageChanged, this,
            [this, f] { chatChanged(f, {StatusMessageRole}); });
}

void ChatListModel::addConference(const Conference* conference, const IChatLog& chatLog)
{
    addChat(ChatKind::Kind::Conference, conference, chatLog);
    for (const auto signal : {&Conference::userJoined, &Conference::userLeft}) {
        connect(conference, signal, this,
                [this, conference] { chatChanged(conference, {PeersRole}); });
    }
    connect(conference, &Conference::peerNameChanged, this,
            [this, conference] { chatChanged(conference, {PeersRole}); });
}

void ChatListModel::addGroup(const Group* group, const IChatLog& chatLog)
{
    addChat(ChatKind::Kind::Group, group, chatLog);
    // The only one that also says the user disconnected, which clears the peers.
    connect(group, &Group::numPeersChanged, this, [this, group] { chatChanged(group, {PeersRole}); });
    connect(group, &Group::peerNameChanged, this, [this, group] { chatChanged(group, {PeersRole}); });
    connect(group, &Group::peerRolesChanged, this, [this, group] { chatChanged(group, {PeersRole}); });
    connect(group, &Group::topicChanged, this, [this, group] { chatChanged(group, {TopicRole}); });
    connect(group, &Group::nicknameChanged, this,
            [this, group] { chatChanged(group, {NicknameRole}); });
    connect(group, &Group::passwordSetChanged, this,
            [this, group] { chatChanged(group, {PasswordSetRole}); });
    connect(group, &Group::privacyStateChanged, this,
            [this, group] { chatChanged(group, {PrivateRole}); });
    connect(group, &Group::peerLimitChanged, this,
            [this, group] { chatChanged(group, {PeerLimitRole}); });
    connect(group, &Group::topicLockChanged, this,
            [this, group] { chatChanged(group, {TopicLockedRole}); });
    connect(group, &Group::voiceStateChanged, this,
            [this, group] { chatChanged(group, {VoiceStateRole}); });
}

void ChatListModel::addChat(ChatKind::Kind kind, const Chat* chat, const IChatLog& chatLog)
{
    const auto row = static_cast<int>(chats.size());
    beginInsertRows(QModelIndex(), row, row);
    chats.append({kind, chat, &chatLog, 0, {}, false, false, false, {}});
    endInsertRows();

    connect(chat, &Chat::displayedNameChanged, this, [this, chat] { chatChanged(chat, {NameRole}); });
    connect(&chatLog, &IChatLog::itemUpdated, this, [this, chat] {
        chatChanged(chat, {LastMessageRole, LastSenderRole, LastIsSelfRole, LastTimeRole});
    });
}

void ChatListModel::setAvatar(const QString& publicKey, const QByteArray& avatar)
{
    setFriendField(publicKey, &ChatRow::avatar, avatar, {AvatarRole, AvatarHashRole});
}

void ChatListModel::setAutoAcceptInvites(const QString& publicKey, bool accept)
{
    setFriendField(publicKey, &ChatRow::autoAcceptInvites, accept, {AutoAcceptInvitesRole});
}

void ChatListModel::setAutoAcceptFiles(const QString& publicKey, bool accept)
{
    setFriendField(publicKey, &ChatRow::autoAcceptFiles, accept, {AutoAcceptFilesRole});
}

void ChatListModel::setTyping(const QString& publicKey, bool typing)
{
    setFriendField(publicKey, &ChatRow::typing, typing, {TypingRole});
}

void ChatListModel::setNote(const QString& publicKey, const QString& note)
{
    setFriendField(publicKey, &ChatRow::note, note, {NoteRole});
}

void ChatListModel::removeChat(ChatKind::Kind kind, const QString& id)
{
    const int row = findRow(kind, id);
    if (row < 0) {
        return;
    }

    QObject::disconnect(chats.at(row).chat, nullptr, this, nullptr);
    QObject::disconnect(chats.at(row).chatLog, nullptr, this, nullptr);
    beginRemoveRows(QModelIndex(), row, row);
    chats.removeAt(row);
    endRemoveRows();
}

void ChatListModel::addUnread(ChatKind::Kind kind, const QString& id)
{
    const int row = findRow(kind, id);
    if (row >= 0) {
        ++chats[row].unread;
        emit dataChanged(index(row), index(row), {UnreadRole});
    }
}

void ChatListModel::markRead(ChatKind::Kind kind, const QString& id)
{
    const int row = findRow(kind, id);
    if (row >= 0 && chats.at(row).unread != 0) {
        chats[row].unread = 0;
        emit dataChanged(index(row), index(row), {UnreadRole});
    }
}

void ChatListModel::clear()
{
    beginResetModel();
    for (const ChatRow& row : std::as_const(chats)) {
        QObject::disconnect(row.chat, nullptr, this, nullptr);
        QObject::disconnect(row.chatLog, nullptr, this, nullptr);
    }
    chats.clear();
    endResetModel();
}

QString ChatListModel::avatarHash(const QByteArray& avatar)
{
    if (avatar.isEmpty()) {
        return {};
    }
    // Long enough to tell one picture from the next.
    return QString::fromLatin1(
        QCryptographicHash::hash(avatar, QCryptographicHash::Sha256).toHex().left(16));
}

template <typename T>
void ChatListModel::setFriendField(const QString& publicKey, T ChatRow::* field, const T& value,
                                   const QList<int>& roles)
{
    const int row = findRow(ChatKind::Kind::Friend, publicKey);
    if (row >= 0 && chats.at(row).*field != value) {
        chats[row].*field = value;
        emit dataChanged(index(row), index(row), roles);
    }
}

int ChatListModel::findRow(ChatKind::Kind kind, const QString& id) const
{
    for (int row = 0; row < chats.size(); ++row) {
        if (chats.at(row).kind == kind && chats.at(row).chat->getPersistentId().toString() == id) {
            return row;
        }
    }
    return -1;
}

int ChatListModel::findRow(const Chat* chat) const
{
    for (int row = 0; row < chats.size(); ++row) {
        if (chats.at(row).chat == chat) {
            return row;
        }
    }
    return -1;
}

void ChatListModel::chatChanged(const Chat* chat, const QList<int>& roles)
{
    const int row = findRow(chat);
    if (row >= 0) {
        emit dataChanged(index(row), index(row), roles);
    }
}
