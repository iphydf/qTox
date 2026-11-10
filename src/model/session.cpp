/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "session.h"

#include "audio/audio.h"
#include "audio/iaudiocontrol.h"
#include "src/core/core.h"
#include "src/core/coreav.h"
#include "src/model/callmanager.h"
#include "src/model/chatmanager.h"
#include "src/model/filemanager.h"
#include "src/model/friendrequestmanager.h"
#include "src/model/invitemanager.h"
#include "src/model/notificationgenerator.h"
#include "src/model/profile/profileinfo.h"
#include "src/persistence/profile.h"
#include "src/persistence/profilelocker.h"
#include "src/persistence/settings.h"

Session::LoginResult Session::create(const QString& name, const QString& password, Settings& settings,
                                     CameraSource& cameraSource, IMessageBoxManager& messageBoxManager,
                                     const QCommandLineParser* parser)
{
    if (Profile::exists(name, settings.getPaths())) {
        return {nullptr, LoginError::AlreadyExists};
    }
    std::unique_ptr<Profile> profile =
        Profile::createProfile(name, password, settings, parser, cameraSource, messageBoxManager);
    if (profile == nullptr) {
        return {nullptr, LoginError::NotWritable};
    }
    return {std::make_unique<Session>(std::move(profile), settings), LoginError::None};
}

Session::LoginResult Session::load(const QString& name, const QString& password, Settings& settings,
                                   CameraSource& cameraSource, IMessageBoxManager& messageBoxManager,
                                   const QCommandLineParser* parser)
{
    Paths& paths = settings.getPaths();
    // By another qTox, or by a profile that this one has loaded: there is one lock.
    if (ProfileLocker::hasLock() || !ProfileLocker::isLockable(name, paths)) {
        return {nullptr, LoginError::InUse};
    }
    std::unique_ptr<Profile> profile =
        Profile::loadProfile(name, password, settings, parser, cameraSource, messageBoxManager);
    if (profile == nullptr) {
        // Profile doesn't say why. With a password, a wrong one is the likely reason.
        return {nullptr, Profile::isEncrypted(name, paths) ? LoginError::WrongPassword
                                                           : LoginError::Unreadable};
    }
    return {std::make_unique<Session>(std::move(profile), settings), LoginError::None};
}

Session::Session(std::unique_ptr<Profile> profile_, Settings& settings, QObject* parent)
    : QObject{parent}
    , audio{Audio::makeAudio(settings)}
    , profile{std::move(profile_)}
{
    profile->getCore().getAv()->setAudio(*audio);

    profileInfo = std::make_unique<ProfileInfo>(&profile->getCore(), profile.get(), settings);
    chatManager = std::make_unique<ChatManager>(*profile, settings, friendList, conferenceList,
                                                groupList, nullptr);
    notificationGenerator = std::make_unique<NotificationGenerator>(settings, profile.get());
    callManager = std::make_unique<CallManager>(friendList, *chatManager, settings, *audio);
    inviteManager = std::make_unique<InviteManager>(friendList, settings);
    fileManager = std::make_unique<FileManager>(friendList, *chatManager, settings);
    friendRequestManager = std::make_unique<FriendRequestManager>(settings);

    // Before any user interface hears of the Core: they connect after us.
    connect(profile.get(), &Profile::coreChanged, this, &Session::onCoreChanged);
}

Session::~Session()
{
    // The chats and the lists refer to what the chat manager and the profile own, and a call's
    // sound uses the audio, which goes last.
    notificationGenerator.reset();
    callManager.reset();
    inviteManager.reset();
    fileManager.reset();
    friendRequestManager.reset();
    chatManager.reset();
    friendList.clear();
    conferenceList.clear();
    groupList.clear();
    profileInfo.reset();
    profile->save();
    profile.reset();
    audio.reset();
}

void Session::setDialogsManager(IDialogsManager* dialogsManager)
{
    chatManager->setDialogsManager(dialogsManager);
}

void Session::start()
{
    profile->startCore();
}

Profile& Session::getProfile()
{
    return *profile;
}

ProfileInfo& Session::getProfileInfo()
{
    return *profileInfo;
}

IAudioControl& Session::getAudio()
{
    return *audio;
}

FriendList& Session::getFriendList()
{
    return friendList;
}

ConferenceList& Session::getConferenceList()
{
    return conferenceList;
}

GroupList& Session::getGroupList()
{
    return groupList;
}

ChatManager& Session::getChatManager()
{
    return *chatManager;
}

CallManager& Session::getCallManager()
{
    return *callManager;
}

InviteManager& Session::getInviteManager()
{
    return *inviteManager;
}

FileManager& Session::getFileManager()
{
    return *fileManager;
}

FriendRequestManager& Session::getFriendRequestManager()
{
    return *friendRequestManager;
}

NotificationGenerator& Session::getNotificationGenerator()
{
    return *notificationGenerator;
}

void Session::onCoreChanged(Core& core)
{
    // The call manager first: a friend who goes offline ends the call there before the chat's
    // users hear of the new status.
    callManager->connectToCore(core);
    inviteManager->connectToCore(core);
    fileManager->connectToCore(core);
    friendRequestManager->connectToCore(core);
    chatManager->connectToCore(core);
}
