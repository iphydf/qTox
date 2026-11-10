/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "qmlapp.h"

#include "rep_toxservice_replica.h"

#include "src/core/groupid.h"
#include "src/core/toxid.h"
#include "src/core/toxpk.h"
#include "src/core/untrustedimage.h"
#include "src/model/identicon.h"
#include "src/model/profile/profileinfo.h"
#include "src/qml/avatarprovider.h"
#include "src/qml/callstate.h"
#include "src/qml/files.h"
#include "src/qml/qmlchat.h"
#include "src/qml/remotemodels.h"
#include "src/translator.h"
#include "src/version.h"

#include <QAbstractItemModelReplica>
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPermissions>
#include <QQmlEngine>
#include <QRemoteObjectNode>
#include <QRemoteObjectPendingCallWatcher>
#include <QSortFilterProxyModel>
#include <QSysInfo>

#ifdef Q_OS_ANDROID
#include <QJniObject>

#include <QtCore/qcoreapplication_platform.h>
#endif

#include <tox/tox.h>
#include <tuple>
#include <utility>

namespace {
QString deviceModel()
{
#ifdef Q_OS_ANDROID
    const QString maker =
        QJniObject::getStaticObjectField<jstring>("android/os/Build", "MANUFACTURER").toString();
    const QString model =
        QJniObject::getStaticObjectField<jstring>("android/os/Build", "MODEL").toString();
    return QStringLiteral("%1 %2").arg(maker, model);
#else
    return QSysInfo::currentCpuArchitecture();
#endif
}

QString systemName()
{
#ifdef Q_OS_ANDROID
    // Android's behaviour follows its API level more than its name.
    const jint sdk = QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
    return QStringLiteral("%1 (API %2)").arg(QSysInfo::prettyProductName()).arg(sdk);
#else
    return QSysInfo::prettyProductName();
#endif
}

// What open() names what another app shared, and answering the call: no remotemodels.h model's
// names. QtoxService.java says the second too.
const QString shareRequest = QStringLiteral("share");
const QString answerRequest = QStringLiteral("answer");

/**
 * @brief Chats with the latest message first, then those without messages by name.
 */
class RecentFirst : public QSortFilterProxyModel
{
public:
    explicit RecentFirst(QAbstractItemModel& chats)
    {
        setSourceModel(&chats);
        sort(0);
    }

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        const QHash<int, QByteArray> roles = sourceModel()->roleNames();
        const QDateTime leftTime = left.data(roles.key("lastTime")).toDateTime();
        const QDateTime rightTime = right.data(roles.key("lastTime")).toDateTime();
        if (leftTime != rightTime) {
            // An invalid time is earlier than any other.
            return leftTime > rightTime;
        }
        const int nameRole = roles.key("name");
        return QString::localeAwareCompare(left.data(nameRole).toString(),
                                           right.data(nameRole).toString())
               < 0;
    }
};
} // namespace

