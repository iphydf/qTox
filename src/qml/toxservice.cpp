/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "toxservice.h"

#include "callaudio.h"

#include "audio/audio.h"
#include "audio/iaudiocontrol.h"
#include "src/core/conferenceid.h"
#include "src/core/core.h"
#include "src/core/coreav.h"
#include "src/core/corefile.h"
#include "src/core/groupid.h"
#include "src/core/icoresettings.h"
#include "src/core/toxid.h"
#include "src/core/toxpk.h"
#include "src/model/chathistory.h"
#include "src/model/chatmanager.h"
#include "src/model/chatroom/friendchatroom.h"
#include "src/model/conference.h"
#include "src/model/conferenceinvite.h"
#include "src/model/friend.h"
#include "src/model/friendmessagedispatcher.h"
#include "src/model/group.h"
#include "src/model/groupinvite.h"
#include "src/model/inotifier.h"
#include "src/model/message.h"
#include "src/model/notificationdata.h"
#include "src/model/notificationgenerator.h"
#include "src/model/profile/profileinfo.h"
#include "src/model/status.h"
#include "src/model/systemmessage.h"
#include "src/model/toxclientstandards.h"
#include "src/persistence/history.h"
#include "src/persistence/paths.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"
#include "src/qml/chatmodel.h"
#include "src/qml/files.h"
#include "src/qml/remotemodels.h"
#include "src/translator.h"
#include "src/video/cameradevice.h"
#include "src/video/camerasource.h"
#include "src/video/videoframe.h"
#include "src/video/videosource.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRemoteObjectHostBase>
#include <QStandardPaths>
#include <QTimer>

#include <limits>
#include <tuple>
#include <utility>

namespace {
QList<int> allRoles(const QAbstractItemModel& model)
{
    return model.roleNames().keys();
}

/**
 * @brief A video frame as a JPEG for the user interface, if one is due: at most 15 a second, at
 *        most @p maxSide pixels wide and high.
 */
QByteArray uiFrame(VideoFrame& frame, int maxSide, std::chrono::steady_clock::time_point& lastShown)
{
    constexpr auto interval = std::chrono::milliseconds{1000 / 15};
    const auto now = std::chrono::steady_clock::now();
    if (now - lastShown < interval) {
        return {};
    }
    lastShown = now;

    const QSize source = frame.getSourceDimensions().size();
    const QImage image =
        frame.toQImage(source.boundedTo(source.scaled(maxSide, maxSide, Qt::KeepAspectRatio)));
    QByteArray jpeg;
    QBuffer buffer{&jpeg};
    if (image.isNull() || !buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "JPEG")) {
        return {};
    }
    return jpeg;
}

/** The cameras, without CameraDevice::NONE. */
QVector<QPair<QString, QString>> cameras()
{
    QVector<QPair<QString, QString>> devices = CameraDevice::getDeviceList();
    devices.removeIf(
        [](const QPair<QString, QString>& device) { return device.first == CameraDevice::NONE; });
    return devices;
}

} // namespace

ToxService::ToxService(Settings& settings_, IMessageBoxManager& messageBoxManager_,
                       CameraSource& cameraSource_, INotifier& notifier_,
                       QRemoteObjectHostBase& host_, QObject* parent)
    : ToxServiceSimpleSource(parent)
    , settings{settings_}
    , messageBoxManager{messageBoxManager_}
    , cameraSource{cameraSource_}
    , notifier{notifier_}
    , host{host_}
    , friendRequests{settings_}
{
    Translator::translate(settings.getTranslationInUse());
    setTranslation(settings.getTranslationInUse());
    readSettings();
    connectSettings();
    updateProfiles();
    updateFriendRequests();
    updateStatus();
    connect(this, &ToxService::loggedInChanged, this, &ToxService::updateStatus);
    connect(this, &ToxService::onlineChanged, this, &ToxService::updateStatus);

    host.enableRemoting(this);
    host.enableRemoting(&chatList, RemoteModels::chats, allRoles(chatList));
    host.enableRemoting(&friendRequests, RemoteModels::friendRequests, allRoles(friendRequests));
    host.enableRemoting(&invites, RemoteModels::invites, allRoles(invites));
    logInAutomatically();
}

Profile* ToxService::profile() const
{
    return session != nullptr ? &session->getProfile() : nullptr;
}

ProfileInfo* ToxService::profileInfo() const
{
    return session != nullptr ? &session->getProfileInfo() : nullptr;
}

ChatManager* ToxService::chatManager() const
{
    return session != nullptr ? &session->getChatManager() : nullptr;
}

CallManager* ToxService::calls() const
{
    return session != nullptr ? &session->getCallManager() : nullptr;
}

InviteManager* ToxService::inviteManager() const
{
    return session != nullptr ? &session->getInviteManager() : nullptr;
}

FileManager* ToxService::files() const
{
    return session != nullptr ? &session->getFileManager() : nullptr;
}

Friend* ToxService::findFriend(const ToxPk& friendPk) const
{
    return session != nullptr ? session->getFriendList().findFriend(friendPk) : nullptr;
}

Conference* ToxService::findConference(const ConferenceId& id) const
{
    return session != nullptr ? session->getConferenceList().findConference(id) : nullptr;
}

Group* ToxService::findGroup(const GroupId& id) const
{
    return session != nullptr ? session->getGroupList().findGroup(id) : nullptr;
}

void ToxService::logInAutomatically()
{
    const QString name = settings.getCurrentProfile();
    Paths& paths = settings.getPaths();
    if (settings.getAutoLogin() && Profile::exists(name, paths) && !Profile::isEncrypted(name, paths)) {
        loadProfile(name, {}, true);
    }
}

ToxService::~ToxService()
{
    host.disableRemoting(this);
    host.disableRemoting(&chatList);
    host.disableRemoting(&friendRequests);
    host.disableRemoting(&invites);
    closeProfile();
}

void ToxService::createProfile(const QString& name, const QString& password, bool autoLogin)
{
    if (session != nullptr) {
        return;
    }

    Session::LoginResult result =
        Session::create(name, password, settings, cameraSource, messageBoxManager);
    if (result.session == nullptr) {
        emit error(tr("Couldn't create a new profile"), describe(result.error));
        return;
    }
    start(std::move(result.session), autoLogin);
    updateProfiles();
}

