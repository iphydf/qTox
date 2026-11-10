/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "rep_toxservice_source.h"

#include "audio/iaudiosink.h"
#include "src/conferencelist.h"
#include "src/core/core.h"
#include "src/core/toxfile.h"
#include "src/friendlist.h"
#include "src/grouplist.h"
#include "src/model/callmanager.h"
#include "src/model/chatkind.h"
#include "src/model/filemanager.h"
#include "src/model/friendrequestmanager.h"
#include "src/model/ichatlog.h"
#include "src/model/inotifier.h"
#include "src/model/invitemanager.h"
#include "src/model/notifications.h"
#include "src/model/session.h"
#include "src/model/status.h"
#include "src/model/systemmessage.h"
#include "src/qml/callstate.h"
#include "src/qml/chatlistmodel.h"
#include "src/qml/friendrequestmodel.h"
#include "src/qml/invitemodel.h"
#include "src/qml/qmldialogsmanager.h"

#include <QHash>
#include <QSet>

#include <chrono>
#include <functional>
#include <memory>

class CameraSource;
class Chat;
class ChatManager;
class ChatModel;
class ConferenceInvite;
class Group;
class GroupId;
class GroupInvite;
class IAudioControl;
class IMessageBoxManager;
class IMessageDispatcher;
class Profile;
class ProfileInfo;
class QRemoteObjectHostBase;
class Settings;
class ToxPk;
class VideoFrame;
class VideoSource;

/**
 * @brief Owns the profile and its Tox connection, and shares them with the user interface.
 *
 * On Android it runs in its own process, so Tox stays connected while the user interface is
 * closed.
 */
class ToxService : public ToxServiceSimpleSource
{
    Q_OBJECT

public:
    ToxService(Settings& settings, IMessageBoxManager& messageBoxManager, CameraSource& cameraSource,
               INotifier& notifier, QRemoteObjectHostBase& host, QObject* parent = nullptr);
    ~ToxService() override;

    void createProfile(const QString& name, const QString& password, bool autoLogin) override;
    void loadProfile(const QString& name, const QString& password, bool autoLogin) override;
    void logout() override;
    void setPassword(const QString& password) override;
    void renameProfile(const QString& name) override;
    void deleteProfile() override;
    QByteArray exportProfile() override;
    void importProfile(const QString& name, const QByteArray& save) override;
    void changeName(const QString& name) override;
    void changeStatusMessage(const QString& message) override;
    void changeStatus(int status) override;
    void newNospam() override;
    void changeAvatar(const QByteArray& avatar) override;
    void removeAvatar() override;
    void addFriend(const QString& toxId, const QString& message) override;
    void acceptFriendRequest(const QString& publicKey) override;
    void rejectFriendRequest(const QString& publicKey) override;
    void setFriendAlias(const QString& publicKey, const QString& alias) override;
    void removeFriend(const QString& publicKey, bool removeHistory) override;
    void removeHistory(const QString& publicKey) override;
    void setFriendNote(const QString& publicKey, const QString& note) override;
    void acceptInvite(const QString& id, const QString& password) override;
    void rejectInvite(const QString& id) override;
    void openChat(int kind, const QString& id) override;
    void sendMessage(int kind, const QString& id, const QString& text) override;
    int findMessage(int kind, const QString& id, const QString& phrase, int fromRow,
                    bool backward) override;
    void acceptFile(const QString& publicKey, int fileNum) override;
    void cancelFile(const QString& publicKey, int fileNum, bool sending) override;
    void pauseFile(const QString& publicKey, int fileNum) override;
    void sendFile(const QString& publicKey, const QString& path) override;
    QString createConference(const QString& title) override;
    void inviteToConference(const QString& id, const QString& publicKey) override;
    void setConferenceTitle(const QString& id, const QString& title) override;
    QString createGroup(const QString& name) override;
    QString joinGroup(const QString& id, const QString& password) override;
    void inviteToGroup(const QString& id, const QString& publicKey) override;
    void setGroupTopic(const QString& id, const QString& topic) override;
    void setGroupNickname(const QString& id, const QString& nickname) override;
    void setGroupPassword(const QString& id, const QString& password) override;
    void setGroupPrivate(const QString& id, bool isPrivate) override;
    void setGroupPeerLimit(const QString& id, int peerLimit) override;
    void setGroupTopicLocked(const QString& id, bool locked) override;
    void setGroupVoiceState(const QString& id, int voiceState) override;
    void setGroupPeerRole(const QString& id, const QString& publicKey, int role) override;
    void kickGroupPeer(const QString& id, const QString& publicKey) override;
    void sendGroupPrivateMessage(const QString& id, const QString& publicKey,
                                 const QString& text) override;
    void startCall(const QString& publicKey, bool video) override;
    void answerCall(bool video) override;
    void joinConferenceCall(const QString& id) override;
    void endCall() override;
    void toggleCallMic() override;
    void toggleCallSpeaker() override;
    void toggleCallLoudspeaker() override;
    void switchCamera() override;
    void leaveConference(const QString& id) override;
    void leaveGroup(const QString& id) override;
    void setAutoAcceptInvites(const QString& publicKey, bool accept) override;
    void setAutoAcceptFiles(const QString& publicKey, bool accept) override;
    void setTyping(const QString& publicKey, bool typing) override;
    void eraseHistory() override;
    void setActiveChat(int kind, const QString& id) override;


private:
    void connectSettings();
    void readSettings();
    /** Logs in to the last profile if the user asked for that and it has no password. */
    void logInAutomatically();
    void closeProfile();
    void start(std::unique_ptr<Session> newSession, bool autoLogin);
    static QString describe(Session::LoginError loginError);