QmlApp::QmlApp(QRemoteObjectNode& node_, QObject* parent)
    : QObject(parent)
    , node{node_}
    , service{node.acquire<ToxServiceReplica>()}
    , chatList{node.acquireModel(RemoteModels::chats, QtRemoteObjects::PrefetchData)}
    , recentChats{std::make_unique<RecentFirst>(*chatList)}
    , foundChats{std::make_unique<QSortFilterProxyModel>()}
    , friendRequests{node.acquireModel(RemoteModels::friendRequests, QtRemoteObjects::PrefetchData)}
    , invites{node.acquireModel(RemoteModels::invites, QtRemoteObjects::PrefetchData)}
{
    foundChats->setSourceModel(recentChats.get());
    foundChats->setFilterCaseSensitivity(Qt::CaseInsensitive);
    connect(service.get(), &QRemoteObjectReplica::stateChanged, this, &QmlApp::connectedChanged);
    connect(service.get(), &QRemoteObjectReplica::initialized, this, &QmlApp::updateActiveChat);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &QmlApp::updateActiveChat);
    connect(service.get(), &ToxServiceReplica::profilesChanged, this, &QmlApp::profilesChanged);
    connect(service.get(), &ToxServiceReplica::encryptedProfilesChanged, this,
            &QmlApp::profilesChanged);
    connect(service.get(), &ToxServiceReplica::loggedInChanged, this, &QmlApp::loggedInChanged);
    connect(service.get(), &ToxServiceReplica::profileNameChanged, this, &QmlApp::profileChanged);
    connect(service.get(), &ToxServiceReplica::encryptedProfilesChanged, this, &QmlApp::profileChanged);
    connect(service.get(), &ToxServiceReplica::onlineChanged, this, &QmlApp::onlineChanged);
    connect(service.get(), &ToxServiceReplica::selfNameChanged, this, &QmlApp::selfNameChanged);
    connect(service.get(), &ToxServiceReplica::selfIdChanged, this, &QmlApp::selfIdChanged);
    connect(service.get(), &ToxServiceReplica::selfStatusMessageChanged, this,
            &QmlApp::selfStatusMessageChanged);
    connect(service.get(), &ToxServiceReplica::selfStatusChanged, this, &QmlApp::selfStatusChanged);
    connect(service.get(), &ToxServiceReplica::selfAvatarHashChanged, this, &QmlApp::selfAvatarChanged);
    connect(service.get(), &ToxServiceReplica::selfIdChanged, this, &QmlApp::selfAvatarChanged);
    connect(service.get(), &ToxServiceReplica::languageChanged, this, &QmlApp::languageChanged);
    connect(service.get(), &ToxServiceReplica::typingNotificationChanged, this,
            &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::statusChangeMessagesChanged, this,
            &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::keepHistoryChanged, this, &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::notifyChanged, this, &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::conferenceAlwaysNotifyChanged, this,
            &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::autoSaveFilesChanged, this, &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::maxAutoAcceptSizeChanged, this,
            &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::notifySoundChanged, this, &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::busySoundChanged, this, &QmlApp::settingsChanged);
    connect(service.get(), &ToxServiceReplica::udpEnabledChanged, this, &QmlApp::networkChanged);
    connect(service.get(), &ToxServiceReplica::ipv6EnabledChanged, this, &QmlApp::networkChanged);
    connect(service.get(), &ToxServiceReplica::lanDiscoveryEnabledChanged, this,
            &QmlApp::networkChanged);
    connect(service.get(), &ToxServiceReplica::proxyTypeChanged, this, &QmlApp::networkChanged);
    connect(service.get(), &ToxServiceReplica::proxyAddressChanged, this, &QmlApp::networkChanged);
    connect(service.get(), &ToxServiceReplica::proxyPortChanged, this, &QmlApp::networkChanged);
    // The service drops the chats when it logs out.
    connect(service.get(), &ToxServiceReplica::loggedInChanged, this, [this](bool loggedIn) {
        if (!loggedIn) {
            // Also those of removed friends and left conferences.
            for (QmlChat* chat : findChildren<QmlChat*>(Qt::FindDirectChildrenOnly)) {
                chat->deleteLater();
            }
            chats.clear();
        }
    });
    connect(service.get(), &ToxServiceReplica::callKindChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::callChatChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::callStateChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::callStartedChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::callVideoChanged, this, [this](bool video) {
        if (!video) {
            videoFrame = {};
        }
        emit callVideoChanged();
    });
    // The service made them.
    connect(service.get(), &ToxServiceReplica::callVideoFrame, this, [this](const QByteArray& jpeg) {
        if (videoFrame.loadFromData(jpeg, "JPEG")) {
            ++videoFrames;
            emit callVideoChanged();
        }
    });
    connect(service.get(), &ToxServiceReplica::selfVideoFrame, this, [this](const QByteArray& jpeg) {
        if (selfFrame.loadFromData(jpeg, "JPEG")) {
            ++selfFrames;
            emit callVideoChanged();
        }
    });
    connect(service.get(), &ToxServiceReplica::callSendsVideoChanged, this, [this](bool sends) {
        if (!sends) {
            selfFrame = {};
        }
        emit callChanged();
        emit callVideoChanged();
    });
    connect(service.get(), &ToxServiceReplica::callOffersVideoChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::canSwitchCameraChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::callMicMutedChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::callSpeakerMutedChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::callLoudspeakerChanged, this, &QmlApp::callChanged);
    connect(service.get(), &ToxServiceReplica::friendRequestCountChanged, this,
            &QmlApp::friendRequestCountChanged);
    connect(service.get(), &ToxServiceReplica::inviteCountChanged, this, &QmlApp::inviteCountChanged);
    connect(service.get(), &ToxServiceReplica::translationChanged, this,
            [this](const QString& localeName) {
                Translator::translate(localeName);
                emit translationChanged();
            });
    connect(service.get(), &ToxServiceReplica::error, this, &QmlApp::error);
}