void ToxService::loadProfile(const QString& name, const QString& password, bool autoLogin)
{
    if (session != nullptr) {
        return;
    }

    Session::LoginResult result =
        Session::load(name, password, settings, cameraSource, messageBoxManager);
    if (result.session == nullptr) {
        emit error(tr("Couldn't load this profile"), describe(result.error));
        return;
    }
    start(std::move(result.session), autoLogin);
}

QString ToxService::describe(Session::LoginError loginError)
{
    // As the Widgets UI says it.
    switch (loginError) {
    case Session::LoginError::None:
        break;
    case Session::LoginError::AlreadyExists:
        return tr("A profile with this name already exists.");
    case Session::LoginError::InUse:
        return tr("This profile is already in use.");
    case Session::LoginError::WrongPassword:
        return tr("Wrong password.");
    case Session::LoginError::Unreadable:
        return tr("Its Tox save file couldn't be read.");
    case Session::LoginError::NotWritable:
        return tr("Its Tox save file couldn't be written.");
    }
    return {};
}

void ToxService::logout()
{
    // Whoever logs out stays logged out, also when Android restarts the service.
    settings.setAutoLogin(false);
    closeProfile();
}

void ToxService::setPassword(const QString& password)
{
    if (profile() == nullptr) {
        return;
    }

    const QString failure = profile()->setPassword(password);
    updateProfiles();
    if (!failure.isEmpty()) {
        emit error(tr("Couldn't change password"), failure);
    }
}

void ToxService::renameProfile(const QString& name)
{
    if (profileInfo() == nullptr) {
        return;
    }

    switch (profileInfo()->renameProfile(name)) {
    case IProfileInfo::RenameResult::OK:
        setProfileName(profile()->getName());
        updateProfiles();
        return;
    case IProfileInfo::RenameResult::EmptyName:
        return;
    case IProfileInfo::RenameResult::ProfileAlreadyExists:
        emit error(tr("Profile already exists"),
                   tr("A profile named \"%1\" already exists.").arg(name));
        return;
    case IProfileInfo::RenameResult::Error:
        emit error(tr("Failed to rename"), tr("Couldn't rename the profile to \"%1\"").arg(name));
        return;
    }
}

void ToxService::deleteProfile()
{
    if (profileInfo() == nullptr) {
        return;
    }

    const QStringList left = profileInfo()->removeProfile();
    settings.setAutoLogin(false);
    closeProfile();
    updateProfiles();
    if (!left.isEmpty()) {
        emit error(tr("Files could not be deleted!"),
                   tr("The following files could not be deleted:") + "\n\n" + left.join('\n')
                       + "\n\n" + tr("Please manually remove them."));
    }
}

QByteArray ToxService::exportProfile()
{
    if (profile() == nullptr) {
        return {};
    }

    const QByteArray save = profile()->exportSave();
    if (save.isEmpty()) {
        emit error(tr("Failed to save file"),
                   tr("Can't read %1.")
                       .arg(Profile::getProfilePath(profile()->getName(), settings.getPaths())));
    }
    return save;
}

void ToxService::importProfile(const QString& name, const QByteArray& save)
{
    switch (Profile::importSave(name, save, settings.getPaths())) {
    case Profile::ImportResult::OK:
        updateProfiles();
        return;
    case Profile::ImportResult::NotToxSave:
        emit error(tr("Couldn't import the profile"), tr("This is not a Tox save file."));
        return;
    case Profile::ImportResult::InUse:
        emit error(tr("Couldn't import the profile"), tr("The profile \"%1\" is in use.").arg(name));
        return;
    case Profile::ImportResult::NotWritable:
        emit error(tr("Couldn't import the profile"),
                   tr("Can't write %1.").arg(Profile::getProfilePath(name, settings.getPaths())));
        return;
    }
}

void ToxService::closeProfile()
{
    if (profile() == nullptr) {
        return;
    }

    // The call and its sound use the audio that goes below.
    endCall();
    clearCall();

    // The chats and the friend list refer to what the chat manager and the
    // profile own.
    for (ChatModel* chat : std::as_const(chats)) {
        host.disableRemoting(chat);
        delete chat;
    }
    chats.clear();
    clearedFrom.clear();
    chatList.clear();
    invites.clear();
    setInviteCount(0);
    notifications.reset();
    // Saves the profile.
    session.reset();
    core = nullptr;
    connection = Core::Connection::None;

    // First, else the status says "Connecting" for a moment.
    setLoggedIn(false);
    setProfileName({});
    setOnline(false);
    setSelfName({});
    setSelfId({});
    setSelfStatusMessage({});
    setSelfAvatar({});
    setSelfAvatarHash({});
}

void ToxService::changeName(const QString& name)
{
    if (core != nullptr) {
        core->setUsername(name);
    }
}

void ToxService::changeStatusMessage(const QString& message)
{
    if (core != nullptr) {
        core->setStatusMessage(message);
    }
}

void ToxService::changeStatus(int status)
{
    // Core takes only these.
    const auto newStatus = static_cast<Status::Status>(status);
    if (core != nullptr
        && (newStatus == Status::Status::Online || newStatus == Status::Status::Away
            || newStatus == Status::Status::Busy)) {
        core->setStatus(newStatus);
    }
}

void ToxService::newNospam()
{
    if (core != nullptr) {
        core->setNospam(QRandomGenerator::global()->generate());
    }
}

void ToxService::changeAvatar(const QByteArray& avatar)
{
    if (profile() != nullptr && !avatar.isEmpty()
        && ToxClientStandards::IsValidAvatarSize(avatar.size())) {
        profile()->setAvatar(avatar);
    }
}

void ToxService::removeAvatar()
{
    if (profile() != nullptr) {
        profile()->removeSelfAvatar();
    }
}

void ToxService::updateSelfAvatar()
{
    // The profile saved it first.
    const QByteArray avatar = profile()->loadAvatarData(profile()->getCore().getSelfPublicKey());
    setSelfAvatar(avatar);
    setSelfAvatarHash(ChatListModel::avatarHash(avatar));
}

void ToxService::addFriend(const QString& toxId, const QString& message)
{
    // Core reports what's wrong with the ID.
    if (core != nullptr) {
        core->requestFriendship(ToxId(toxId.trimmed()), message);
    }
}

void ToxService::acceptFriendRequest(const QString& publicKey)
{
    if (session != nullptr) {
        session->getFriendRequestManager().accept(ToxPk{publicKey});
    }
}

void ToxService::rejectFriendRequest(const QString& publicKey)
{
    if (session != nullptr) {
        session->getFriendRequestManager().reject(ToxPk{publicKey});
    }
}