    // The session's, or nothing when nobody is logged in.
    Profile* profile() const;
    ProfileInfo* profileInfo() const;
    ChatManager* chatManager() const;
    CallManager* calls() const;
    InviteManager* inviteManager() const;
    FileManager* files() const;
    Friend* findFriend(const ToxPk& friendPk) const;
    Conference* findConference(const ConferenceId& id) const;
    Group* findGroup(const GroupId& id) const;
    void onCoreChanged(Core& newCore);
    void updateSelfAvatar();
    void onInviteAdded(const InviteManager::Invite& invite, const ToxPk& friendPk);
    void onMessageReceived(Chat& chat);
    /** Makes a change to a group, and says so if Tox refuses it. */
    void changeGroup(const QString& id, const std::function<bool(Group&)>& change);
    /**
     * @brief Removes a group and what refers to it: ChatManager's part with @p removeModel, which
     *        also quits the group or not.
     */
    void removeChat(ChatKind::Kind kind, const QString& id, bool leave);
    Chat* findChat(ChatKind::Kind kind, const QString& id) const;
    void onFriendStatusChanged(const Friend& f, Status::Status status);
    void onCallStateChanged(const ToxPk& friendPk, CallManager::State state);
    /** The friend or conference in the call, if any. */
    Friend* callingFriend();
    Conference* callingConference();
    void setCall(const Friend* f, CallState::State state);
    void setCall(ChatKind::Kind kind, const QString& id, CallState::State state);
    void clearCall();
    /** Shows the friend's video, if the call has it. */
    void watchCallVideo(const Friend& f);
    void stopCallVideo();
    void onCallVideoFrame(const std::shared_ptr<VideoFrame>& frame);
    /** Picks what a video call sends: @p device, else the camera the user had, else the first. */
    void chooseCamera(QString device = {});
    void setSendsVideo(bool sends);
    void onSelfVideoFrame(const std::shared_ptr<VideoFrame>& frame);
    /** Tells Android what the service records during the call. */
    void updateRecording();
    /** Plays a call's sound as the Widgets UI does: with @p loop, until the call changes. */
    QString receivedFilesDir() const;
    /** Where to save a friend's files without asking, or empty to ask. */
    IChatLog* findChatLog(ChatKind::Kind kind, const QString& id) const;
    /** The chat log's first item that the chat shows. */
    ChatLogIdx firstShown(ChatKind::Kind kind, const QString& id, const IChatLog& chatLog) const;
    IMessageDispatcher* findDispatcher(ChatKind::Kind kind, const QString& id) const;
    void closeChat(ChatKind::Kind kind, const QString& id);
    /** Counts a message that came in, unless the user is reading its chat. */
    void countUnread(ChatKind::Kind kind, const QString& id);
    QString describeStatus() const;
    /** Shows whether the service is logged in and online in its ongoing notification. */
    void updateStatus();
    /** Saves the profile's Tox data once the changes that come together are in. */
    void saveSoon();
    void updateProfiles();
    void updateFriendRequests();

    Settings& settings;
    IMessageBoxManager& messageBoxManager;
    CameraSource& cameraSource;
    INotifier& notifier;
    QRemoteObjectHostBase& host;
    QmlDialogsManager dialogsManager;
    ChatListModel chatList;
    FriendRequestModel friendRequests;
    InviteModel invites;
    // What there is while a user is logged in.
    std::unique_ptr<Session> session;
    // After the session it watches, so it goes first.
    std::unique_ptr<Notifications> notifications;
    // The call's ringing or end, if playing.
    // The friend's video in the call, if any, and when the user interface last got a frame of it.
    std::shared_ptr<VideoSource> callVideoSource;
    std::chrono::steady_clock::time_point callVideoShown;
    std::chrono::steady_clock::time_point selfVideoShown;
    // Its remotemodels.h chat name, or empty.
    QString activeChat;
    bool savePending{false};
    // What the ongoing notification says.
    QString shownStatus;
    Core::Connection connection{Core::Connection::None};
    // By remotemodels.h's chat name.
    QHash<QString, ChatModel*> chats;
    // Where the chats that the user cleared start, by remotemodels.h's chat name.
    QHash<QString, ChatLogIdx> clearedFrom;
    // The files friends offered that the user hasn't answered yet, by fileKey().
    // When the chat last showed each transfer's progress, by fileKey().
    Core* core{nullptr};
};