QmlApp::~QmlApp() = default;

bool QmlApp::isConnected() const
{
    return service->isReplicaValid();
}

QStringList QmlApp::getProfiles() const
{
    return service->profiles();
}

bool QmlApp::isLoggedIn() const
{
    return service->loggedIn();
}

QString QmlApp::getProfileName() const
{
    return service->profileName();
}

bool QmlApp::hasPassword() const
{
    return !service->profileName().isEmpty() && isEncrypted(service->profileName());
}

bool QmlApp::isOnline() const
{
    return service->online();
}

QString QmlApp::getSelfName() const
{
    return service->selfName();
}

void QmlApp::setSelfName(const QString& name)
{
    service->changeName(name);
}

QString QmlApp::getSelfId() const
{
    return service->selfId();
}

QString QmlApp::getSelfStatusMessage() const
{
    return service->selfStatusMessage();
}

void QmlApp::setSelfStatusMessage(const QString& message)
{
    service->changeStatusMessage(message);
}

int QmlApp::getSelfStatus() const
{
    return service->selfStatus();
}

void QmlApp::setSelfStatus(int status)
{
    service->changeStatus(status);
}

QString QmlApp::getSelfAvatar() const
{
    return getSelfId().isEmpty()
               ? QString{}
               : QStringLiteral("image://avatar/self/%1").arg(service->selfAvatarHash());
}

QString QmlApp::avatarSource(ChatKind::Kind kind, const QString& id, const QString& hash) const
{
    return AvatarProvider::source(kind, id, hash);
}

bool QmlApp::hasOwnAvatar() const
{
    return !service->selfAvatarHash().isEmpty();
}

QAbstractItemModel* QmlApp::getChats()
{
    return recentChats.get();
}

QAbstractItemModel* QmlApp::getFoundChats()
{
    return foundChats.get();
}

QString QmlApp::getChatFilter() const
{
    return chatFilter;
}

void QmlApp::setChatFilter(const QString& filter)
{
    if (filter == chatFilter) {
        return;
    }
    chatFilter = filter;
    // The replica knows its roles once it's there.
    foundChats->setFilterRole(chatList->roleNames().key("name"));
    foundChats->setFilterFixedString(filter);
    emit chatFilterChanged();
}

QAbstractItemModel* QmlApp::getFriendRequests()
{
    return friendRequests.get();
}

int QmlApp::getCallKind() const
{
    return service->callKind();
}

QString QmlApp::getCallChat() const
{
    return service->callChat();
}

int QmlApp::getCallState() const
{
    return service->callState();
}

QDateTime QmlApp::getCallStarted() const
{
    return service->callStarted();
}

bool QmlApp::isCallMicMuted() const
{
    return service->callMicMuted();
}

bool QmlApp::isCallSpeakerMuted() const
{
    return service->callSpeakerMuted();
}

bool QmlApp::isCallLoudspeaker() const
{
    return service->callLoudspeaker();
}

bool QmlApp::hasLoudspeakerChoice()
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

QString QmlApp::getCallVideo() const
{
    return service->callVideo() && !videoFrame.isNull()
               ? QStringLiteral("image://callvideo/%1").arg(videoFrames)
               : QString{};
}

QImage QmlApp::callVideoFrame() const
{
    return videoFrame;
}

QString QmlApp::getSelfVideo() const
{
    return service->callSendsVideo() && !selfFrame.isNull()
               ? QStringLiteral("image://callvideo/self/%1").arg(selfFrames)
               : QString{};
}

