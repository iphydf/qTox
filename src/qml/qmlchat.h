/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/status.h"
#include "src/qml/chatkind.h"
#include "src/qml/peerrole.h"
#include "src/qml/voicestate.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <QtQml/qqmlregistration.h>
#include <memory>

class QAbstractItemModel;
class QAbstractItemModelReplica;
class QRemoteObjectNode;
class ToxServiceReplica;

/**
 * @brief One chat, as QML shows it.
 */
class QmlChat : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(ChatKind::Kind kind READ getKind CONSTANT)
    Q_PROPERTY(QString name READ getName NOTIFY changed)
    /** Friends only: their own name, the user's alias for them, and how they are. */
    Q_PROPERTY(QString userName READ getUserName NOTIFY changed)
    Q_PROPERTY(QString alias READ getAlias NOTIFY changed)
    Q_PROPERTY(QString statusMessage READ getStatusMessage NOTIFY changed)
    Q_PROPERTY(bool online READ isOnline NOTIFY changed)
    /** A UserStatus::Status. */
    Q_PROPERTY(int status READ getStatus NOTIFY changed)
    /** An image source. */
    Q_PROPERTY(QString avatar READ getAvatar NOTIFY changed)
    /** Friends only: whether qTox joins the conferences and groups they invite the user to. */
    Q_PROPERTY(bool autoAcceptInvites READ getAutoAcceptInvites NOTIFY changed)
    /** Friends only: whether qTox saves the files they send without asking. */
    Q_PROPERTY(bool autoAcceptFiles READ getAutoAcceptFiles NOTIFY changed)
    /** Friends only: whether they're writing to the user. */
    Q_PROPERTY(bool typing READ isTyping NOTIFY changed)
    /** Friends only: what the user notes about them. */
    Q_PROPERTY(QString note READ getNote NOTIFY changed)
    /**
     * Conferences and groups: who's there, the user too, as {name, publicKey, self}, by name, and
     * in groups also {role}, a PeerRole::Role. A group's public keys are its own.
     */
    Q_PROPERTY(QVariantList peers READ getPeers NOTIFY changed)
    /** Conferences only: whether they have calls. */
    Q_PROPERTY(bool audio READ hasAudio NOTIFY changed)
    /** Groups only: what the group is about, and the user's name there, empty for the profile's. */
    Q_PROPERTY(QString topic READ getTopic NOTIFY changed)
    Q_PROPERTY(QString nickname READ getNickname NOTIFY changed)
    /** Groups only: what the founder decided. */
    Q_PROPERTY(bool passwordSet READ isPasswordSet NOTIFY changed)
    Q_PROPERTY(bool isPrivate READ isPrivate NOTIFY changed)
    Q_PROPERTY(int peerLimit READ getPeerLimit NOTIFY changed)
    Q_PROPERTY(bool topicLocked READ isTopicLocked NOTIFY changed)
    Q_PROPERTY(VoiceState::State voiceState READ getVoiceState NOTIFY changed)
    /** Groups only: the user's role there, and what it lets them do: writing to privateTo too. */
    Q_PROPERTY(PeerRole::Role selfRole READ getSelfRole NOTIFY changed)
    Q_PROPERTY(bool canSetTopic READ canSetTopic NOTIFY changed)
    Q_PROPERTY(bool canWrite READ canWrite NOTIFY changed)
    /** Groups only: the peer whom sendMessage() writes to alone, by their key there, if any. */
    Q_PROPERTY(QString privateTo READ getPrivateTo WRITE setPrivateTo NOTIFY changed)
    Q_PROPERTY(QString privateToName READ getPrivateToName NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* messages READ getMessages CONSTANT)

