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
#include "src/model/callduration.h"
#include "src/model/chathistory.h"
#include "src/model/chatmanager.h"
#include "src/model/conference.h"
#include "src/model/conferenceinvite.h"
#include "src/model/friend.h"
#include "src/model/friendmessagedispatcher.h"
#include "src/model/group.h"
#include "src/model/groupinvite.h"
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
#include "src/persistence/profilelocker.h"
#include "src/persistence/settings.h"
#include "src/qml/chatmodel.h"
#include "src/qml/files.h"
#include "src/qml/inotifier.h"
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
#include <QSaveFile>
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

/** A conference's ID as the user interface says it, or none's if it isn't one. */
ConferenceId toConferenceId(const QString& id)
{
    const QByteArray raw = QByteArray::fromHex(id.toLatin1());
    return raw.size() == ConferenceId::size ? ConferenceId{raw} : ConferenceId{};
}

/** A group's ID as the user interface says it, or none's if it isn't one. */
GroupId toGroupId(const QString& id)
{
    const QByteArray raw = QByteArray::fromHex(id.toLatin1());
    return raw.size() == GroupId::size ? GroupId{raw} : GroupId{};
}

/** Whether @p text is an action, as "/me waves" is, and what it says without the prefix. */
std::pair<bool, QString> parseAction(const QString& text)
{
    const bool isAction = text.startsWith(Message::ACTION_PREFIX, Qt::CaseInsensitive);
    return {isAction, isAction ? text.mid(Message::ACTION_PREFIX.size()) : text};
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

/** The cameras, without CameraDevice's none. */
QVector<QPair<QString, QString>> cameras()
{
    QVector<QPair<QString, QString>> devices = CameraDevice::getDeviceList();
    devices.removeIf(
        [](const QPair<QString, QString>& device) { return device.first == CameraDevice::NONE; });
    return devices;
}

quint64 fileKey(uint32_t friendId, uint32_t fileNum)
{
    return (static_cast<quint64>(friendId) << 32) | fileNum;
}

bool isDone(ToxFile::FileStatus status)
{
    return status == ToxFile::CANCELED || status == ToxFile::FINISHED || status == ToxFile::BROKEN;
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
    if (profile != nullptr) {
        return;
    }

    // As the Widgets UI says it.
    const QString failure = tr("Couldn't create a new profile");
    if (Profile::exists(name, settings.getPaths())) {
        emit error(failure, tr("A profile with this name already exists."));
        return;
    }
    std::unique_ptr<Profile> created =
        Profile::createProfile(name, password, settings, nullptr, cameraSource, messageBoxManager);
    if (created == nullptr) {
        emit error(failure, tr("Its Tox save file couldn't be written."));
        return;
    }
    start(std::move(created), autoLogin);
    updateProfiles();
}

void ToxService::loadProfile(const QString& name, const QString& password, bool autoLogin)
{
    if (profile != nullptr) {
        return;
    }

    // As the Widgets UI says it, which says "Wrong password." also for profiles without one.
    const QString failure = tr("Couldn't load this profile");
    Paths& paths = settings.getPaths();
    if (!ProfileLocker::isLockable(name, paths)) {
        emit error(failure, tr("This profile is already in use."));
        return;
    }
    std::unique_ptr<Profile> loaded =
        Profile::loadProfile(name, password, settings, nullptr, cameraSource, messageBoxManager);
    if (loaded == nullptr) {
        emit error(failure, Profile::isEncrypted(name, paths)
                                ? tr("Wrong password.")
                                : tr("Its Tox save file couldn't be read."));
        return;
    }
    start(std::move(loaded), autoLogin);
}

void ToxService::logout()
{
    // Whoever logs out stays logged out, also when Android restarts the service.
    settings.setAutoLogin(false);
    closeProfile();
}

void ToxService::setPassword(const QString& password)
{
    if (profile == nullptr) {
        return;
    }

    const QString failure = profile->setPassword(password);
    updateProfiles();
    if (!failure.isEmpty()) {
        emit error(tr("Couldn't change password"), failure);
    }
}

void ToxService::renameProfile(const QString& name)
{
    if (profileInfo == nullptr) {
        return;
    }

    switch (profileInfo->renameProfile(name)) {
    case IProfileInfo::RenameResult::OK:
        setProfileName(profile->getName());
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
    if (profileInfo == nullptr) {
        return;
    }

    const QStringList left = profileInfo->removeProfile();
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
    if (profile == nullptr) {
        return {};
    }

    // Also what changed since Core last asked to save.
    profile->onSaveToxSave();
    QFile save{settings.getPaths().getSettingsDirPath() + profile->getName() + Core::TOX_EXT};
    if (!save.open(QIODevice::ReadOnly)) {
        emit error(tr("Failed to save file"), tr("Can't read %1.").arg(save.fileName()));
        return {};
    }
    return save.readAll();
}

void ToxService::importProfile(const QString& name, const QByteArray& save)
{
    // The name comes from a file name, so it can't be a path.
    if (name.isEmpty() || name.contains('/') || name.contains('\\') || save.isEmpty()) {
        emit error(tr("Couldn't import the profile"), tr("This is not a Tox save file."));
        return;
    }
    if (profile != nullptr && profile->getName() == name) {
        emit error(tr("Couldn't import the profile"), tr("The profile \"%1\" is in use.").arg(name));
        return;
    }

    QSaveFile file{settings.getPaths().getSettingsDirPath() + name + Core::TOX_EXT};
    if (!file.open(QIODevice::WriteOnly) || file.write(save) != save.size() || !file.commit()) {
        emit error(tr("Couldn't import the profile"), tr("Can't write %1.").arg(file.fileName()));
        return;
    }
    updateProfiles();
}

void ToxService::closeProfile()
{
    if (profile == nullptr) {
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
    offeredFiles.clear();
    fileProgressShown.clear();
    invites.clear();
    setInviteCount(0);
    for (const QString& tag : std::as_const(shownNotifications)) {
        notifier.cancel(tag);
    }
    shownNotifications.clear();
    notifications.reset();
    chatManager.reset();
    friendList.clear();
    conferenceList.clear();
    groupList.clear();
    profileInfo.reset();
    profile->save();
    profile.reset();
    audio.reset();
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
    if (profile != nullptr && !avatar.isEmpty()
        && ToxClientStandards::IsValidAvatarSize(avatar.size())) {
        profile->setAvatar(avatar);
    }
}

void ToxService::removeAvatar()
{
    if (profile != nullptr) {
        profile->removeSelfAvatar();
    }
}

void ToxService::updateSelfAvatar()
{
    // The profile saved it first.
    const QByteArray avatar = profile->loadAvatarData(profile->getCore().getSelfPublicKey());
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
    const int row = findFriendRequest(publicKey);
    if (core == nullptr || row < 0) {
        return;
    }

    core->acceptFriendRequest(ToxPk{publicKey});
    settings.removeFriendRequest(row);
    settings.savePersonal();
    updateFriendRequests();
}

void ToxService::rejectFriendRequest(const QString& publicKey)
{
    const int row = findFriendRequest(publicKey);
    if (row < 0) {
        return;
    }

    settings.removeFriendRequest(row);
    settings.savePersonal();
    updateFriendRequests();
}

void ToxService::setFriendAlias(const QString& publicKey, const QString& alias)
{
    const ToxPk friendPk{publicKey};
    Friend* f = friendList.findFriend(friendPk);
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
    Friend* f = friendList.findFriend(friendPk);
    if (f == nullptr || chatManager == nullptr) {
        return;
    }

    if (History* history = profile->getHistory(); removeHistory && history != nullptr) {
        history->removeChatHistory(f->getPersistentId());
    }

    // What refers to the chat log goes first, as in the Widgets UI.
    cancelNotification(*f);
    closeChat(ChatKind::Kind::Friend, publicKey);
    chatList.removeChat(ChatKind::Kind::Friend, publicKey);
    chatManager->removeFriend(friendPk);
    friendList.removeFriend(friendPk, settings, false);
    settings.savePersonal();
    delete f;
}

void ToxService::removeHistory(const QString& publicKey)
{
    const Friend* f = friendList.findFriend(ToxPk{publicKey});
    History* history = profile != nullptr ? profile->getHistory() : nullptr;
    IChatLog* chatLog = findChatLog(ChatKind::Kind::Friend, publicKey);
    if (f == nullptr || history == nullptr || chatLog == nullptr) {
        return;
    }

    history->removeChatHistory(f->getPersistentId());
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
    if (friendList.findFriend(friendPk) == nullptr) {
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
    if (core == nullptr || !invite) {
        return;
    }
    if (const auto* conference = std::get_if<ConferenceInvite>(&invite->invite)) {
        core->joinConference(*conference);
    } else if (const auto* group = std::get_if<GroupInvite>(&invite->invite)) {
        core->joinGroup(*group, password.toUtf8());
    }
}

void ToxService::rejectInvite(const QString& id)
{
    invites.take(id);
    setInviteCount(invites.rowCount());
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
    if (dispatcher == nullptr || text.trimmed().isEmpty()) {
        return;
    }

    const auto [isAction, content] = parseAction(text);
    dispatcher->sendMessage(isAction, content);
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
    const Friend* f = friendList.findFriend(ToxPk{publicKey});
    if (f == nullptr || core == nullptr) {
        return;
    }
    const auto offered = offeredFiles.constFind(fileKey(f->getId(), static_cast<uint32_t>(fileNum)));
    if (offered == offeredFiles.cend()) {
        return;
    }

    receiveFile(*offered, receivedFilesDir());
}

void ToxService::receiveFile(const ToxFile& file, const QString& dir)
{
    if (!QDir{}.mkpath(dir)) {
        emit error(tr("Couldn't receive the file"), tr("Can't write to %1.").arg(dir));
        return;
    }
    core->getCoreFile()->acceptFileRecvRequest(file.friendId, file.fileNum,
                                               Files::uniquePath(dir, file.fileName));
}

void ToxService::cancelFile(const QString& publicKey, int fileNum, bool sending)
{
    const Friend* f = friendList.findFriend(ToxPk{publicKey});
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
    const Friend* f = friendList.findFriend(ToxPk{publicKey});
    if (f != nullptr && core != nullptr) {
        core->getCoreFile()->pauseResumeFile(f->getId(), static_cast<uint32_t>(fileNum));
    }
}

void ToxService::sendFile(const QString& publicKey, const QString& path)
{
    const Friend* f = friendList.findFriend(ToxPk{publicKey});
    const QFileInfo file{path};
    if (f == nullptr || core == nullptr) {
        return;
    }
    if (!file.isReadable()) {
        emit error(tr("Couldn't send the file"), tr("Can't read %1.").arg(path));
        return;
    }
    core->getCoreFile()->sendFile(f->getId(), file.fileName(), path, file.size());
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
    const Conference* conference = conferenceList.findConference(toConferenceId(id));
    const Friend* f = friendList.findFriend(ToxPk{publicKey});
    if (core == nullptr || conference == nullptr || f == nullptr) {
        return;
    }
    core->conferenceInviteFriend(f->getId(), static_cast<int>(conference->getId()));
}

void ToxService::setConferenceTitle(const QString& id, const QString& title)
{
    const Conference* conference = conferenceList.findConference(toConferenceId(id));
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
    const GroupId groupId = toGroupId(id);
    if (core == nullptr || groupId.isEmpty() || core->joinGroup(groupId, password.toUtf8()) < 0) {
        return {};
    }
    return groupId.toString();
}

void ToxService::inviteToGroup(const QString& id, const QString& publicKey)
{
    const Group* group = groupList.findGroup(toGroupId(id));
    const Friend* f = friendList.findFriend(ToxPk{publicKey});
    if (core == nullptr || group == nullptr || f == nullptr) {
        return;
    }
    core->groupInviteFriend(f->getId(), static_cast<int>(group->getId()));
}

void ToxService::setGroupTopic(const QString& id, const QString& topic)
{
    const Group* group = groupList.findGroup(toGroupId(id));
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
    const GroupId groupId = toGroupId(id);
    const Group* group = groupList.findGroup(groupId);
    GroupMessageDispatcher* dispatcher =
        chatManager != nullptr ? chatManager->getGroupDispatcher(groupId) : nullptr;
    if (group == nullptr || dispatcher == nullptr || text.trimmed().isEmpty()) {
        return;
    }
    // Unless they left.
    const ToxPk recipient{publicKey};
    if (!group->getPeerList().contains(recipient)) {
        return;
    }
    const auto [isAction, content] = parseAction(text);
    dispatcher->sendPrivateMessage(recipient, isAction, content);
}

void ToxService::changeGroup(const QString& id, const std::function<bool(Group&)>& change)
{
    Group* group = groupList.findGroup(toGroupId(id));
    if (core != nullptr && group != nullptr && !change(*group)) {
        emit error(tr("Couldn't change the group"), tr("Tox refused the change."));
    }
}

void ToxService::setAutoAcceptInvites(const QString& publicKey, bool accept)
{
    const ToxPk friendPk{publicKey};
    if (friendList.findFriend(friendPk) == nullptr) {
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
    if (friendList.findFriend(friendPk) == nullptr) {
        return;
    }
    settings.setAutoAcceptDir(friendPk, accept ? receivedFilesDir() : QString{});
    settings.savePersonal();
    chatList.setAutoAcceptFiles(publicKey, accept);
}

void ToxService::setTyping(const QString& publicKey, bool typing)
{
    const Friend* f = friendList.findFriend(ToxPk{publicKey});
    // Once off, also no "stopped typing".
    if (core != nullptr && f != nullptr && settings.getTypingNotification()) {
        core->sendTyping(f->getId(), typing);
    }
}

void ToxService::eraseHistory()
{
    if (History* history = profile != nullptr ? profile->getHistory() : nullptr) {
        history->eraseHistory();
    }
}

void ToxService::startCall(const QString& publicKey, bool video)
{
    Friend* f = friendList.findFriend(ToxPk{publicKey});
    if (core == nullptr || f == nullptr || callState() != static_cast<int>(CallState::State::None)) {
        return;
    }
    if (video) {
        chooseCamera();
    }
    if (!core->getAv()->startCall(f->getId(), video)) {
        return;
    }
    setCall(f, CallState::State::Outgoing);
    setSendsVideo(video);
    addSystemMessage(*f, SystemMessageType::outgoingCall, {f->getDisplayedName()});
    playSound(IAudioSink::Sound::OutgoingCall, true);
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
    if (core->getAv()->answerCall(f->getId(), sendsVideo)) {
        setSendsVideo(sendsVideo);
        // CoreAV says nothing more when the user answers.
        onAvStart(f->getId(), sendsVideo);
    }
}

void ToxService::joinConferenceCall(const QString& id)
{
    const Conference* conference = conferenceList.findConference(toConferenceId(id));
    if (core == nullptr || conference == nullptr || !conference->isAvConference()
        || callState() != static_cast<int>(CallState::State::None)) {
        return;
    }
    core->getAv()->joinConferenceCall(*conference);
    setCall(ChatKind::Kind::Conference, id, CallState::State::Active);
}

void ToxService::endCall()
{
    // CoreAV says when a friend's call ended.
    if (const Friend* f = callingFriend()) {
        core->getAv()->cancelCall(f->getId());
    } else if (const Conference* conference = callingConference()) {
        core->getAv()->leaveConferenceCall(static_cast<int>(conference->getId()));
        clearCall();
    }
}

void ToxService::toggleCallMic()
{
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
    CoreAV& av = *core->getAv();
    if (const Friend* f = callingFriend()) {
        av.toggleMuteCallOutput(f);
        setCallSpeakerMuted(av.isCallOutputMuted(f));
    } else if (const Conference* conference = callingConference()) {
        av.muteCallOutput(conference, !av.isConferenceCallOutputMuted(conference));
        setCallSpeakerMuted(av.isConferenceCallOutputMuted(conference));
    }
}

/** As the Widgets UI's chat form does it. */
void ToxService::onFriendStatusChanged(const Friend& f, Status::Status status)
{
    if (status == Status::Status::Offline) {
        // Toxcore doesn't say that they stopped.
        chatList.setTyping(f.getPublicKey().toString(), false);
        if (callingFriend() == &f) {
            const bool active = callState() == static_cast<int>(CallState::State::Active);
            const auto seconds =
                static_cast<quint32>(callStarted().secsTo(QDateTime::currentDateTime()));
            // So that onAvEnd() finds no call: these messages say how it ended.
            cancelNotification(f);
            clearCall();
            core->getAv()->cancelCall(f.getId());
            if (active) {
                addSystemMessage(f, SystemMessageType::userWentOfflineDuringCall,
                                 {f.getDisplayedName(), secondsToDHMS(seconds)});
            } else {
                addSystemMessage(f, SystemMessageType::userWentOffline, {f.getDisplayedName()});
            }
        }
    }

    if (settings.getStatusChangeNotificationEnabled()) {
        addSystemMessage(f, SystemMessageType::peerStateChange,
                         {f.getDisplayedName(), Status::getTitle(status)});
    }
}

void ToxService::onAvInvite(uint32_t friendId, bool video)
{
    const Friend* f = friendList.findFriend(friendList.id2Key(friendId));
    if (f == nullptr) {
        return;
    }
    // One call at a time.
    if (callState() != static_cast<int>(CallState::State::None)) {
        core->getAv()->cancelCall(friendId);
        return;
    }

    setCall(f, CallState::State::Incoming);
    setCallOffersVideo(video);
    addSystemMessage(*f, SystemMessageType::incomingCall, {f->getDisplayedName()});
    playSound(IAudioSink::Sound::IncomingCall, true);
    notify(RemoteModels::chat(ChatKind::Kind::Friend, f->getPublicKey().toString()),
           {f->getDisplayedName(),
            video ? tr("Incoming video call") : tr("Incoming call"),
            {},
            profile->loadAvatar(f->getPublicKey())},
           INotifier::Actions::AnswerOrDecline);
}

void ToxService::onAvStart(uint32_t friendId, bool video)
{
    std::ignore = video;
    const Friend* f = callingFriend();
    if (f == nullptr || f->getId() != friendId) {
        return;
    }
    cancelNotification(*f);
    setCall(f, CallState::State::Active);
    watchCallVideo(*f);
}

void ToxService::watchCallVideo(const Friend& f)
{
    // An audio call has none.
    VideoSource* source = core->getAv()->getVideoSourceFromCall(f.getId());
    if (source == nullptr) {
        return;
    }
    callVideoSource = source;
    // Only then does it pass on frames.
    source->subscribe();
    connect(source, &VideoSource::frameAvailable, this, &ToxService::onCallVideoFrame);
    connect(source, &VideoSource::sourceStopped, this, [this] { setCallVideo(false); });
}

void ToxService::stopCallVideo()
{
    if (callVideoSource != nullptr) {
        disconnect(callVideoSource, nullptr, this, nullptr);
        // Once the call is over, this deletes it.
        callVideoSource->unsubscribe();
        callVideoSource = nullptr;
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

void ToxService::cancelNotification(const Friend& f)
{
    const QString tag = RemoteModels::chat(ChatKind::Kind::Friend, f.getPublicKey().toString());
    if (shownNotifications.remove(tag)) {
        notifier.cancel(tag);
    }
}

void ToxService::onAvEnd(uint32_t friendId, bool error)
{
    const Friend* f = callingFriend();
    if (f == nullptr || f->getId() != friendId) {
        return;
    }
    // As the Widgets UI's chat form says it.
    if (callState() == static_cast<int>(CallState::State::Active)) {
        const auto seconds = static_cast<quint32>(callStarted().secsTo(QDateTime::currentDateTime()));
        addSystemMessage(*f, error ? SystemMessageType::unexpectedCallEnd : SystemMessageType::callEnd,
                         {f->getDisplayedName(), secondsToDHMS(seconds)});
    }
    cancelNotification(*f);
    clearCall();
    playSound(IAudioSink::Sound::CallEnd, false);
}

Friend* ToxService::callingFriend()
{
    return callChat().isEmpty() || core == nullptr
                   || callKind() != static_cast<int>(ChatKind::Kind::Friend)
               ? nullptr
               : friendList.findFriend(ToxPk{callChat()});
}

Conference* ToxService::callingConference()
{
    return callChat().isEmpty() || core == nullptr
                   || callKind() != static_cast<int>(ChatKind::Kind::Conference)
               ? nullptr
               : conferenceList.findConference(toConferenceId(callChat()));
}

void ToxService::setCall(const Friend* f, CallState::State state)
{
    setCall(ChatKind::Kind::Friend, f->getPublicKey().toString(), state);
}

void ToxService::setCall(ChatKind::Kind kind, const QString& id, CallState::State state)
{
    // A new state stops the ringing.
    callSound.reset();
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

void ToxService::addSystemMessage(const Friend& f, SystemMessageType type,
                                  const SystemMessage::Args& args)
{
    if (IChatLog* chatLog = chatManager->getFriendChatLog(f.getPublicKey())) {
        chatLog->addSystemMessage({type, QDateTime::currentDateTime(), args});
    }
}

void ToxService::playSound(IAudioSink::Sound sound, bool loop)
{
    // As the Widgets UI decides.
    const bool busy = core->getStatus() == Status::Status::Busy;
    if (!settings.getAudioOutDevEnabled() || !settings.getNotifySound()
        || (busy && !settings.getBusySound())) {
        return;
    }
    callSound = audio->makeSink();
    if (callSound == nullptr) {
        return;
    }
    callSound->playMono16Sound(sound);
    if (loop) {
        callSound->startLoop();
    }
}

void ToxService::leaveConference(const QString& id)
{
    if (chatManager == nullptr) {
        return;
    }

    const ConferenceId conferenceId = toConferenceId(id);
    Conference* conference = conferenceList.findConference(conferenceId);
    if (conference == nullptr) {
        return;
    }

    // What refers to the conference goes first, as in the Widgets UI.
    if (callingConference() == conference) {
        endCall();
    }
    closeChat(ChatKind::Kind::Conference, id);
    chatList.removeChat(ChatKind::Kind::Conference, id);
    chatManager->removeConference(conferenceId);
    conferenceList.removeConference(conferenceId);
    delete conference;
}

void ToxService::leaveGroup(const QString& id)
{
    removeGroup(toGroupId(id), &ChatManager::removeGroup);
}

void ToxService::removeGroup(const GroupId& groupId, void (ChatManager::*removeModel)(const GroupId&))
{
    Group* group = groupList.findGroup(groupId);
    if (chatManager == nullptr || group == nullptr) {
        return;
    }

    // What refers to the group goes first, as in the Widgets UI.
    const QString id = groupId.toString();
    closeChat(ChatKind::Kind::Group, id);
    chatList.removeChat(ChatKind::Kind::Group, id);
    (chatManager.get()->*removeModel)(groupId);
    groupList.removeGroup(groupId);
    delete group;
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

void ToxService::start(std::unique_ptr<Profile> newProfile, bool autoLogin)
{
    profile = std::move(newProfile);
    // Logging in needs the password of a profile that has one.
    settings.setAutoLogin(autoLogin && !profile->isEncrypted());
    audio = Audio::makeAudio(settings);
    profile->getCore().getAv()->setAudio(*audio);

    chatManager = std::make_unique<ChatManager>(*profile, settings, friendList, conferenceList,
                                                groupList, &dialogsManager);
    notifications = std::make_unique<NotificationGenerator>(settings, profile.get());
    connect(chatManager.get(), &ChatManager::friendAdded, this,
            [this](Friend* f, std::shared_ptr<FriendChatroom> chatroom,
                   std::shared_ptr<FriendMessageDispatcher> dispatcher,
                   std::shared_ptr<ChatHistory> chatLog) {
                std::ignore = chatroom;
                chatList.addFriend(f, *chatLog);
                chatList.setAvatar(f->getPublicKey().toString(),
                                   profile->loadAvatarData(f->getPublicKey()));
                chatList.setAutoAcceptInvites(f->getPublicKey().toString(),
                                              settings.getAutoConferenceInvite(f->getPublicKey()));
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
                connect(dispatcher.get(), &IMessageDispatcher::messageReceived, this,
                        [this, f](const ToxPk& sender, const Message& message) {
                            std::ignore = sender;
                            const QString id = f->getPublicKey().toString();
                            countUnread(ChatKind::Kind::Friend, id);
                            notify(RemoteModels::chat(ChatKind::Kind::Friend, id),
                                   notifications->friendMessageNotification(f, message.content),
                                   INotifier::Actions::Reply);
                        });
            });
    connect(chatManager.get(), &ChatManager::conferenceAdded, this,
            [this](Conference* conference, std::shared_ptr<ConferenceRoom> chatroom,
                   std::shared_ptr<ConferenceMessageDispatcher> dispatcher,
                   std::shared_ptr<IChatLog> chatLog) {
                std::ignore = chatroom;
                chatList.addConference(conference, *chatLog);
                connect(dispatcher.get(), &IMessageDispatcher::messageReceived, this,
                        [this, conference](const ToxPk& sender, const Message& message) {
                            const QString id = conference->getPersistentId().toString();
                            countUnread(ChatKind::Kind::Conference, id);
                            // As the Widgets UI decides.
                            if (message.mentionsSelf() || settings.getConferenceAlwaysNotify()) {
                                notify(RemoteModels::chat(ChatKind::Kind::Conference, id),
                                       notifications->conferenceMessageNotification(conference, sender,
                                                                                    message.content),
                                       INotifier::Actions::Reply);
                            }
                        });
            });
    connect(chatManager.get(), &ChatManager::groupAdded, this,
            [this](Group* group, std::shared_ptr<GroupRoom> chatroom,
                   std::shared_ptr<GroupMessageDispatcher> dispatcher,
                   std::shared_ptr<IChatLog> chatLog) {
                std::ignore = chatroom;
                chatList.addGroup(group, *chatLog);
                connect(dispatcher.get(), &IMessageDispatcher::messageReceived, this,
                        [this, group](const ToxPk& sender, const Message& message) {
                            const QString id = group->getPersistentId().toString();
                            countUnread(ChatKind::Kind::Group, id);
                            // As the Widgets UI decides, and always for the user alone. A
                            // reply from the notification would go to everyone.
                            const bool isPrivate = !message.recipient.isEmpty();
                            if (isPrivate || message.mentionsSelf()
                                || settings.getConferenceAlwaysNotify()) {
                                notify(RemoteModels::chat(ChatKind::Kind::Group, id),
                                       notifications->groupMessageNotification(group, sender,
                                                                               message.content),
                                       isPrivate ? INotifier::Actions::None
                                                 : INotifier::Actions::Reply);
                            }
                        });
            });
    // A group whose password Tox rejected: ChatManager quit it.
    connect(chatManager.get(), &ChatManager::groupRemoved, this,
            [this](const GroupId& groupId) { removeGroup(groupId, &ChatManager::removeGroupModel); });

    connect(profile.get(), &Profile::selfAvatarChanged, this, &ToxService::updateSelfAvatar);
    // Profile says it before it saves the avatar, which it may refuse.
    connect(
        profile.get(), &Profile::friendAvatarChanged, this,
        [this](const ToxPk& friendPk) {
            if (profile != nullptr) {
                chatList.setAvatar(friendPk.toString(), profile->loadAvatarData(friendPk));
            }
        },
        Qt::QueuedConnection);
    connect(profile.get(), &Profile::coreChanged, this, &ToxService::onCoreChanged);
    connect(profile.get(), &Profile::failedToStart, this, [this] {
        emit error(tr("Toxcore failed to start"), tr("Toxcore failed to start with your profile."));
    });
    connect(profile.get(), &Profile::badProxy, this, [this] {
        emit error(tr("Toxcore failed to start"),
                   tr("Your proxy settings are invalid. Please check them."));
    });
    profileInfo = std::make_unique<ProfileInfo>(&profile->getCore(), profile.get(), settings);
    setProfileName(profile->getName());
    profile->startCore();

    readSettings();
    updateFriendRequests();
    setLoggedIn(true);
}

void ToxService::onCoreChanged(Core& newCore)
{
    core = &newCore;
    chatList.setSelf(core->getSelfPublicKey());
    chatManager->connectToCore(newCore);
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
        chatManager->getSharedMessageProcessorParams().onUserNameSet(name);
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
    connect(core, &Core::friendRequestReceived, this, &ToxService::onFriendRequestReceived);
    connect(core->getAv(), &CoreAV::avInvite, this, &ToxService::onAvInvite);
    connect(core->getAv(), &CoreAV::avStart, this, &ToxService::onAvStart);
    connect(core->getAv(), &CoreAV::avEnd, this, &ToxService::onAvEnd);
    connect(core, &Core::friendTypingChanged, this, [this](uint32_t friendId, bool typing) {
        chatList.setTyping(friendList.id2Key(friendId).toString(), typing);
    });
    const CoreFile* coreFile = core->getCoreFile();
    for (const auto signal : {&CoreFile::fileSendStarted, &CoreFile::fileReceiveRequested,
                              &CoreFile::fileTransferAccepted, &CoreFile::fileTransferCancelled,
                              &CoreFile::fileTransferFinished, &CoreFile::fileTransferPaused}) {
        connect(coreFile, signal, this, &ToxService::onFileUpdated);
    }
    connect(coreFile, &CoreFile::fileTransferInfo, this, &ToxService::onFileProgress);
    connect(coreFile, &CoreFile::fileTransferRemotePausedUnpaused, this, &ToxService::onFileUpdated);
    connect(coreFile, &CoreFile::fileTransferBrokenUnbroken, this, &ToxService::onFileUpdated);
    connect(coreFile, &CoreFile::fileSendFailed, this, &ToxService::onFileSendFailed);
    connect(core, &Core::conferenceInviteReceived, this, &ToxService::onConferenceInviteReceived);
    connect(core, &Core::groupInviteReceived, this, &ToxService::onGroupInviteReceived);
    connect(core, &Core::groupPasswordRejected, this,
            [this] { emit error(tr("Couldn't join the group"), tr("Wrong password.")); });
    connect(core, &Core::failedToAddFriend, this, [this](const ToxPk& friendPk, const QString& errorInfo) {
        emit error(tr("Couldn't add friend"), errorInfo.isEmpty() ? friendPk.toString() : errorInfo);
    });
}

void ToxService::onFriendRequestReceived(const ToxPk& friendPk, const QString& message)
{
    if (settings.addFriendRequest(friendPk.toString(), message)) {
        updateFriendRequests();
        notify(RemoteModels::friendRequests,
               notifications->friendRequestNotification(friendPk, message));
    }
}

void ToxService::onConferenceInviteReceived(const ConferenceInvite& invite)
{
    const Friend* f = friendList.findFriend(friendList.id2Key(invite.getFriendId()));
    if (f == nullptr) {
        return;
    }

    if (settings.getAutoConferenceInvite(f->getPublicKey())) {
        core->joinConference(invite);
        return;
    }

    invites.add(invite, f->getDisplayedName());
    setInviteCount(invites.rowCount());
    notify(RemoteModels::invites, notifications->conferenceInvitationNotification(f));
}

void ToxService::onGroupInviteReceived(const GroupInvite& invite)
{
    const Friend* f = friendList.findFriend(friendList.id2Key(invite.getFriendId()));
    if (f == nullptr) {
        return;
    }

    // Without a password: if the group has one, Tox rejects it.
    if (settings.getAutoGroupInvite(f->getPublicKey())) {
        core->joinGroup(invite);
        return;
    }

    invites.add(invite, f->getDisplayedName());
    setInviteCount(invites.rowCount());
    notify(RemoteModels::invites, notifications->groupInvitationNotification(f));
}

void ToxService::onFileUpdated(const ToxFile& file)
{
    // Avatars aren't files the user sees.
    Friend* f = friendList.findFriend(friendList.id2Key(file.friendId));
    if (f == nullptr || core == nullptr || file.fileKind == TOX_FILE_KIND_AVATAR) {
        return;
    }

    const quint64 key = fileKey(file.friendId, file.fileNum);
    const bool offered = file.direction == ToxFile::RECEIVING && file.status == ToxFile::INITIALIZING;
    if (offered) {
        offeredFiles.insert(key, file);
    } else {
        offeredFiles.remove(key);
    }
    if (isDone(file.status)) {
        fileProgressShown.remove(key);
    }

    const ToxPk& sender =
        file.direction == ToxFile::SENDING ? core->getSelfPublicKey() : f->getPublicKey();
    chatManager->getFriendChatLog(f->getPublicKey())->onFileUpdated(sender, file);

    if (offered) {
        if (const QString dir = autoAcceptDir(f->getPublicKey(), file.progress.getFileSize());
            !dir.isEmpty()) {
            receiveFile(file, dir);
        }
        const QString id = f->getPublicKey().toString();
        countUnread(ChatKind::Kind::Friend, id);
        notify(RemoteModels::chat(ChatKind::Kind::Friend, id),
               notifications->fileTransferNotification(f, file.fileName, file.progress.getFileSize()));
    }
}

void ToxService::onFileProgress(const ToxFile& file)
{
    // Toxcore says it for every chunk, which would flood the user interface.
    constexpr auto interval = std::chrono::milliseconds{500};
    const auto now = std::chrono::steady_clock::now();
    auto& shown = fileProgressShown[fileKey(file.friendId, file.fileNum)];
    if (now - shown >= interval) {
        shown = now;
        onFileUpdated(file);
    }
}

void ToxService::onFileSendFailed(uint32_t friendId, const QString& fileName)
{
    const ToxPk friendPk = friendList.id2Key(friendId);
    if (IChatLog* chatLog = chatManager != nullptr ? chatManager->getFriendChatLog(friendPk) : nullptr) {
        chatLog->addSystemMessage(
            {SystemMessageType::fileSendFailed, QDateTime::currentDateTime(), {fileName}});
    }
}

/** On Android, inside what Qt's FileProvider shares, so that other apps can open them. */
QString ToxService::receivedFilesDir() const
{
#ifdef Q_OS_ANDROID
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/received";
#else
    return settings.getGlobalAutoAcceptDir();
#endif
}

QString ToxService::autoAcceptDir(const ToxPk& friendPk, uint64_t fileSize) const
{
    // As the Widgets UI decides: the friend's folder, else everyone's if on, up to the size.
    QString dir = settings.getAutoAcceptDir(friendPk);
    if (dir.isEmpty() && settings.getAutoSaveEnabled()) {
        dir = settings.getGlobalAutoAcceptDir();
    }
    const size_t maxSize = settings.getMaxAutoAcceptSize();
    if (dir.isEmpty() || (maxSize != 0 && fileSize > maxSize)) {
        return {};
    }
#ifdef Q_OS_ANDROID
    // Only there can other apps open them. A profile from a computer names its folders.
    return receivedFilesDir();
#else
    return dir;
#endif
}

IChatLog* ToxService::findChatLog(ChatKind::Kind kind, const QString& id) const
{
    if (chatManager == nullptr) {
        return nullptr;
    }

    switch (kind) {
    case ChatKind::Kind::Friend:
        return chatManager->getFriendChatLog(ToxPk{id});
    case ChatKind::Kind::Conference:
        return chatManager->getConferenceChatLog(toConferenceId(id));
    case ChatKind::Kind::Group:
        return chatManager->getGroupChatLog(toGroupId(id));
    }
    return nullptr;
}

ChatLogIdx ToxService::firstShown(ChatKind::Kind kind, const QString& id, const IChatLog& chatLog) const
{
    return clearedFrom.value(RemoteModels::chat(kind, id), chatLog.getFirstIdx());
}

IMessageDispatcher* ToxService::findDispatcher(ChatKind::Kind kind, const QString& id) const
{
    if (chatManager == nullptr) {
        return nullptr;
    }

    switch (kind) {
    case ChatKind::Kind::Friend:
        return chatManager->getFriendDispatcher(ToxPk{id});
    case ChatKind::Kind::Conference:
        return chatManager->getConferenceDispatcher(toConferenceId(id));
    case ChatKind::Kind::Group:
        return chatManager->getGroupDispatcher(toGroupId(id));
    }
    return nullptr;
}

void ToxService::reply(const QString& tag, const QString& text)
{
    const auto chat = RemoteModels::parseChat(tag);
    if (!chat) {
        return;
    }
    sendMessage(static_cast<int>(chat->first), chat->second, text);
    // The user read it, to answer it.
    chatList.markRead(chat->first, chat->second);
    if (shownNotifications.remove(tag)) {
        notifier.cancel(tag);
    }
}

void ToxService::setActiveChat(int kind, const QString& id)
{
    const std::optional<ChatKind::Kind> chatKind = ChatKind::fromInt(kind);
    activeChat = chatKind ? RemoteModels::chat(*chatKind, id) : QString{};
    if (activeChat.isEmpty()) {
        return;
    }

    chatList.markRead(*chatKind, id);
    if (shownNotifications.remove(activeChat)) {
        notifier.cancel(activeChat);
    }
    if (notifications != nullptr) {
        // Its message counts start again.
        notifications->onNotificationActivated();
    }
}

void ToxService::notify(const QString& tag, const NotificationData& data, INotifier::Actions actions)
{
    if (tag == activeChat || !(settings.getNotify() && settings.getDesktopNotify())) {
        return;
    }

    notifier.show(tag, data.title, data.message, data.image, actions);
    shownNotifications.insert(tag);
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
    if (newStatus != status) {
        status = newStatus;
        notifier.showStatus(status);
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
        if (profile != nullptr) {
            profile->onSaveToxSave();
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

int ToxService::findFriendRequest(const QString& publicKey) const
{
    for (int row = 0; row < settings.getFriendRequestSize(); ++row) {
        if (settings.getFriendRequest(row).address == publicKey) {
            return row;
        }
    }
    return -1;
}