QImage QmlApp::selfVideoFrame() const
{
    return selfFrame;
}

bool QmlApp::callOffersVideo() const
{
    return service->callOffersVideo();
}

bool QmlApp::callSendsVideo() const
{
    return service->callSendsVideo();
}

bool QmlApp::canSwitchCamera() const
{
    return service->canSwitchCamera();
}

int QmlApp::getFriendRequestCount() const
{
    return service->friendRequestCount();
}

QAbstractItemModel* QmlApp::getInvites()
{
    return invites.get();
}

int QmlApp::getInviteCount() const
{
    return service->inviteCount();
}

QVariantList QmlApp::getLanguages() const
{
    QVariantList languages{QVariantMap{{"code", QString{}}, {"name", tr("System language")}}};
    for (const QString& locale : Translator::getLocales()) {
        languages.append(QVariantMap{{"code", locale}, {"name", Translator::getLanguageName(locale)}});
    }
    return languages;
}

QString QmlApp::getLanguage() const
{
    return service->language();
}

void QmlApp::setLanguage(const QString& language)
{
    service->setLanguage(language);
}

bool QmlApp::getStatusChangeMessages() const
{
    return service->statusChangeMessages();
}

void QmlApp::setStatusChangeMessages(bool value)
{
    service->setStatusChangeMessages(value);
}

bool QmlApp::getTypingNotification() const
{
    return service->typingNotification();
}

void QmlApp::setTypingNotification(bool value)
{
    service->setTypingNotification(value);
}

bool QmlApp::getKeepHistory() const
{
    return service->keepHistory();
}

void QmlApp::setKeepHistory(bool value)
{
    service->setKeepHistory(value);
}

bool QmlApp::getNotify() const
{
    return service->notify();
}

void QmlApp::setNotify(bool value)
{
    service->setNotify(value);
}

bool QmlApp::getConferenceAlwaysNotify() const
{
    return service->conferenceAlwaysNotify();
}

void QmlApp::setConferenceAlwaysNotify(bool value)
{
    service->setConferenceAlwaysNotify(value);
}

bool QmlApp::getAutoSaveFiles() const
{
    return service->autoSaveFiles();
}

void QmlApp::setAutoSaveFiles(bool value)
{
    service->setAutoSaveFiles(value);
}

qint64 QmlApp::getMaxAutoAcceptSize() const
{
    return service->maxAutoAcceptSize();
}

void QmlApp::setMaxAutoAcceptSize(qint64 value)
{
    service->setMaxAutoAcceptSize(value);
}

bool QmlApp::getNotifySound() const
{
    return service->notifySound();
}

void QmlApp::setNotifySound(bool value)
{
    service->setNotifySound(value);
}

bool QmlApp::getBusySound() const
{
    return service->busySound();
}

void QmlApp::setBusySound(bool value)
{
    service->setBusySound(value);
}

QVariantMap QmlApp::getVersions() const
{
    // As the Widgets UI's About shows them.
    return {
        {"version", VersionInfo::gitDescribe()},
        {"commit", VersionInfo::gitVersion()},
        {"toxcore",
         QStringLiteral("%1.%2.%3").arg(tox_version_major()).arg(tox_version_minor()).arg(tox_version_patch())},
        {"qt", QStringLiteral(QT_VERSION_STR)},
        // What the user's device is, for reports about how qTox looks on it.
        {"qtRuntime", QString::fromLatin1(qVersion())},
        {"system", systemName()},
        {"device", deviceModel()},
    };
}

bool QmlApp::isUdpEnabled() const
{
    return service->udpEnabled();
}

void QmlApp::setUdpEnabled(bool enabled)
{
    service->setUdpEnabled(enabled);
}

bool QmlApp::isIpv6Enabled() const
{
    return service->ipv6Enabled();
}

void QmlApp::setIpv6Enabled(bool enabled)
{
    service->setIpv6Enabled(enabled);
}

bool QmlApp::isLanDiscoveryEnabled() const
{
    return service->lanDiscoveryEnabled();
}

void QmlApp::setLanDiscoveryEnabled(bool enabled)
{
    service->setLanDiscoveryEnabled(enabled);
}