void ToxService::setFriendAlias(const QString& publicKey, const QString& alias)
{
    const ToxPk friendPk{publicKey};
    Friend* f = findFriend(friendPk);
    if (f == nullptr) {
        return;
    }

    f->setAlias(alias);
    settings.setFriendAlias(friendPk, alias);
    settings.savePersonal();
}

void ToxService::removeFriend(const QString& publicKey, bool removeHistory)
{
    const ToxPk friendPk{publicKey};
    Friend* f = findFriend(friendPk);
    if (f == nullptr || chatManager() == nullptr) {
        return;
    }

    if (removeHistory) {
        chatManager()->removeHistory(*f);
    }

    removeChat(ChatKind::Kind::Friend, publicKey, true);
    settings.savePersonal();
}

void ToxService::removeHistory(const QString& publicKey)
{
    const Friend* f = findFriend(ToxPk{publicKey});
    IChatLog* chatLog = findChatLog(ChatKind::Kind::Friend, publicKey);
    if (f == nullptr || chatLog == nullptr || !chatManager()->removeHistory(*f)) {
        return;
    }

    // The chat still has what it loaded: it starts again.
    const QString name = RemoteModels::chat(ChatKind::Kind::Friend, publicKey);
    clearedFrom.insert(name, chatLog->getNextIdx());
    if (ChatModel* chat = chats.value(name)) {
        chat->startAt(chatLog->getNextIdx());
    }
    chatLog->addSystemMessage({SystemMessageType::cleared, QDateTime::currentDateTime(), {}});
}

void ToxService::setFriendNote(const QString& publicKey, const QString& note)
{
    const ToxPk friendPk{publicKey};
    if (findFriend(friendPk) == nullptr) {
        return;
    }
    settings.setContactNote(friendPk, note);
    settings.savePersonal();
    chatList.setNote(publicKey, note);
}

void ToxService::acceptInvite(const QString& id, const QString& password)
{
    const std::optional<InviteModel::Invite> invite = invites.take(id);
    setInviteCount(invites.rowCount());
    if (inviteManager() != nullptr && invite) {
        inviteManager()->accept(invite->invite, password);
    }
}

void ToxService::rejectInvite(const QString& id)
{
    const std::optional<InviteModel::Invite> invite = invites.take(id);
    setInviteCount(invites.rowCount());
    if (inviteManager() != nullptr && invite) {
        inviteManager()->reject(invite->invite);
    }
}

void ToxService::openChat(int kind, const QString& id)
{
    const std::optional<ChatKind::Kind> chatKind = ChatKind::fromInt(kind);
    if (!chatKind || core == nullptr) {
        return;
    }

    const QString name = RemoteModels::chat(*chatKind, id);
    IChatLog* chatLog = findChatLog(*chatKind, id);
    if (chats.contains(name) || chatLog == nullptr) {
        return;
    }

    auto* chat =
        new ChatModel(*chatLog, core->getSelfPublicKey(), firstShown(*chatKind, id, *chatLog),
                      {settings.getStylePreference(), settings.getHidePostNullSuffix()}, this);
    chats.insert(name, chat);
    host.enableRemoting(chat, name, allRoles(*chat));
}

void ToxService::sendMessage(int kind, const QString& id, const QString& text)
{
    const std::optional<ChatKind::Kind> chatKind = ChatKind::fromInt(kind);
    IMessageDispatcher* dispatcher = chatKind ? findDispatcher(*chatKind, id) : nullptr;
    const std::optional<Message> message = Message::typed(text);
    if (dispatcher != nullptr && message) {
        dispatcher->sendMessage(message->isAction, message->content);
    }
}

int ToxService::findMessage(int kind, const QString& id, const QString& phrase, int fromRow,
                            bool backward)
{
    const std::optional<ChatKind::Kind> chatKind = ChatKind::fromInt(kind);
    const IChatLog* chatLog = chatKind ? findChatLog(*chatKind, id) : nullptr;
    if (chatLog == nullptr || phrase.isEmpty() || fromRow < 0) {
        return -1;
    }
    // ChatModel's rows start at the chat log's first item that the chat shows.
    const size_t first = firstShown(*chatKind, id, *chatLog).get();
    const size_t from = first + static_cast<size_t>(fromRow);
    if (from >= chatLog->getNextIdx().get()) {
        return -1;
    }
    // As the Widgets UI searches by default.
    const SearchPos start{ChatLogIdx{from}, 0};
    const SearchResult result = backward ? chatLog->searchBackward(start, phrase, {})
                                         : chatLog->searchForward(start, phrase, {});
    return result.found && result.pos.logIdx.get() >= first
               ? static_cast<int>(result.pos.logIdx.get() - first)
               : -1;
}

void ToxService::acceptFile(const QString& publicKey, int fileNum)
{
    if (files() == nullptr) {
        return;
    }
    const std::optional<ToxFile> offered =
        files()->offeredFile(ToxPk{publicKey}, static_cast<uint32_t>(fileNum));
    const QString dir = receivedFilesDir();
    if (offered && !files()->accept(*offered, dir)) {
        emit error(tr("Couldn't receive the file"), tr("Can't write to %1.").arg(dir));
    }
}

void ToxService::cancelFile(const QString& publicKey, int fileNum, bool sending)
{
    const Friend* f = findFriend(ToxPk{publicKey});
    if (f == nullptr || core == nullptr) {
        return;
    }
    if (sending) {
        core->getCoreFile()->cancelFileSend(f->getId(), static_cast<uint32_t>(fileNum));
    } else {
        core->getCoreFile()->cancelFileRecv(f->getId(), static_cast<uint32_t>(fileNum));
    }
}

void ToxService::pauseFile(const QString& publicKey, int fileNum)
{
    const Friend* f = findFriend(ToxPk{publicKey});
    if (f != nullptr && core != nullptr) {
        core->getCoreFile()->pauseResumeFile(f->getId(), static_cast<uint32_t>(fileNum));
    }
}

void ToxService::sendFile(const QString& publicKey, const QString& path)
{
    if (files() != nullptr && files()->send(ToxPk{publicKey}, path) != FileManager::SendResult::Sent) {
        emit error(tr("Couldn't send the file"), tr("Can't read %1.").arg(path));
    }
}

QString ToxService::createConference(const QString& title)
{
    if (core == nullptr) {
        return {};
    }

    // As the Widgets UI makes them: with audio.
    const int number = core->createConference();
    if (number < 0) {
        return {};
    }
    if (!title.trimmed().isEmpty()) {
        core->changeConferenceTitle(static_cast<uint32_t>(number), title.trimmed());
    }
    return core->getConferencePersistentId(static_cast<uint32_t>(number)).toString();
}

