/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "qmlchat.h"

#include "rep_toxservice_replica.h"

#include "src/qml/avatarprovider.h"
#include "src/qml/files.h"
#include "src/qml/remotemodels.h"

#include <QAbstractItemModelReplica>
#include <QDir>
#include <QFile>
#include <QRemoteObjectNode>
#include <QRemoteObjectPendingCallWatcher>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>

namespace {
bool moderates(PeerRole::Role role)
{
    return role == PeerRole::Role::Founder || role == PeerRole::Role::Moderator;
}

/** The peer in @p peers with @p publicKey, or an empty map. */
QVariantMap findPeer(const QVariantList& peers, const QString& publicKey)
{
    const auto it = std::find_if(peers.cbegin(), peers.cend(), [&publicKey](const QVariant& peer) {
        return peer.toMap().value("publicKey").toString() == publicKey;
    });
    return it != peers.cend() ? it->toMap() : QVariantMap{};
}
} // namespace

QmlChat::QmlChat(ChatKind::Kind kind_, QString id_, ToxServiceReplica& service_,
                 QRemoteObjectNode& node, QAbstractItemModel& chats_, QObject* parent)
    : QObject(parent)
    , kind{kind_}
    , id{std::move(id_)}
    , service{service_}
    , chats{chats_}
    // Only what the list shows: prefetching makes the service load the chat's whole history.
    , messages{node.acquireModel(RemoteModels::chat(kind, id), QtRemoteObjects::FetchRootSize)}
{
    state.name = id;
    // As the Widgets UI's chat form.
    constexpr auto typingDuration = std::chrono::seconds{3};
    userTypingTimer.setSingleShot(true);
    userTypingTimer.setInterval(typingDuration);
    connect(&userTypingTimer, &QTimer::timeout, this, [this] {
        userTyping = false;
        service.setTyping(id, false);
    });
    connect(&chats, &QAbstractItemModel::modelReset, this, &QmlChat::update);
    connect(&chats, &QAbstractItemModel::rowsInserted, this, &QmlChat::update);
    connect(&chats, &QAbstractItemModel::dataChanged, this, &QmlChat::update);
    update();
}

ChatKind::Kind QmlChat::getKind() const
{
    return kind;
}

QmlChat::~QmlChat() = default;

QString QmlChat::getName() const
{
    return state.name;
}

QString QmlChat::getUserName() const
{
    return state.userName;
}

QString QmlChat::getAlias() const
{
    return state.alias;
}

QString QmlChat::getStatusMessage() const
{
    return state.statusMessage;
}

bool QmlChat::isOnline() const
{
    return state.online;
}

int QmlChat::getStatus() const
{
    return state.status;
}

QString QmlChat::getAvatar() const
{
    return state.avatar;
}

bool QmlChat::getAutoAcceptInvites() const
{
    return state.autoAcceptInvites;
}

bool QmlChat::getAutoAcceptFiles() const
{
    return state.autoAcceptFiles;
}

bool QmlChat::isTyping() const
{
    return state.typing;
}

QString QmlChat::getNote() const
{
    return state.note;
}

QVariantList QmlChat::getPeers() const
{
    return state.peers;
}

bool QmlChat::hasAudio() const
{
    return state.audio;
}

QString QmlChat::getTopic() const
{
    return state.topic;
}

QString QmlChat::getNickname() const
{
    return state.nickname;
}

bool QmlChat::isPasswordSet() const
{
    return state.passwordSet;
}

bool QmlChat::isPrivate() const
{
    return state.isPrivate;
}

int QmlChat::getPeerLimit() const
{
    return state.peerLimit;
}

bool QmlChat::isTopicLocked() const
{
    return state.topicLocked;
}

VoiceState::State QmlChat::getVoiceState() const
{
    return state.voiceState;
}

PeerRole::Role QmlChat::getSelfRole() const
{
    for (const QVariant& peer : state.peers) {
        const QVariantMap map = peer.toMap();
        if (map.value("self").toBool() && map.contains("role")) {
            return static_cast<PeerRole::Role>(map.value("role").toInt());
        }
    }
    return PeerRole::Role::Unknown;
}

/** As toxcore decides. */
bool QmlChat::canSetTopic() const
{
    const PeerRole::Role role = getSelfRole();
    return kind == ChatKind::Kind::Group && role != PeerRole::Role::Observer
           && (!state.topicLocked || moderates(role));
}

/** As toxcore decides. */
bool QmlChat::canWrite() const
{
    if (kind != ChatKind::Kind::Group) {
        return true;
    }
    const PeerRole::Role role = getSelfRole();
    if (role == PeerRole::Role::Observer) {
        return false;
    }
    // Private messages don't need the group's voice.
    if (!privateTo.isEmpty()) {
        return true;
    }
    switch (state.voiceState) {
    case VoiceState::State::All:
        return true;
    case VoiceState::State::Moderators:
        return moderates(role);
    case VoiceState::State::Founder:
        return role == PeerRole::Role::Founder;
    }
    return true;
}

