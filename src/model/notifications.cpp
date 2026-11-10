/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "notifications.h"

#include "src/conferencelist.h"
#include "src/core/toxfile.h"
#include "src/core/toxpk.h"
#include "src/friendlist.h"
#include "src/grouplist.h"
#include "src/model/chatmanager.h"
#include "src/model/conference.h"
#include "src/model/destination.h"
#include "src/model/filemanager.h"
#include "src/model/friend.h"
#include "src/model/friendrequestmanager.h"
#include "src/model/group.h"
#include "src/model/imessagedispatcher.h"
#include "src/model/message.h"
#include "src/model/notificationdata.h"
#include "src/model/notificationgenerator.h"
#include "src/model/textformatter.h"
#include "src/persistence/inotificationsettings.h"

#include <variant>

Notifications::Notifications(INotifier& notifier_, const INotificationSettings& settings_,
                             NotificationGenerator& generator_, ChatManager& chatManager_,
                             CallManager& callManager_, InviteManager& inviteManager,
                             FriendRequestManager& friendRequestManager, FileManager& fileManager,
                             FriendList& friendList_, ConferenceList& conferenceList_,
                             GroupList& groupList_, InFront inFront_)
    : notifier{notifier_}
    , settings{settings_}
    , generator{generator_}
    , chatManager{chatManager_}
    , callManager{callManager_}
    , friendList{friendList_}
    , conferenceList{conferenceList_}
    , groupList{groupList_}
    , inFront{std::move(inFront_)}
{
    notifier.setListener(this);

    connect(&chatManager, &ChatManager::messageReceived, this, &Notifications::onMessageReceived);
    connect(&callManager, &CallManager::stateChanged, this, &Notifications::onCallStateChanged);
    connect(&fileManager, &FileManager::offered, this, &Notifications::onFileOffered);
    connect(&inviteManager, &InviteManager::added, this, &Notifications::onInviteAdded);
    connect(&friendRequestManager, &FriendRequestManager::received, this,
            &Notifications::onFriendRequestReceived);

    // Nothing to read in a chat that's gone.
    connect(&chatManager, &ChatManager::friendRemoved, this, [this](const ToxPk& friendPk) {
        cancel(Destination::chat(ChatKind::Kind::Friend, friendPk.toString()));
    });
    connect(&chatManager, &ChatManager::conferenceRemoved, this,
            [this](const ConferenceId& conferenceId) {
                cancel(Destination::chat(ChatKind::Kind::Conference, conferenceId.toString()));
            });
    connect(&chatManager, &ChatManager::groupRemoved, this, [this](const GroupId& groupId) {
        cancel(Destination::chat(ChatKind::Kind::Group, groupId.toString()));
    });
}

Notifications::~Notifications()
{
    notifier.setListener(nullptr);
    // They're about the user who logs out.
    for (const QString& tag : std::as_const(shownTags)) {
        notifier.cancel(tag);
    }
}

void Notifications::shown(const QString& destination)
{
    cancel(destination);
    // Its message counts start again.
    generator.onNotificationActivated();
}

void Notifications::replied(const QString& tag, const QString& text)
{
    Chat* chat = findChat(tag);
    IMessageDispatcher* dispatcher = chat != nullptr ? chatManager.getDispatcher(*chat) : nullptr;
    const std::optional<Message> message = Message::typed(text);
    if (dispatcher == nullptr || !message) {
        return;
    }
    dispatcher->sendMessage(message->isAction, message->content);
    cancel(tag);
    emit answered(*chat);
}

void Notifications::declined(const QString& tag)
{
    if (const auto* f = qobject_cast<const Friend*>(findChat(tag))) {
        callManager.end(f->getPublicKey());
    }
}

void Notifications::opened()
{
    generator.onNotificationActivated();
}

void Notifications::onMessageReceived(Chat& chat, const ToxPk& sender, const Message& message,
                                      bool notify)
{
    const QString tag = Destination::chat(chat);
    if (const auto* f = qobject_cast<const Friend*>(&chat)) {
        show(tag, &chat, generator.friendMessageNotification(f, message.content),
             INotifier::Actions::Reply);
    } else if (!notify) {
        // Conferences and groups notify about mentions and private messages only, by default.
    } else if (const auto* conference = qobject_cast<const Conference*>(&chat)) {
        show(tag, &chat, generator.conferenceMessageNotification(conference, sender, message.content),
             INotifier::Actions::Reply);
    } else if (const auto* group = qobject_cast<const Group*>(&chat)) {
        // A reply from the notification would go to everyone.
        show(tag, &chat, generator.groupMessageNotification(group, sender, message.content),
             message.recipient.isEmpty() ? INotifier::Actions::Reply : INotifier::Actions::None);
    }
}

void Notifications::onCallStateChanged(const ToxPk& friendPk, CallManager::State state)
{
    Friend* f = friendList.findFriend(friendPk);
    if (f == nullptr) {
        return;
    }
    const QString tag = Destination::chat(*f);
    if (state == CallManager::State::Incoming) {
        show(tag, f, generator.incomingCallNotification(f, callManager.hasVideo(friendPk)),
             INotifier::Actions::AnswerOrDecline);
    } else if (state != CallManager::State::Outgoing) {
        // Answered or over.
        cancel(tag);
    }
}

void Notifications::onFileOffered(const ToxFile& file, const ToxPk& friendPk)
{
    Friend* f = friendList.findFriend(friendPk);
    if (f != nullptr) {
        show(Destination::chat(*f), f,
             generator.fileTransferNotification(f, file.fileName, file.progress.getFileSize()));
    }
}

void Notifications::onInviteAdded(const InviteManager::Invite& invite, const ToxPk& friendPk)
{
    const Friend* f = friendList.findFriend(friendPk);
    if (f != nullptr) {
        show(Destination::invites, nullptr,
             std::holds_alternative<ConferenceInvite>(invite)
                 ? generator.conferenceInvitationNotification(f)
                 : generator.groupInvitationNotification(f));
    }
}

void Notifications::onFriendRequestReceived(const ToxPk& friendPk, const QString& message)
{
    show(Destination::friendRequests, nullptr, generator.friendRequestNotification(friendPk, message));
}

void Notifications::show(const QString& tag, const Chat* chat, NotificationData data,
                         INotifier::Actions actions)
{
    if (!(settings.getNotify() && settings.getDesktopNotify()) || (chat != nullptr && inFront(*chat))) {
        return;
    }

    if (settings.getHidePostNullSuffix()) {
        data.message = TextFormatter::processPostNullSuffix(data.message, false);
    }
    notifier.show(tag, data, actions);
    shownTags.insert(tag);
}

void Notifications::cancel(const QString& tag)
{
    if (shownTags.remove(tag)) {
        notifier.cancel(tag);
    }
}

Chat* Notifications::findChat(const QString& destination) const
{
    return Destination::findChat(destination, friendList, conferenceList, groupList);
}
