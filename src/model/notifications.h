/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/callmanager.h"
#include "src/model/inotifier.h"
#include "src/model/invitemanager.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

class Chat;
class ChatManager;
class ConferenceList;
class FileManager;
class FriendList;
class FriendRequestManager;
class GroupList;
class INotificationSettings;
class NotificationGenerator;
class ToxPk;
struct Message;
struct NotificationData;
struct ToxFile;

/**
 * @brief Tells the user, in the system's notifications, what comes in while they look elsewhere:
 * messages, calls, files, invites and friend requests.
 *
 * Each notification is tagged with the Destination it's about, so a newer one replaces it, and it
 * goes once the user looks at that. The user interfaces only say what the user is looking at.
 */
class Notifications : public QObject, public INotifier::IListener
{
    Q_OBJECT

public:
    /** Whether the user is looking at a chat now. */
    using InFront = std::function<bool(const Chat& chat)>;

    Notifications(INotifier& notifier, const INotificationSettings& settings,
                  NotificationGenerator& generator, ChatManager& chatManager, CallManager& callManager,
                  InviteManager& inviteManager, FriendRequestManager& friendRequestManager,
                  FileManager& fileManager, FriendList& friendList, ConferenceList& conferenceList,
                  GroupList& groupList, InFront inFront);
    ~Notifications() override;
    Notifications(const Notifications&) = delete;
    Notifications& operator=(const Notifications&) = delete;
    Notifications(Notifications&&) = delete;
    Notifications& operator=(Notifications&&) = delete;

    /** The user is looking at @p destination now: what's shown about it goes. */
    void shown(const QString& destination);

    void replied(const QString& tag, const QString& text) override;
    void declined(const QString& tag) override;
    void opened() override;

signals:
    /** The user answered a chat's notification, so they've read the chat. */
    void answered(const Chat& chat);

private:
    void onMessageReceived(Chat& chat, const ToxPk& sender, const Message& message, bool notify);
    void onCallStateChanged(const ToxPk& friendPk, CallManager::State state);
    void onFileOffered(const ToxFile& file, const ToxPk& friendPk);
    void onInviteAdded(const InviteManager::Invite& invite, const ToxPk& friendPk);
    void onFriendRequestReceived(const ToxPk& friendPk, const QString& message);
    /** Shows @p data, unless the user is looking at @p chat or turned notifications off. */
    void show(const QString& tag, const Chat* chat, NotificationData data,
              INotifier::Actions actions = INotifier::Actions::None);
    void cancel(const QString& tag);
    Chat* findChat(const QString& destination) const;

    INotifier& notifier;
    const INotificationSettings& settings;
    NotificationGenerator& generator;
    ChatManager& chatManager;
    CallManager& callManager;
    FriendList& friendList;
    ConferenceList& conferenceList;
    GroupList& groupList;
    InFront inFront;
    // By tag.
    QSet<QString> shownTags;
};