void ToxService::inviteToConference(const QString& id, const QString& publicKey)
{
    const Conference* conference = findConference(ConferenceId::fromString(id));
    const Friend* f = findFriend(ToxPk{publicKey});
    if (core == nullptr || conference == nullptr || f == nullptr) {
        return;
    }
    core->conferenceInviteFriend(f->getId(), static_cast<int>(conference->getId()));
}

void ToxService::setConferenceTitle(const QString& id, const QString& title)
{
    const Conference* conference = findConference(ConferenceId::fromString(id));
    if (core != nullptr && conference != nullptr && !title.trimmed().isEmpty()) {
        core->changeConferenceTitle(conference->getId(), title.trimmed());
    }
}

QString ToxService::createGroup(const QString& name)
{
    if (core == nullptr) {
        return {};
    }

    const int number = core->createGroup(name.trimmed());
    return number < 0 ? QString{}
                      : core->getGroupPersistentId(static_cast<uint32_t>(number)).toString();
}

QString ToxService::joinGroup(const QString& id, const QString& password)
{
    const GroupId groupId = GroupId::fromString(id);
    if (core == nullptr || groupId.isEmpty() || core->joinGroup(groupId, password.toUtf8()) < 0) {
        return {};
    }
    return groupId.toString();
}

void ToxService::inviteToGroup(const QString& id, const QString& publicKey)
{
    const Group* group = findGroup(GroupId::fromString(id));
    const Friend* f = findFriend(ToxPk{publicKey});
    if (core == nullptr || group == nullptr || f == nullptr) {
        return;
    }
    core->groupInviteFriend(f->getId(), static_cast<int>(group->getId()));
}

void ToxService::setGroupTopic(const QString& id, const QString& topic)
{
    const Group* group = findGroup(GroupId::fromString(id));
    if (core != nullptr && group != nullptr) {
        core->changeGroupTopic(group->getId(), topic.trimmed());
    }
}

void ToxService::setGroupNickname(const QString& id, const QString& nickname)
{
    changeGroup(id, [&nickname](Group& group) { return group.setGroupNickname(nickname.trimmed()); });
}

void ToxService::setGroupPassword(const QString& id, const QString& password)
{
    changeGroup(id, [&password](Group& group) { return group.setGroupPassword(password.toUtf8()); });
}

void ToxService::setGroupPrivate(const QString& id, bool isPrivate)
{
    changeGroup(id, [isPrivate](Group& group) {
        return group.setGroupPrivacyState(isPrivate ? GroupPrivacyState::Private
                                                    : GroupPrivacyState::Public);
    });
}

void ToxService::setGroupPeerLimit(const QString& id, int peerLimit)
{
    if (peerLimit < 1 || peerLimit > std::numeric_limits<uint16_t>::max()) {
        return;
    }
    changeGroup(id, [peerLimit](Group& group) {
        return group.setGroupPeerLimit(static_cast<uint16_t>(peerLimit));
    });
}

void ToxService::setGroupTopicLocked(const QString& id, bool locked)
{
    changeGroup(id, [locked](Group& group) {
        return group.setGroupTopicLock(locked ? GroupTopicLock::Enabled : GroupTopicLock::Disabled);
    });
}

void ToxService::setGroupVoiceState(const QString& id, int voiceState)
{
    const auto state = static_cast<GroupVoiceState>(voiceState);
    if (state != GroupVoiceState::All && state != GroupVoiceState::Moderator
        && state != GroupVoiceState::Founder) {
        return;
    }
    changeGroup(id, [state](Group& group) { return group.setGroupVoiceState(state); });
}

void ToxService::setGroupPeerRole(const QString& id, const QString& publicKey, int role)
{
    // A group has one founder, for good.
    const auto newRole = static_cast<GroupRole>(role);
    if (newRole != GroupRole::Moderator && newRole != GroupRole::User
        && newRole != GroupRole::Observer) {
        return;
    }
    changeGroup(id, [&publicKey, newRole](Group& group) {
        return group.setPeerRole(ToxPk{publicKey}, newRole);
    });
}

void ToxService::kickGroupPeer(const QString& id, const QString& publicKey)
{
    changeGroup(id, [&publicKey](Group& group) { return group.kickPeer(ToxPk{publicKey}); });
}

void ToxService::sendGroupPrivateMessage(const QString& id, const QString& publicKey, const QString& text)
{
    const GroupId groupId = GroupId::fromString(id);
    const Group* group = findGroup(groupId);
    GroupMessageDispatcher* dispatcher =
        chatManager() != nullptr ? chatManager()->getGroupDispatcher(groupId) : nullptr;
    if (group == nullptr || dispatcher == nullptr || text.trimmed().isEmpty()) {
        return;
    }
    // Not to a peer who left.
    const ToxPk recipient{publicKey};
    if (!group->getPeerList().contains(recipient)) {
        return;
    }
    if (const std::optional<Message> message = Message::typed(text)) {
        dispatcher->sendPrivateMessage(recipient, message->isAction, message->content);
    }
}

void ToxService::changeGroup(const QString& id, const std::function<bool(Group&)>& change)
{
    Group* group = findGroup(GroupId::fromString(id));
    if (core != nullptr && group != nullptr && !change(*group)) {
        emit error(tr("Couldn't change the group"), tr("Tox refused the change."));
    }
}

void ToxService::setAutoAcceptInvites(const QString& publicKey, bool accept)
{
    const ToxPk friendPk{publicKey};
    if (findFriend(friendPk) == nullptr) {
        return;
    }
    settings.setAutoConferenceInvite(friendPk, accept);
    settings.setAutoGroupInvite(friendPk, accept);
    settings.savePersonal();
    chatList.setAutoAcceptInvites(publicKey, accept);
}

void ToxService::setAutoAcceptFiles(const QString& publicKey, bool accept)
{
    const ToxPk friendPk{publicKey};
    if (findFriend(friendPk) == nullptr) {
        return;
    }
    settings.setAutoAcceptDir(friendPk, accept ? receivedFilesDir() : QString{});
    settings.savePersonal();
    chatList.setAutoAcceptFiles(publicKey, accept);
}

void ToxService::setTyping(const QString& publicKey, bool typing)
{
    const ToxPk friendPk{publicKey};
    if (findFriend(friendPk) != nullptr) {
        chatManager()->getFriendChatroom(friendPk)->setDraftHasText(typing);
    }
}

