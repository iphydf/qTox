/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/conferencelist.h"
#include "src/friendlist.h"
#include "src/grouplist.h"

#include <QObject>

#include <memory>

class CallManager;
class CameraSource;
class ChatManager;
class Core;
class FileManager;
class FriendRequestManager;
class IAudioControl;
class IDialogsManager;
class IMessageBoxManager;
class InviteManager;
class NotificationGenerator;
class Profile;
class ProfileInfo;
class QCommandLineParser;
class Settings;

/**
 * @brief What there is for as long as a user is logged in: their profile, its chats, and what
 * handles calls, invites and files. It builds them, connects them to the Core and takes them
 * apart in the one order that works. A user interface shows a session.
 */
class Session : public QObject
{
    Q_OBJECT

public:
    enum class LoginError
    {
        None,
        /** A new profile can't have the name of one that is there. */
        AlreadyExists,
        /** This program or another has the profile open. */
        InUse,
        WrongPassword,
        /** The Tox save file of a profile without a password couldn't be read. */
        Unreadable,
        /** A new profile's Tox save file couldn't be written. */
        NotWritable,
    };
    struct LoginResult
    {
        // Nothing if there is an error.
        std::unique_ptr<Session> session;
        LoginError error;
    };

    /** Makes a new profile and logs in to it. @param parser The command line's proxy settings. */
    static LoginResult create(const QString& name, const QString& password, Settings& settings,
                              CameraSource& cameraSource, IMessageBoxManager& messageBoxManager,
                              const QCommandLineParser* parser = nullptr);
    /** Logs in to a profile that is there. */
    static LoginResult load(const QString& name, const QString& password, Settings& settings,
                            CameraSource& cameraSource, IMessageBoxManager& messageBoxManager,
                            const QCommandLineParser* parser = nullptr);

    Session(std::unique_ptr<Profile> profile, Settings& settings, QObject* parent = nullptr);
    ~Session() override;

    /** Who asks the user about a chat. Set it before start(). */
    void setDialogsManager(IDialogsManager* dialogsManager);
    /** Starts the Core: from then on, friends and chats appear. */
    void start();

    Profile& getProfile();
    ProfileInfo& getProfileInfo();
    IAudioControl& getAudio();
    FriendList& getFriendList();
    ConferenceList& getConferenceList();
    GroupList& getGroupList();
    ChatManager& getChatManager();
    CallManager& getCallManager();
    InviteManager& getInviteManager();
    FileManager& getFileManager();
    FriendRequestManager& getFriendRequestManager();
    NotificationGenerator& getNotificationGenerator();

private:
    void onCoreChanged(Core& core);

    // The profile's Core plays and records through it.
    std::unique_ptr<IAudioControl> audio;
    std::unique_ptr<Profile> profile;
    FriendList friendList;
    ConferenceList conferenceList;
    GroupList groupList;
    std::unique_ptr<ProfileInfo> profileInfo;
    std::unique_ptr<ChatManager> chatManager;
    std::unique_ptr<NotificationGenerator> notificationGenerator;
    std::unique_ptr<CallManager> callManager;
    std::unique_ptr<InviteManager> inviteManager;
    std::unique_ptr<FileManager> fileManager;
    std::unique_ptr<FriendRequestManager> friendRequestManager;
};