QString QmlChat::getPrivateTo() const
{
    return privateTo;
}

void QmlChat::setPrivateTo(const QString& publicKey)
{
    if (kind == ChatKind::Kind::Group && publicKey != privateTo) {
        privateTo = publicKey;
        emit changed();
    }
}

QString QmlChat::getPrivateToName() const
{
    return findPeer(state.peers, privateTo).value("name").toString();
}

QAbstractItemModel* QmlChat::getMessages()
{
    return messages.get();
}

void QmlChat::sendMessage(const QString& text)
{
    if (!privateTo.isEmpty()) {
        service.sendGroupPrivateMessage(id, privateTo, text);
        return;
    }
    service.sendMessage(static_cast<int>(kind), id, text);
}

void QmlChat::find(const QString& phrase, int fromRow, bool backward)
{
    auto* watcher =
        new QRemoteObjectPendingCallWatcher(service.findMessage(static_cast<int>(kind), id, phrase,
                                                                fromRow, backward),
                                            this);
    connect(watcher, &QRemoteObjectPendingCallWatcher::finished, this,
            [this](QRemoteObjectPendingCallWatcher* call) {
                call->deleteLater();
                emit found(call->returnValue().isValid() ? call->returnValue().toInt() : -1);
            });
}

void QmlChat::setAlias(const QString& newAlias)
{
    if (kind == ChatKind::Kind::Friend) {
        service.setFriendAlias(id, newAlias.trimmed());
    }
}

void QmlChat::setAutoAcceptInvites(bool accept)
{
    if (kind == ChatKind::Kind::Friend) {
        service.setAutoAcceptInvites(id, accept);
    }
}

void QmlChat::setAutoAcceptFiles(bool accept)
{
    if (kind == ChatKind::Kind::Friend) {
        service.setAutoAcceptFiles(id, accept);
    }
}

void QmlChat::setNote(const QString& note)
{
    if (kind == ChatKind::Kind::Friend) {
        service.setFriendNote(id, note);
    }
}

void QmlChat::removeHistory()
{
    if (kind == ChatKind::Kind::Friend) {
        service.removeHistory(id);
    }
}

void QmlChat::setDraft(const QString& text)
{
    // As the Widgets UI's chat form: for a while, while there's text.
    const bool nowTyping = !text.isEmpty();
    if (kind != ChatKind::Kind::Friend || nowTyping == userTyping) {
        return;
    }
    userTyping = nowTyping;
    service.setTyping(id, nowTyping);
    if (nowTyping) {
        userTypingTimer.start();
    } else {
        userTypingTimer.stop();
    }
}

void QmlChat::setTitle(const QString& title)
{
    if (kind == ChatKind::Kind::Conference) {
        service.setConferenceTitle(id, title);
    }
}

void QmlChat::setTopic(const QString& topic)
{
    if (kind == ChatKind::Kind::Group) {
        service.setGroupTopic(id, topic);
    }
}

void QmlChat::setNickname(const QString& nickname)
{
    if (kind == ChatKind::Kind::Group) {
        service.setGroupNickname(id, nickname);
    }
}

void QmlChat::setPassword(const QString& password)
{
    if (kind == ChatKind::Kind::Group) {
        service.setGroupPassword(id, password);
    }
}

void QmlChat::setPrivate(bool isPrivate)
{
    if (kind == ChatKind::Kind::Group) {
        service.setGroupPrivate(id, isPrivate);
    }
}

void QmlChat::setPeerLimit(int peerLimit)
{
    if (kind == ChatKind::Kind::Group) {
        service.setGroupPeerLimit(id, peerLimit);
    }
}

void QmlChat::setTopicLocked(bool locked)
{
    if (kind == ChatKind::Kind::Group) {
        service.setGroupTopicLocked(id, locked);
    }
}

void QmlChat::setVoiceState(VoiceState::State voiceState)
{
    if (kind == ChatKind::Kind::Group) {
        service.setGroupVoiceState(id, static_cast<int>(voiceState));
    }
}

/** As toxcore decides: the founder moderates everyone else, moderators those below them. */
bool QmlChat::canModerate(PeerRole::Role role) const
{
    switch (kind == ChatKind::Kind::Group ? getSelfRole() : PeerRole::Role::Unknown) {
    case PeerRole::Role::Founder:
        return role != PeerRole::Role::Founder;
    case PeerRole::Role::Moderator:
        return role == PeerRole::Role::User || role == PeerRole::Role::Observer;
    default:
        return false;
    }
}