void ToxService::eraseHistory()
{
    if (History* history = profile() != nullptr ? profile()->getHistory() : nullptr) {
        history->eraseHistory();
    }
}

void ToxService::startCall(const QString& publicKey, bool video)
{
    Friend* f = findFriend(ToxPk{publicKey});
    if (core == nullptr || f == nullptr || callState() != static_cast<int>(CallState::State::None)) {
        return;
    }
    if (video) {
        chooseCamera();
    }
    if (calls()->start(f->getPublicKey(), video)) {
        setSendsVideo(video);
    }
}

void ToxService::answerCall(bool video)
{
    Friend* f = callingFriend();
    if (f == nullptr || callState() != static_cast<int>(CallState::State::Incoming)) {
        return;
    }
    // Without the user's video, the friend sees that video is off.
    const bool sendsVideo = video && callOffersVideo();
    if (sendsVideo) {
        chooseCamera();
    }
    if (calls()->answer(f->getPublicKey(), sendsVideo)) {
        setSendsVideo(sendsVideo);
    }
}

void ToxService::joinConferenceCall(const QString& id)
{
    const Conference* conference = findConference(ConferenceId::fromString(id));
    if (conference != nullptr && calls()->joinConference(*conference)) {
        setCall(ChatKind::Kind::Conference, id, CallState::State::Active);
    }
}

void ToxService::endCall()
{
    if (const Friend* f = callingFriend()) {
        calls()->end(f->getPublicKey());
    } else if (const Conference* conference = callingConference()) {
        calls()->leaveConference(*conference);
        clearCall();
    }
}

void ToxService::toggleCallMic()
{
    if (core == nullptr) {
        return;
    }
    CoreAV& av = *core->getAv();
    if (const Friend* f = callingFriend()) {
        av.toggleMuteCallInput(f);
        setCallMicMuted(av.isCallInputMuted(f));
    } else if (const Conference* conference = callingConference()) {
        av.muteCallInput(conference, !av.isConferenceCallInputMuted(conference));
        setCallMicMuted(av.isConferenceCallInputMuted(conference));
    }
}

void ToxService::toggleCallLoudspeaker()
{
    // Only for calls the user is in: an incoming one rings where Android plays ringtones.
    const auto state = static_cast<CallState::State>(callState());
    if (state != CallState::State::Outgoing && state != CallState::State::Active) {
        return;
    }
    setCallLoudspeaker(!callLoudspeaker());
    CallAudio::setLoudspeaker(callLoudspeaker());
}

void ToxService::toggleCallSpeaker()
{
    if (core == nullptr) {
        return;
    }
    CoreAV& av = *core->getAv();
    if (const Friend* f = callingFriend()) {
        av.toggleMuteCallOutput(f);
        setCallSpeakerMuted(av.isCallOutputMuted(f));
    } else if (const Conference* conference = callingConference()) {
        av.muteCallOutput(conference, !av.isConferenceCallOutputMuted(conference));
        setCallSpeakerMuted(av.isConferenceCallOutputMuted(conference));
    }
}

void ToxService::onFriendStatusChanged(const Friend& f, Status::Status status)
{
    if (status == Status::Status::Offline) {
        // Toxcore doesn't say that they stopped.
        chatList.setTyping(f.getPublicKey().toString(), false);
    }
}

void ToxService::onCallStateChanged(const ToxPk& friendPk, CallManager::State state)
{
    const Friend* f = findFriend(friendPk);
    if (f == nullptr) {
        return;
    }
    switch (state) {
    case CallManager::State::Incoming: {
        const bool video = calls()->hasVideo(friendPk);
        setCall(f, CallState::State::Incoming);
        setCallOffersVideo(video);
        break;
    }
    case CallManager::State::Outgoing:
        setCall(f, CallState::State::Outgoing);
        break;
    case CallManager::State::Active:
        setCall(f, CallState::State::Active);
        watchCallVideo(*f);
        break;
    case CallManager::State::None:
        clearCall();
        break;
    }
}

void ToxService::watchCallVideo(const Friend& f)
{
    // An audio call has none.
    callVideoSource = core->getAv()->getVideoSourceFromCall(f.getId());
    if (callVideoSource == nullptr) {
        return;
    }
    // Only then does it pass on frames.
    callVideoSource->subscribe();
    connect(callVideoSource.get(), &VideoSource::frameAvailable, this, &ToxService::onCallVideoFrame);
    connect(callVideoSource.get(), &VideoSource::sourceStopped, this, [this] { setCallVideo(false); });
}

void ToxService::stopCallVideo()
{
    if (callVideoSource != nullptr) {
        disconnect(callVideoSource.get(), nullptr, this, nullptr);
        callVideoSource->unsubscribe();
        callVideoSource.reset();
    }
    setCallVideo(false);
}

void ToxService::onCallVideoFrame(const std::shared_ptr<VideoFrame>& frame)
{
    // As big as a phone's screen needs.
    constexpr int maxSide = 640;
    if (callVideoSource == nullptr) {
        return;
    }
    const QByteArray jpeg = uiFrame(*frame, maxSide, callVideoShown);
    if (jpeg.isEmpty()) {
        return;
    }
    setCallVideo(true);
    emit callVideoFrame(jpeg);
}

void ToxService::chooseCamera(QString device)
{
    const QVector<QPair<QString, QString>> devices = cameras();
    setCanSwitchCamera(devices.size() > 1);
    const auto known = [&devices](const QString& name) {
        return std::any_of(devices.cbegin(), devices.cend(),
                           [&name](const QPair<QString, QString>& d) { return d.first == name; });
    };
    if (!known(device)) {
        device = settings.getVideoDev();
    }
    if (!known(device)) {
        if (devices.isEmpty()) {
            return;
        }
        device = devices.first().first;
    }
    settings.setVideoDev(device);

    // The biggest size up to the Widgets UI's 480p: without a size, FFmpeg asks Android's
    // cameras for their biggest.
    constexpr int maxLines = 480;
    const QVector<VideoMode> modes = CameraDevice::getVideoModes(device);
    const VideoMode* best = nullptr;
    for (const VideoMode& mode : modes) {
        if (std::min(mode.width, mode.height) <= maxLines
            && (best == nullptr || mode.width * mode.height > best->width * best->height)) {
            best = &mode;
        }
    }
    if (best != nullptr) {
        settings.setCamVideoRes(best->toRect());
        settings.setCamVideoFPS(best->fps);
    }
}