int QmlApp::getProxyType() const
{
    return service->proxyType();
}

void QmlApp::setProxyType(int type)
{
    service->setProxyType(type);
}

QString QmlApp::getProxyAddress() const
{
    return service->proxyAddress();
}

void QmlApp::setProxyAddress(const QString& address)
{
    service->setProxyAddress(address);
}

int QmlApp::getProxyPort() const
{
    return service->proxyPort();
}

void QmlApp::setProxyPort(int port)
{
    service->setProxyPort(port);
}

bool QmlApp::isEncrypted(const QString& profileName) const
{
    return service->encryptedProfiles().contains(profileName);
}

void QmlApp::createProfile(const QString& name, const QString& password, bool autoLogin)
{
    service->createProfile(name, password, autoLogin);
}

void QmlApp::loadProfile(const QString& name, const QString& password, bool autoLogin)
{
    service->loadProfile(name, password, autoLogin);
}

void QmlApp::logout()
{
    service->logout();
}

void QmlApp::setPassword(const QString& password)
{
    service->setPassword(password);
}

void QmlApp::renameProfile(const QString& name)
{
    service->renameProfile(name.trimmed());
}

void QmlApp::deleteProfile()
{
    service->deleteProfile();
}

void QmlApp::exportProfile(const QUrl& file)
{
    auto* watcher = new QRemoteObjectPendingCallWatcher(service->exportProfile(), this);
    connect(watcher, &QRemoteObjectPendingCallWatcher::finished, this,
            [this, path = Files::path(file)](QRemoteObjectPendingCallWatcher* call) {
                call->deleteLater();
                // The service said why it has none.
                const QByteArray save = call->returnValue().toByteArray();
                if (save.isEmpty()) {
                    return;
                }
                Files::write(path, save, this, [this](bool written) {
                    if (written) {
                        emit notice(tr("Profile exported"));
                    } else {
                        emit error(tr("Failed to save file"),
                                   tr("The file you chose could not be saved."));
                    }
                });
            });
}

QString QmlApp::importedProfileName(const QUrl& file)
{
    // As the Widgets UI's ProfileImporter.
    const QFileInfo info{Files::name(file)};
    if (info.suffix() != QStringLiteral("tox") || info.completeBaseName().isEmpty()) {
        emit error(tr("Ignoring non-Tox file"),
                   tr("Warning: You have chosen a file that is not a Tox save file; ignoring."));
        return {};
    }
    return info.completeBaseName();
}

void QmlApp::importProfile(const QUrl& file)
{
    const QString name = importedProfileName(file);
    if (name.isEmpty()) {
        return;
    }
    Files::read(Files::path(file), this, [this, name](std::optional<QByteArray> save) {
        if (save) {
            service->importProfile(name, *save);
        } else {
            emit error(tr("Couldn't import the profile"), tr("Unable to open this file."));
        }
    });
}

void QmlApp::createConference(const QString& title, const QString& publicKey)
{
    auto* watcher = new QRemoteObjectPendingCallWatcher(service->createConference(title), this);
    connect(watcher, &QRemoteObjectPendingCallWatcher::finished, this,
            [this, publicKey](QRemoteObjectPendingCallWatcher* call) {
                call->deleteLater();
                const QString id = call->returnValue().toString();
                if (id.isEmpty()) {
                    emit error(tr("Couldn't create a conference"), tr("Tox couldn't make one."));
                    return;
                }
                if (!publicKey.isEmpty()) {
                    service->inviteToConference(id, publicKey);
                }
                emit chatAdded(ChatKind::Kind::Conference, id);
            });
}

void QmlApp::createGroup(const QString& name, const QString& publicKey)
{
    auto* watcher = new QRemoteObjectPendingCallWatcher(service->createGroup(name), this);
    connect(watcher, &QRemoteObjectPendingCallWatcher::finished, this,
            [this, publicKey](QRemoteObjectPendingCallWatcher* call) {
                call->deleteLater();
                const QString id = call->returnValue().toString();
                if (id.isEmpty()) {
                    emit error(tr("Couldn't create a group"), tr("Tox couldn't make one."));
                    return;
                }
                if (!publicKey.isEmpty()) {
                    service->inviteToGroup(id, publicKey);
                }
                emit chatAdded(ChatKind::Kind::Group, id);
            });
}