/** As toxcore decides: the roles the user may give are those of the peers they moderate. */
bool QmlChat::canGiveRole(PeerRole::Role current, PeerRole::Role role) const
{
    return current != role && canModerate(current) && canModerate(role);
}

void QmlChat::setPeerRole(const QString& publicKey, PeerRole::Role role)
{
    if (kind == ChatKind::Kind::Group) {
        service.setGroupPeerRole(id, publicKey, static_cast<int>(role));
    }
}

void QmlChat::kick(const QString& publicKey)
{
    if (kind == ChatKind::Kind::Group) {
        service.kickGroupPeer(id, publicKey);
    }
}

void QmlChat::invite(const QString& publicKey)
{
    switch (kind) {
    case ChatKind::Kind::Friend:
        return;
    case ChatKind::Kind::Conference:
        service.inviteToConference(id, publicKey);
        return;
    case ChatKind::Kind::Group:
        service.inviteToGroup(id, publicKey);
        return;
    }
}

void QmlChat::acceptFile(int fileNum)
{
    if (kind == ChatKind::Kind::Friend) {
        service.acceptFile(id, fileNum);
    }
}

void QmlChat::cancelFile(int fileNum, bool sending)
{
    if (kind == ChatKind::Kind::Friend) {
        service.cancelFile(id, fileNum, sending);
    }
}

void QmlChat::pauseFile(int fileNum)
{
    if (kind == ChatKind::Kind::Friend) {
        service.pauseFile(id, fileNum);
    }
}

void QmlChat::sendFile(const QUrl& file)
{
    if (kind != ChatKind::Kind::Friend) {
        return;
    }
    if (file.isLocalFile()) {
        service.sendFile(id, file.toLocalFile());
        return;
    }

    // The service may not be allowed to read a content: URL, or not for long: it sends a copy.
    const QString source = file.toString();
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/sending";
    const QString copy = Files::uniquePath(dir, Files::name(file));
    auto copied = std::make_shared<bool>(false);
    QThread* copier = QThread::create(
        [source, dir, copy, copied] { *copied = QDir{}.mkpath(dir) && QFile::copy(source, copy); });
    connect(copier, &QThread::finished, copier, &QObject::deleteLater);
    connect(copier, &QThread::finished, this, [this, copy, copied] {
        if (*copied) {
            service.sendFile(id, copy);
        } else {
            emit error(tr("Couldn't send the file"), tr("Unable to open this file."));
        }
    });
    copier->start();
}

void QmlChat::update()
{
    const QHash<int, QByteArray> roles = chats.roleNames();
    const int kindRole = roles.key("kind", -1);
    const int idRole = roles.key("chatId", -1);
    const int nameRole = roles.key("name", -1);
    if (kindRole < 0 || idRole < 0 || nameRole < 0) {
        return;
    }

    for (int row = 0; row < chats.rowCount(); ++row) {
        const QModelIndex index = chats.index(row, 0);
        if (chats.data(index, kindRole).toInt() != static_cast<int>(kind)
            || chats.data(index, idRole).toString() != id) {
            continue;
        }
        const auto value = [&](const char* role) { return chats.data(index, roles.key(role)); };
        const QString newName = chats.data(index, nameRole).toString();
        // Until the replica has it.
        const QVariant status = value("status");
        const State newState{
            // A friend without a name keeps showing their key.
            .name = newName.isEmpty() ? state.name : newName,
            .userName = value("userName").toString(),
            .alias = value("alias").toString(),
            .statusMessage = value("statusMessage").toString(),
            .online = value("online").toBool(),
            .status = status.isValid() ? status.toInt() : static_cast<int>(Status::Status::Offline),
            .avatar = AvatarProvider::source(kind, id, value("avatarHash").toString()),
            .autoAcceptInvites = value("autoAcceptInvites").toBool(),
            .autoAcceptFiles = value("autoAcceptFiles").toBool(),
            .typing = value("typing").toBool(),
            .note = value("note").toString(),
            .peers = value("peers").toList(),
            .audio = value("audio").toBool(),
            .topic = value("topic").toString(),
            .nickname = value("nickname").toString(),
            .passwordSet = value("passwordSet").toBool(),
            .isPrivate = value("isPrivate").toBool(),
            .peerLimit = value("peerLimit").toInt(),
            .topicLocked = value("topicLocked").toBool(),
            .voiceState = static_cast<VoiceState::State>(value("voiceState").toInt()),
        };
        if (newState != state) {
            state = newState;
            // A peer who left gets no more private messages.
            if (findPeer(state.peers, privateTo).isEmpty()) {
                privateTo.clear();
            }
            emit changed();
        }
        return;
    }
}