void ToxService::switchCamera()
{
    const QVector<QPair<QString, QString>> devices = cameras();
    if (!callSendsVideo() || devices.size() < 2) {
        return;
    }
    const QString current = settings.getVideoDev();
    const auto it =
        std::find_if(devices.cbegin(), devices.cend(),
                     [&current](const QPair<QString, QString>& d) { return d.first == current; });
    const auto next = (it == devices.cend() || std::next(it) == devices.cend()) ? devices.cbegin()
                                                                                : std::next(it);
    chooseCamera(next->first);
    cameraSource.setupDefault();
}

void ToxService::setSendsVideo(bool sends)
{
    setCallSendsVideo(sends);
    if (sends) {
        connect(&cameraSource, &VideoSource::frameAvailable, this, &ToxService::onSelfVideoFrame,
                Qt::UniqueConnection);
    } else {
        disconnect(&cameraSource, &VideoSource::frameAvailable, this, &ToxService::onSelfVideoFrame);
    }
    updateRecording();
}

void ToxService::onSelfVideoFrame(const std::shared_ptr<VideoFrame>& frame)
{
    // A corner of the screen.
    constexpr int maxSide = 320;
    const QByteArray jpeg = uiFrame(*frame, maxSide, selfVideoShown);
    if (!jpeg.isEmpty()) {
        emit selfVideoFrame(jpeg);
    }
}

void ToxService::updateRecording()
{
    const auto state = static_cast<CallState::State>(callState());
    const bool inCall = state == CallState::State::Outgoing || state == CallState::State::Active;
    notifier.setRecording(inCall, inCall && callSendsVideo());
}

Friend* ToxService::callingFriend()
{
    return callChat().isEmpty() || core == nullptr
                   || callKind() != static_cast<int>(ChatKind::Kind::Friend)
               ? nullptr
               : findFriend(ToxPk{callChat()});
}

Conference* ToxService::callingConference()
{
    return callChat().isEmpty() || core == nullptr
                   || callKind() != static_cast<int>(ChatKind::Kind::Conference)
               ? nullptr
               : findConference(ConferenceId::fromString(callChat()));
}

void ToxService::setCall(const Friend* f, CallState::State state)
{
    setCall(ChatKind::Kind::Friend, f->getPublicKey().toString(), state);
}

void ToxService::setCall(ChatKind::Kind kind, const QString& id, CallState::State state)
{
    if (state != CallState::State::Active) {
        stopCallVideo();
    }
    setCallKind(static_cast<int>(kind));
    setCallChat(id);
    setCallState(static_cast<int>(state));
    setCallStarted(state == CallState::State::Active ? QDateTime::currentDateTime() : QDateTime{});
    setCallMicMuted(false);
    setCallSpeakerMuted(false);
    const bool inCall = state == CallState::State::Outgoing || state == CallState::State::Active;
    CallAudio::setInCall(inCall);
    if (!inCall) {
        setCallLoudspeaker(false);
    }
    if (state == CallState::State::None) {
        setCallOffersVideo(false);
        setSendsVideo(false);
    }
    updateRecording();
}

void ToxService::clearCall()
{
    // Without a chat, its kind doesn't matter.
    setCall(ChatKind::Kind::Friend, {}, CallState::State::None);
}

void ToxService::leaveConference(const QString& id)
{
    if (chatManager() == nullptr) {
        return;
    }

    const Conference* conference = findConference(ConferenceId::fromString(id));
    if (conference == nullptr) {
        return;
    }

    if (callingConference() == conference) {
        endCall();
    }
    removeChat(ChatKind::Kind::Conference, id, true);
}

void ToxService::leaveGroup(const QString& id)
{
    removeChat(ChatKind::Kind::Group, id, true);
}

void ToxService::removeChat(ChatKind::Kind kind, const QString& id, bool leave)
{
    Chat* chat = findChat(kind, id);
    if (chat == nullptr) {
        return;
    }

    // What refers to the chat goes first, as in the Widgets UI.
    closeChat(kind, id);
    chatList.removeChat(kind, id);
    chatManager()->removeChat(*chat, leave);
}

void ToxService::connectSettings()
{
    connect(this, &ToxService::languageChanged, this, [this](const QString& language) {
        settings.setTranslation(language);
        Translator::translate(settings.getTranslationInUse());
        setTranslation(settings.getTranslationInUse());
    });
    connect(this, &ToxService::typingNotificationChanged, this,
            [this](bool enabled) { settings.setTypingNotification(enabled); });
    connect(this, &ToxService::statusChangeMessagesChanged, this,
            [this](bool enabled) { settings.setStatusChangeNotificationEnabled(enabled); });
    connect(this, &ToxService::keepHistoryChanged, this,
            [this](bool enabled) { settings.setEnableLogging(enabled); });
    connect(this, &ToxService::notifyChanged, this,
            [this](bool enabled) { settings.setNotify(enabled); });
    connect(this, &ToxService::conferenceAlwaysNotifyChanged, this,
            [this](bool enabled) { settings.setConferenceAlwaysNotify(enabled); });
    connect(this, &ToxService::notifySoundChanged, this,
            [this](bool enabled) { settings.setNotifySound(enabled); });
    connect(this, &ToxService::busySoundChanged, this,
            [this](bool enabled) { settings.setBusySound(enabled); });
    connect(this, &ToxService::autoSaveFilesChanged, this,
            [this](bool enabled) { settings.setAutoSaveEnabled(enabled); });
    connect(this, &ToxService::maxAutoAcceptSizeChanged, this, [this](qint64 size) {
        if (size < 0) {
            setMaxAutoAcceptSize(static_cast<qint64>(settings.getMaxAutoAcceptSize()));
            return;
        }
        settings.setMaxAutoAcceptSize(static_cast<size_t>(size));
    });
    connect(this, &ToxService::udpEnabledChanged, this,
            [this](bool enabled) { settings.setForceTCP(!enabled); });
    connect(this, &ToxService::ipv6EnabledChanged, this,
            [this](bool enabled) { settings.setEnableIPv6(enabled); });
    connect(this, &ToxService::lanDiscoveryEnabledChanged, this,
            [this](bool enabled) { settings.setEnableLanDiscovery(enabled); });
    connect(this, &ToxService::proxyTypeChanged, this, [this](int type) {
        if (type < static_cast<int>(ICoreSettings::ProxyType::ptNone)
            || type > static_cast<int>(ICoreSettings::ProxyType::ptHTTP)) {
            setProxyType(static_cast<int>(settings.getProxyType()));
            return;
        }
        settings.setProxyType(static_cast<ICoreSettings::ProxyType>(type));
    });
    connect(this, &ToxService::proxyAddressChanged, this,
            [this](const QString& address) { settings.setProxyAddr(address); });
    connect(this, &ToxService::proxyPortChanged, this, [this](int port) {
        if (port < 0 || port > std::numeric_limits<quint16>::max()) {
            setProxyPort(settings.getProxyPort());
            return;
        }
        settings.setProxyPort(static_cast<quint16>(port));
    });
}