QString QmlApp::joinGroup(const QString& id, const QString& password)
{
    const QString trimmed = id.trimmed();
    if (QByteArray::fromHex(trimmed.toLatin1()).size() != GroupId::size
        || trimmed.size() != GroupId::size * 2) {
        return tr("That's not a group's ID.");
    }

    auto* watcher = new QRemoteObjectPendingCallWatcher(service->joinGroup(trimmed, password), this);
    connect(watcher, &QRemoteObjectPendingCallWatcher::finished, this,
            [this](QRemoteObjectPendingCallWatcher* call) {
                call->deleteLater();
                const QString joined = call->returnValue().toString();
                if (joined.isEmpty()) {
                    emit error(tr("Couldn't join the group"), tr("Tox couldn't join it."));
                    return;
                }
                emit chatAdded(ChatKind::Kind::Group, joined);
            });
    return {};
}

void QmlApp::newNospam()
{
    service->newNospam();
}

void QmlApp::eraseHistory()
{
    service->eraseHistory();
}

void QmlApp::startCall(const QString& publicKey, bool video)
{
    askForCall(video, [this, publicKey](bool withVideo) { service->startCall(publicKey, withVideo); });
}

void QmlApp::answerCall(bool video)
{
    askForCall(video, [this](bool withVideo) { service->answerCall(withVideo); });
}

void QmlApp::switchCamera()
{
    service->switchCamera();
}

void QmlApp::joinConferenceCall(const QString& id)
{
    askForCall(false, [this, id](bool) { service->joinConferenceCall(id); });
}

void QmlApp::endCall()
{
    service->endCall();
}

void QmlApp::toggleCallMic()
{
    service->toggleCallMic();
}

void QmlApp::toggleCallSpeaker()
{
    service->toggleCallSpeaker();
}

void QmlApp::toggleCallLoudspeaker()
{
    service->toggleCallLoudspeaker();
}

void QmlApp::askForCall(bool video, std::function<void(bool video)> then)
{
    askFor(QMicrophonePermission{}, [this, video, then](bool microphone) {
        if (!microphone) {
            emit error(tr("No microphone"),
                       tr("qTox may not use the microphone, so your friend can't hear you."));
        }
        if (!video) {
            then(false);
            return;
        }
        askFor(QCameraPermission{}, [this, then](bool camera) {
            if (!camera) {
                emit error(tr("No camera"),
                           tr("qTox may not use the camera, so your friend can't see you."));
            }
            then(camera);
        });
    });
}

void QmlApp::askFor(const QPermission& permission, std::function<void(bool granted)> then)
{
    if (const Qt::PermissionStatus status = qApp->checkPermission(permission);
        status != Qt::PermissionStatus::Undetermined) {
        then(status == Qt::PermissionStatus::Granted);
        return;
    }
    qApp->requestPermission(permission, this, [then](const QPermission& asked) {
        then(asked.status() == Qt::PermissionStatus::Granted);
    });
}

QmlChat* QmlApp::chat(ChatKind::Kind kind, const QString& id)
{
    const QString name = RemoteModels::chat(kind, id);
    if (QmlChat* existing = chats.value(name)) {
        return existing;
    }

    service->openChat(static_cast<int>(kind), id);
    auto* chat = new QmlChat(kind, id, *service, node, *chatList, this);
    connect(chat, &QmlChat::error, this, &QmlApp::error);
    QQmlEngine::setObjectOwnership(chat, QQmlEngine::CppOwnership);
    chats.insert(name, chat);
    return chat;
}

QString QmlApp::addFriend(const QString& toxId, const QString& message)
{
    const QString id = toxId.trimmed();
    if (!ToxId::isValidToxId(id)) {
        return tr("That's not a valid Tox ID.");
    }
    if (ToxId(id) == ToxId(getSelfId())) {
        return tr("That's your own Tox ID.");
    }

    service->addFriend(id, message.isEmpty() ? tr("%1 here! Tox me maybe?").arg(getSelfName())
                                             : message);
    return {};
}