public:
    /** @param chats The service's chat list, where the chat's name and the rest come from. */
    QmlChat(ChatKind::Kind kind, QString id, ToxServiceReplica& service, QRemoteObjectNode& node,
            QAbstractItemModel& chats, QObject* parent = nullptr);
    ~QmlChat() override;

    ChatKind::Kind getKind() const;
    QString getName() const;
    QString getUserName() const;
    QString getAlias() const;
    QString getStatusMessage() const;
    bool isOnline() const;
    int getStatus() const;
    QString getAvatar() const;
    bool getAutoAcceptInvites() const;
    bool getAutoAcceptFiles() const;
    bool isTyping() const;
    QString getNote() const;
    QVariantList getPeers() const;
    bool hasAudio() const;
    QString getTopic() const;
    QString getNickname() const;
    bool isPasswordSet() const;
    bool isPrivate() const;
    int getPeerLimit() const;
    bool isTopicLocked() const;
    VoiceState::State getVoiceState() const;
    PeerRole::Role getSelfRole() const;
    bool canSetTopic() const;
    bool canWrite() const;
    QString getPrivateTo() const;
    void setPrivateTo(const QString& publicKey);
    QString getPrivateToName() const;
    QAbstractItemModel* getMessages();

    Q_INVOKABLE void sendMessage(const QString& text);
    /** Looks for the next message with @p phrase from @p fromRow on: found() says where. */
    Q_INVOKABLE void find(const QString& phrase, int fromRow, bool backward);
    /** A friend's: empty for none. */
    Q_INVOKABLE void setAlias(const QString& alias);
    Q_INVOKABLE void setAutoAcceptInvites(bool accept);
    Q_INVOKABLE void setAutoAcceptFiles(bool accept);
    Q_INVOKABLE void setNote(const QString& note);
    /** A friend's: deletes the chat history and clears the chat. */
    Q_INVOKABLE void removeHistory();
    /** What the user is writing to a friend, who sees whether they are. */
    Q_INVOKABLE void setDraft(const QString& text);
    /** A conference's. */
    Q_INVOKABLE void setTitle(const QString& title);
    /** A group's. */
    Q_INVOKABLE void setTopic(const QString& topic);
    Q_INVOKABLE void setNickname(const QString& nickname);
    /** A group's, which only its founder can change: an empty password removes it. */
    Q_INVOKABLE void setPassword(const QString& password);
    Q_INVOKABLE void setPrivate(bool isPrivate);
    Q_INVOKABLE void setPeerLimit(int peerLimit);
    Q_INVOKABLE void setTopicLocked(bool locked);
    Q_INVOKABLE void setVoiceState(VoiceState::State voiceState);
    /** Whether the user may change the role of a peer in @p role, and kick them. */
    Q_INVOKABLE bool canModerate(PeerRole::Role role) const;
    /** Whether the user may give a peer in role @p current role @p role. */
    Q_INVOKABLE bool canGiveRole(PeerRole::Role current, PeerRole::Role role) const;
    Q_INVOKABLE void setPeerRole(const QString& publicKey, PeerRole::Role role);
    Q_INVOKABLE void kick(const QString& publicKey);
    /** Into a conference or group: the friend must be online. */
    Q_INVOKABLE void invite(const QString& publicKey);
    /** A friend's file, by its messages' fileNum: the service puts it with the received files. */
    Q_INVOKABLE void acceptFile(int fileNum);
    Q_INVOKABLE void cancelFile(int fileNum, bool sending);
    /** Pauses the user's side of a friend's file transfer, or resumes it. */
    Q_INVOKABLE void pauseFile(int fileNum);
    /** Sends a friend a file, or a copy of what Android's picker gives. */
    Q_INVOKABLE void sendFile(const QUrl& file);

signals:
    void changed();
    /** The messages' row that find() found, or -1. */
    void found(int row);
    void error(const QString& title, const QString& text);

private:
    void update();

    const ChatKind::Kind kind;
    const QString id;
    ToxServiceReplica& service;
    QAbstractItemModel& chats;
    std::unique_ptr<QAbstractItemModelReplica> messages;
    // What the chat list says of the chat.
    struct State
    {
        QString name;
        QString userName;
        QString alias;
        QString statusMessage;
        bool online{false};
        int status{static_cast<int>(Status::Status::Offline)};
        QString avatar;
        bool autoAcceptInvites{false};
        bool autoAcceptFiles{false};
        bool typing{false};
        QString note;
        QVariantList peers;
        bool audio{false};
        QString topic;
        QString nickname;
        bool passwordSet{false};
        bool isPrivate{false};
        int peerLimit{0};
        bool topicLocked{false};
        VoiceState::State voiceState{VoiceState::State::All};

        bool operator==(const State& other) const = default;
    };
    State state;
    QString privateTo;
    // Whether the friend sees the user typing, which they stop seeing after a while.
    bool userTyping{false};
    QTimer userTypingTimer;
};