/**
 * @brief Shows the settings to the user interface: again after logging in, which loads the
 * profile's.
 */
void ToxService::readSettings()
{
    setLanguage(settings.getTranslation());
    setShowIdenticons(settings.getShowIdenticons());
    setTypingNotification(settings.getTypingNotification());
    setStatusChangeMessages(settings.getStatusChangeNotificationEnabled());
    setKeepHistory(settings.getEnableLogging());
    setNotify(settings.getNotify());
    setConferenceAlwaysNotify(settings.getConferenceAlwaysNotify());
    setNotifySound(settings.getNotifySound());
    setBusySound(settings.getBusySound());
    setAutoSaveFiles(settings.getAutoSaveEnabled());
    setMaxAutoAcceptSize(static_cast<qint64>(settings.getMaxAutoAcceptSize()));
    setUdpEnabled(!settings.getForceTCP());
    setIpv6Enabled(settings.getEnableIPv6());
    setLanDiscoveryEnabled(settings.getEnableLanDiscovery());
    setProxyType(static_cast<int>(settings.getProxyType()));
    setProxyAddress(settings.getProxyAddr());
    setProxyPort(settings.getProxyPort());
}

void ToxService::start(std::unique_ptr<Session> newSession, bool autoLogin)
{
    session = std::move(newSession);
    session->setDialogsManager(&dialogsManager);
    // Logging in needs the password of a profile that has one.
    settings.setAutoLogin(autoLogin && !profile()->isEncrypted());
    notifications =
        std::make_unique<Notifications>(notifier, settings, session->getNotificationGenerator(),
                                        *chatManager(), *calls(), *inviteManager(),
                                        session->getFriendRequestManager(), *files(),
                                        session->getFriendList(), session->getConferenceList(),
                                        session->getGroupList(), [this](const Chat& chat) {
                                            return Destination::chat(chat) == activeChat;
                                        });
    // Answering a message's notification reads its chat.
    connect(notifications.get(), &Notifications::answered, this, [this](const Chat& chat) {
        chatList.markRead(ChatKind::of(chat), chat.getPersistentId().toString());
    });
    connect(chatManager(), &ChatManager::messageReceived, this, &ToxService::onMessageReceived);
    calls()->setOneCallAtATime(true);
    // Answering asks the user for the microphone.
    calls()->setAnswersAutomatically(false);
    connect(calls(), &CallManager::stateChanged, this, &ToxService::onCallStateChanged);
    connect(inviteManager(), &InviteManager::added, this, &ToxService::onInviteAdded);
    connect(&session->getFriendRequestManager(), &FriendRequestManager::received, this,
            &ToxService::updateFriendRequests);
    connect(&session->getFriendRequestManager(), &FriendRequestManager::removed, this,
            &ToxService::updateFriendRequests);
    // Toxcore says the progress for every chunk, which would flood the user interface.
    files()->setProgressInterval(std::chrono::milliseconds{500});
#ifdef Q_OS_ANDROID
    // Only there can other apps open them. A profile from a computer names its folders.
    files()->setAutoAcceptDir(receivedFilesDir());
#endif
    connect(files(), &FileManager::offered, this, [this](const ToxFile& file, const ToxPk& friendPk) {
        std::ignore = file;
        countUnread(ChatKind::Kind::Friend, friendPk.toString());
    });
    connect(chatManager(), &ChatManager::friendAdded, this,
            [this](Friend* f, std::shared_ptr<FriendChatroom> chatroom,
                   std::shared_ptr<FriendMessageDispatcher> dispatcher,
                   std::shared_ptr<ChatHistory> chatLog) {
                std::ignore = chatroom;
                chatList.addFriend(f, *chatLog);
                chatList.setAvatar(f->getPublicKey().toString(),
                                   profile()->loadAvatarData(f->getPublicKey()));
                // One switch for both: on if either is, so that the user sees and can stop it.
                chatList.setAutoAcceptInvites(f->getPublicKey().toString(),
                                              settings.getAutoConferenceInvite(f->getPublicKey())
                                                  || settings.getAutoGroupInvite(f->getPublicKey()));
                chatList.setAutoAcceptFiles(f->getPublicKey().toString(),
                                            !settings.getAutoAcceptDir(f->getPublicKey()).isEmpty());
                chatList.setNote(f->getPublicKey().toString(),
                                 settings.getContactNote(f->getPublicKey()));
                connect(f, &Friend::statusChanged, this, [this, f](const ToxPk&, Status::Status status) {
                    onFriendStatusChanged(*f, status);
                });
                // Core doesn't save them, and Android ends this process without the logout
                // that would.
                connect(f, &Friend::nameChanged, this, &ToxService::saveSoon);
                connect(f, &Friend::statusMessageChanged, this, &ToxService::saveSoon);
            });
    connect(chatManager(), &ChatManager::conferenceAdded, this,
            [this](Conference* conference, std::shared_ptr<ConferenceRoom> chatroom,
                   std::shared_ptr<ConferenceMessageDispatcher> dispatcher,
                   std::shared_ptr<IChatLog> chatLog) {
                std::ignore = chatroom;
                chatList.addConference(conference, *chatLog);
            });
    connect(chatManager(), &ChatManager::groupAdded, this,
            [this](Group* group, std::shared_ptr<GroupRoom> chatroom,
                   std::shared_ptr<GroupMessageDispatcher> dispatcher,
                   std::shared_ptr<IChatLog> chatLog) {
                std::ignore = chatroom;
                chatList.addGroup(group, *chatLog);
            });
    // A group whose password Tox rejected: ChatManager quit it.
    connect(chatManager(), &ChatManager::groupRemoved, this, [this](const GroupId& groupId) {
        removeChat(ChatKind::Kind::Group, groupId.toString(), false);
    });

    connect(profile(), &Profile::selfAvatarChanged, this, &ToxService::updateSelfAvatar);
    // Profile says it before it saves the avatar, which it may refuse.
    connect(
        profile(), &Profile::friendAvatarChanged, this,
        [this](const ToxPk& friendPk) {
            if (profile() != nullptr) {
                chatList.setAvatar(friendPk.toString(), profile()->loadAvatarData(friendPk));
            }
        },
        Qt::QueuedConnection);
    connect(profile(), &Profile::coreChanged, this, &ToxService::onCoreChanged);
    connect(profile(), &Profile::failedToStart, this, [this] {
        emit error(tr("Toxcore failed to start"), tr("Toxcore failed to start with your profile."));
    });
    connect(profile(), &Profile::badProxy, this, [this] {
        emit error(tr("Toxcore failed to start"),
                   tr("Your proxy settings are invalid. Please check them."));
    });
    setProfileName(profile()->getName());
    session->start();

    readSettings();
    updateFriendRequests();
    setLoggedIn(true);
}