void QmlApp::acceptFriendRequest(const QString& publicKey)
{
    service->acceptFriendRequest(publicKey);
}

void QmlApp::rejectFriendRequest(const QString& publicKey)
{
    service->rejectFriendRequest(publicKey);
}

void QmlApp::acceptInvite(const QString& id, const QString& password)
{
    service->acceptInvite(id, password);
}

void QmlApp::rejectInvite(const QString& id)
{
    service->rejectInvite(id);
}

void QmlApp::removeFriend(const QString& publicKey, bool removeHistory)
{
    service->removeFriend(publicKey, removeHistory);
    // Its pages still show it while they close: it goes at logout.
    chats.remove(RemoteModels::chat(ChatKind::Kind::Friend, publicKey));
}

void QmlApp::leaveConference(const QString& id)
{
    service->leaveConference(id);
    // Its pages still show it while they close: it goes at logout.
    chats.remove(RemoteModels::chat(ChatKind::Kind::Conference, id));
}

void QmlApp::leaveGroup(const QString& id)
{
    service->leaveGroup(id);
    // Its pages still show it while they close: it goes at logout.
    chats.remove(RemoteModels::chat(ChatKind::Kind::Group, id));
}

void QmlApp::showingChat(ChatKind::Kind kind, const QString& id)
{
    shownChat = {kind, id};
    updateActiveChat();
}

void QmlApp::hidingChat(ChatKind::Kind kind, const QString& id)
{
    if (shownChat == std::pair{kind, id}) {
        shownChat.reset();
        updateActiveChat();
    }
}

void QmlApp::updateActiveChat()
{
    // It's sent again once the service is there.
    if (!service->isReplicaValid()) {
        return;
    }
    if (shownChat && QGuiApplication::applicationState() == Qt::ApplicationActive) {
        service->setActiveChat(static_cast<int>(shownChat->first), shownChat->second);
    } else {
        service->setActiveChat(-1, {});
    }
}

void QmlApp::open(const QString& modelName)
{
    openRequest = modelName;
    emit openRequested();
}

void QmlApp::receiveShare(const QUrl& file, const QString& text)
{
    sharedFile = file;
    sharedText = text;
    open(shareRequest);
}

QVariantMap QmlApp::takeOpenRequest()
{
    const QString modelName = std::exchange(openRequest, {});
    if (modelName == shareRequest) {
        return {{"page", "share"},
                {"file", std::exchange(sharedFile, {})},
                {"text", std::exchange(sharedText, {})}};
    }
    if (modelName == answerRequest && getCallState() == static_cast<int>(CallState::State::Incoming)) {
        // As the friend calls.
        answerCall(callOffersVideo());
        return {{"page", "chat"}, {"kind", getCallKind()}, {"chatId", getCallChat()}};
    }
    if (const auto chat = RemoteModels::parseChat(modelName)) {
        return {{"page", "chat"}, {"kind", static_cast<int>(chat->first)}, {"chatId", chat->second}};
    }
    if (modelName == RemoteModels::friendRequests || modelName == RemoteModels::invites) {
        return {{"page", "requests"}};
    }
    return {};
}

void QmlApp::copyToClipboard(const QString& text)
{
    QGuiApplication::clipboard()->setText(text);
}

bool QmlApp::canShare() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

void QmlApp::share(const QString& text)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxActivity", "share",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       QNativeInterface::QAndroidApplication::context().object(),
                                       QJniObject::fromString(text).object<jstring>());
#else
    std::ignore = text;
#endif
}

void QmlApp::saveFile(const QString& path, const QUrl& target)
{
    Files::copy(path, Files::path(target), this, [this](bool copied) {
        if (copied) {
            emit notice(tr("Saved"));
        } else {
            emit error(tr("Failed to save file"), tr("The file you chose could not be saved."));
        }
    });
}

void QmlApp::shareFile(const QString& path)
{
#ifdef Q_OS_ANDROID
    const bool shared =
        QJniObject::callStaticMethod<jboolean>("io/github/qtox/qtox/QtoxActivity", "shareFile",
                                               "(Landroid/content/Context;Ljava/lang/String;)Z",
                                               QNativeInterface::QAndroidApplication::context().object(),
                                               QJniObject::fromString(path).object<jstring>());
    if (!shared) {
        emit error(tr("Couldn't share the file"),
                   tr("Can't share %1.").arg(QFileInfo{path}.fileName()));
    }
#else
    std::ignore = path;
#endif
}

void QmlApp::openFile(const QString& path)
{
#ifdef Q_OS_ANDROID
    const bool opened =
        QJniObject::callStaticMethod<jboolean>("io/github/qtox/qtox/QtoxActivity", "openFile",
                                               "(Landroid/content/Context;Ljava/lang/String;)Z",
                                               QNativeInterface::QAndroidApplication::context().object(),
                                               QJniObject::fromString(path).object<jstring>());
#else
    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
    if (!opened) {
        emit error(tr("Couldn't open the file"),
                   tr("No app can open %1.").arg(QFileInfo{path}.fileName()));
    }
}

void QmlApp::setAvatar(const QUrl& file)
{
    // Android's picker gives content: URLs, which QFile opens too.
    QFile picture{Files::path(file)};
    if (!picture.open(QIODevice::ReadOnly)) {
        emit error(tr("Couldn't set the picture"), tr("Unable to open this file."));
        return;
    }

    QByteArray avatar;
    switch (ProfileInfo::createAvatar(picture.readAll(), avatar)) {
    case IProfileInfo::SetAvatarResult::OK:
        service->changeAvatar(avatar);
        return;
    case IProfileInfo::SetAvatarResult::TooLarge:
        emit error(tr("Couldn't set the picture"), tr("This picture is too big."));
        return;
    default:
        emit error(tr("Couldn't set the picture"), tr("Unable to read this image."));
        return;
    }
}

void QmlApp::removeAvatar()
{
    service->removeAvatar();
}

QImage QmlApp::avatar(const QString& providerId) const
{
    const QStringList parts = providerId.split(u'/');
    if (parts.size() == 2 && parts[0] == u"self") {
        return avatarOrDefault(service->selfAvatar(), ChatKind::Kind::Friend,
                               getSelfId().left(ToxPk::numHexChars));
    }
    const std::optional<ChatKind::Kind> kind =
        parts.size() == 3 ? ChatKind::fromInt(parts[0].toInt()) : std::nullopt;
    if (!kind) {
        return {};
    }

    const QHash<int, QByteArray> roles = chatList->roleNames();
    for (int row = 0; row < chatList->rowCount(); ++row) {
        const QModelIndex index = chatList->index(row, 0);
        if (chatList->data(index, roles.key("kind")).toInt() == static_cast<int>(*kind)
            && chatList->data(index, roles.key("chatId")).toString() == parts[1]) {
            return avatarOrDefault(chatList->data(index, roles.key("avatar")).toByteArray(), *kind,
                                   parts[1]);
        }
    }
    return {};
}

QImage QmlApp::avatarOrDefault(const QByteArray& avatar, ChatKind::Kind kind, const QString& id) const
{
    const QImage picture = UntrustedImage::decode(avatar);
    if (!picture.isNull()) {
        return picture;
    }
    // As the Widgets UI's Profile::loadAvatar() does.
    if (kind == ChatKind::Kind::Conference) {
        return QImage{QStringLiteral(":/img/conference_dark.svg")};
    }
    if (kind == ChatKind::Kind::Group) {
        return QImage{QStringLiteral(":/img/group_dark.svg")};
    }
    if (service->showIdenticons()) {
        // Large enough to stay sharp when shrunk.
        return Identicon{ToxPk{id}.getByteArray()}.toImage(64);
    }
    return QImage{QStringLiteral(":/img/contact_dark.svg")};
}

void QmlApp::moveToBack()
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject{QNativeInterface::QAndroidApplication::context()}
            .callMethod<jboolean>("moveTaskToBack", jboolean{true});
    });
#endif
}

QmlApp* QmlAppForeign::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    std::ignore = qmlEngine;
    std::ignore = jsEngine;
    // The app owns it, not QML.
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}