void ToxService::onCoreChanged(Core& newCore)
{
    core = &newCore;
    chatList.setSelf(core->getSelfPublicKey());
    setSelfName(core->getUsername());
    setSelfId(core->getSelfId().toString());
    setSelfStatusMessage(core->getStatusMessage());
    setSelfStatus(static_cast<int>(core->getStatus()));

    connect(core, &Core::connectionChanged, this, [this](Core::Connection newConnection) {
        connection = newConnection;
        setOnline(connection != Core::Connection::None);
        updateStatus();
    });
    connect(core, &Core::usernameSet, this, [this](const QString& name) {
        chatManager()->getSharedMessageProcessorParams().onUserNameSet(name);
        setSelfName(name);
    });
    connect(core, &Core::idSet, this, [this](const ToxId& id) {
        setSelfId(id.toString());
        // Core doesn't save it.
        saveSoon();
    });
    connect(core, &Core::statusMessageSet, this, &ToxService::setSelfStatusMessage);
    connect(core, &Core::statusSet, this,
            [this](Status::Status status) { setSelfStatus(static_cast<int>(status)); });
    connect(core, &Core::friendTypingChanged, this, [this](uint32_t friendId, bool typing) {
        chatList.setTyping(session->getFriendList().id2Key(friendId).toString(), typing);
    });
    connect(core, &Core::groupPasswordRejected, this,
            [this] { emit error(tr("Couldn't join the group"), tr("Wrong password.")); });
    connect(core, &Core::failedToAddFriend, this, [this](const ToxPk& friendPk, const QString& errorInfo) {
        emit error(tr("Couldn't add friend"), errorInfo.isEmpty() ? friendPk.toString() : errorInfo);
    });
}

void ToxService::onMessageReceived(Chat& chat)
{
    countUnread(ChatKind::of(chat), chat.getPersistentId().toString());
}

void ToxService::onInviteAdded(const InviteManager::Invite& invite, const ToxPk& friendPk)
{
    const Friend* f = findFriend(friendPk);
    invites.add(invite, f->getDisplayedName());
    setInviteCount(invites.rowCount());
}

QString ToxService::receivedFilesDir() const
{
#ifdef Q_OS_ANDROID
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/received";
#else
    return settings.getGlobalAutoAcceptDir();
#endif
}

Chat* ToxService::findChat(ChatKind::Kind kind, const QString& id) const
{
    switch (kind) {
    case ChatKind::Kind::Friend:
        return findFriend(ToxPk{id});
    case ChatKind::Kind::Conference:
        return findConference(ConferenceId::fromString(id));
    case ChatKind::Kind::Group:
        return findGroup(GroupId::fromString(id));
    }
    return nullptr;
}

IChatLog* ToxService::findChatLog(ChatKind::Kind kind, const QString& id) const
{
    const Chat* chat = findChat(kind, id);
    return chat != nullptr ? chatManager()->getChatLog(*chat) : nullptr;
}

ChatLogIdx ToxService::firstShown(ChatKind::Kind kind, const QString& id, const IChatLog& chatLog) const
{
    return clearedFrom.value(RemoteModels::chat(kind, id), chatLog.getFirstIdx());
}

IMessageDispatcher* ToxService::findDispatcher(ChatKind::Kind kind, const QString& id) const
{
    const Chat* chat = findChat(kind, id);
    return chat != nullptr ? chatManager()->getDispatcher(*chat) : nullptr;
}

void ToxService::setActiveChat(int kind, const QString& id)
{
    const std::optional<ChatKind::Kind> chatKind = ChatKind::fromInt(kind);
    activeChat = chatKind ? RemoteModels::chat(*chatKind, id) : QString{};
    if (activeChat.isEmpty()) {
        return;
    }

    chatList.markRead(*chatKind, id);
    if (notifications != nullptr) {
        notifications->shown(activeChat);
    }
}

QString ToxService::describeStatus() const
{
    if (!loggedIn()) {
        return tr("Not logged in");
    }
    switch (connection) {
    case Core::Connection::None:
        return tr("Connecting…");
    case Core::Connection::Udp:
        return tr("Online");
    case Core::Connection::TcpRelay:
        return tr("Online via TCP relay");
    }
    return {};
}

void ToxService::updateStatus()
{
    const QString newStatus = describeStatus();
    if (newStatus != shownStatus) {
        shownStatus = newStatus;
        notifier.showStatus(shownStatus);
    }
}

void ToxService::saveSoon()
{
    if (savePending) {
        return;
    }
    savePending = true;
    QTimer::singleShot(0, this, [this] {
        savePending = false;
        if (profile() != nullptr) {
            profile()->onSaveToxSave();
        }
    });
}

void ToxService::closeChat(ChatKind::Kind kind, const QString& id)
{
    if (ChatModel* chat = chats.take(RemoteModels::chat(kind, id))) {
        host.disableRemoting(chat);
        delete chat;
    }
}

void ToxService::countUnread(ChatKind::Kind kind, const QString& id)
{
    if (RemoteModels::chat(kind, id) != activeChat) {
        chatList.addUnread(kind, id);
    }
}

void ToxService::updateProfiles()
{
    const QStringList names = Profile::getAllProfileNames(settings.getPaths());
    QStringList encrypted;
    for (const QString& name : names) {
        if (Profile::isEncrypted(name, settings.getPaths())) {
            encrypted.append(name);
        }
    }
    setProfiles(names);
    setEncryptedProfiles(encrypted);
}

void ToxService::updateFriendRequests()
{
    friendRequests.reload();
    setFriendRequestCount(settings.getFriendRequestSize());
}
