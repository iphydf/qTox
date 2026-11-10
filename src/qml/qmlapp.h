/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/qml/chatkind.h"

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

class QAbstractItemModel;
class QImage;
class QAbstractItemModelReplica;
class QJSEngine;
class QPermission;
class QQmlEngine;
class QRemoteObjectNode;
class QSortFilterProxyModel;
class QmlChat;
class ToxServiceReplica;

Q_MOC_INCLUDE("src/qml/qmlchat.h")

/**
 * @brief What the QML user interface sees of qTox: the Tox service's profiles, friends and chats.
 */
class QmlApp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QStringList profiles READ getProfiles NOTIFY profilesChanged)
    Q_PROPERTY(bool loggedIn READ isLoggedIn NOTIFY loggedInChanged)
    /** The profile logged in, or empty. */
    Q_PROPERTY(QString profileName READ getProfileName NOTIFY profileChanged)
    Q_PROPERTY(bool hasPassword READ hasPassword NOTIFY profileChanged)
    Q_PROPERTY(bool online READ isOnline NOTIFY onlineChanged)
    Q_PROPERTY(QString selfName READ getSelfName WRITE setSelfName NOTIFY selfNameChanged)
    Q_PROPERTY(QString selfId READ getSelfId NOTIFY selfIdChanged)
    Q_PROPERTY(QString selfStatusMessage READ getSelfStatusMessage WRITE setSelfStatusMessage NOTIFY
                   selfStatusMessageChanged)
    /** A UserStatus::Status: Online, Away or Busy. */
    Q_PROPERTY(int selfStatus READ getSelfStatus WRITE setSelfStatus NOTIFY selfStatusChanged)
    /** The user's avatar as an image source, or empty when nobody's logged in. */
    Q_PROPERTY(QString selfAvatar READ getSelfAvatar NOTIFY selfAvatarChanged)
    /** Whether the user chose a picture: else selfAvatar is the default one. */
    Q_PROPERTY(bool hasOwnAvatar READ hasOwnAvatar NOTIFY selfAvatarChanged)
    /** Friends and conferences, most recent first. */
    Q_PROPERTY(QAbstractItemModel* chats READ getChats CONSTANT)
    /** Those of chats whose name has chatFilter in it. */
    Q_PROPERTY(QAbstractItemModel* foundChats READ getFoundChats CONSTANT)
    Q_PROPERTY(QString chatFilter READ getChatFilter WRITE setChatFilter NOTIFY chatFilterChanged)
    Q_PROPERTY(QAbstractItemModel* friendRequests READ getFriendRequests CONSTANT)
    /** The call's chat: a ChatKind::Kind and an ID, empty for no call. */
    Q_PROPERTY(int callKind READ getCallKind NOTIFY callChanged)
    Q_PROPERTY(QString callChat READ getCallChat NOTIFY callChanged)
    /** A CallState::State. */
    Q_PROPERTY(int callState READ getCallState NOTIFY callChanged)
    /** When the call became active. */
    Q_PROPERTY(QDateTime callStarted READ getCallStarted NOTIFY callChanged)
    Q_PROPERTY(bool callMicMuted READ isCallMicMuted NOTIFY callChanged)
    Q_PROPERTY(bool callSpeakerMuted READ isCallSpeakerMuted NOTIFY callChanged)
    /** Whether the call plays on the loudspeaker rather than the earpiece or a headset. */
    Q_PROPERTY(bool callLoudspeaker READ isCallLoudspeaker NOTIFY callChanged)
    /** Whether the user can choose that, as on Android: elsewhere, the system's settings do. */
    Q_PROPERTY(bool hasLoudspeakerChoice READ hasLoudspeakerChoice CONSTANT)
    /** The friend's video in the call as an image source: empty for none. */
    Q_PROPERTY(QString callVideo READ getCallVideo NOTIFY callVideoChanged)
    /** What the user's camera sends the friend, likewise. */
    Q_PROPERTY(QString selfVideo READ getSelfVideo NOTIFY callVideoChanged)
    /** Whether the friend calling offers video. */
    Q_PROPERTY(bool callOffersVideo READ callOffersVideo NOTIFY callChanged)
    Q_PROPERTY(bool callSendsVideo READ callSendsVideo NOTIFY callChanged)
    Q_PROPERTY(bool canSwitchCamera READ canSwitchCamera NOTIFY callChanged)
    Q_PROPERTY(int friendRequestCount READ getFriendRequestCount NOTIFY friendRequestCountChanged)
    Q_PROPERTY(QAbstractItemModel* invites READ getInvites CONSTANT)
    Q_PROPERTY(int inviteCount READ getInviteCount NOTIFY inviteCountChanged)
    /** The choices for language: {code, name}, the system's first with an empty code. */
    Q_PROPERTY(QVariantList languages READ getLanguages NOTIFY translationChanged)
    Q_PROPERTY(QString language READ getLanguage WRITE setLanguage NOTIFY languageChanged)
    /** The profile's privacy, notification and file settings. */
    Q_PROPERTY(bool typingNotification READ getTypingNotification WRITE setTypingNotification NOTIFY settingsChanged)
    Q_PROPERTY(bool statusChangeMessages READ getStatusChangeMessages WRITE setStatusChangeMessages
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool keepHistory READ getKeepHistory WRITE setKeepHistory NOTIFY settingsChanged)
    Q_PROPERTY(bool notify READ getNotify WRITE setNotify NOTIFY settingsChanged)
    Q_PROPERTY(bool conferenceAlwaysNotify READ getConferenceAlwaysNotify WRITE
                   setConferenceAlwaysNotify NOTIFY settingsChanged)
    Q_PROPERTY(bool autoSaveFiles READ getAutoSaveFiles WRITE setAutoSaveFiles NOTIFY settingsChanged)
    Q_PROPERTY(qint64 maxAutoAcceptSize READ getMaxAutoAcceptSize WRITE setMaxAutoAcceptSize NOTIFY settingsChanged)
    Q_PROPERTY(bool notifySound READ getNotifySound WRITE setNotifySound NOTIFY settingsChanged)
    Q_PROPERTY(bool busySound READ getBusySound WRITE setBusySound NOTIFY settingsChanged)
    /** What qTox is: {version, commit, toxcore, qt}. */
    Q_PROPERTY(QVariantMap versions READ getVersions CONSTANT)
    Q_PROPERTY(bool udpEnabled READ isUdpEnabled WRITE setUdpEnabled NOTIFY networkChanged)
    Q_PROPERTY(bool ipv6Enabled READ isIpv6Enabled WRITE setIpv6Enabled NOTIFY networkChanged)
    Q_PROPERTY(bool lanDiscoveryEnabled READ isLanDiscoveryEnabled WRITE setLanDiscoveryEnabled
                   NOTIFY networkChanged)
    /** An ICoreSettings::ProxyType. */
    Q_PROPERTY(int proxyType READ getProxyType WRITE setProxyType NOTIFY networkChanged)
    Q_PROPERTY(QString proxyAddress READ getProxyAddress WRITE setProxyAddress NOTIFY networkChanged)
    Q_PROPERTY(int proxyPort READ getProxyPort WRITE setProxyPort NOTIFY networkChanged)

public:
    explicit QmlApp(QRemoteObjectNode& node, QObject* parent = nullptr);
    ~QmlApp() override;

    /** Whether the service is there: the other properties are only meaningful when it is. */
    bool isConnected() const;
    QStringList getProfiles() const;
    bool isLoggedIn() const;
    QString getProfileName() const;
    bool hasPassword() const;
    bool isOnline() const;
    QString getSelfName() const;
    void setSelfName(const QString& name);
    QString getSelfId() const;
    QString getSelfStatusMessage() const;
    void setSelfStatusMessage(const QString& message);
    int getSelfStatus() const;
    void setSelfStatus(int status);
    QString getSelfAvatar() const;
    bool hasOwnAvatar() const;
    QAbstractItemModel* getChats();
    QAbstractItemModel* getFoundChats();
    QString getChatFilter() const;
    void setChatFilter(const QString& filter);
    QAbstractItemModel* getFriendRequests();
    int getCallKind() const;
    QString getCallChat() const;
    int getCallState() const;
    QDateTime getCallStarted() const;
    bool isCallMicMuted() const;
    bool isCallSpeakerMuted() const;
    bool isCallLoudspeaker() const;
    static bool hasLoudspeakerChoice();
    QString getCallVideo() const;
    /** What callVideo shows. */
    QImage callVideoFrame() const;
    QString getSelfVideo() const;
    QImage selfVideoFrame() const;
    bool callOffersVideo() const;
    bool callSendsVideo() const;
    bool canSwitchCamera() const;
    int getFriendRequestCount() const;
    QAbstractItemModel* getInvites();
    int getInviteCount() const;
    QVariantList getLanguages() const;
    QString getLanguage() const;
    void setLanguage(const QString& language);
    bool getTypingNotification() const;
    void setTypingNotification(bool value);
    bool getStatusChangeMessages() const;
    void setStatusChangeMessages(bool value);
    bool getKeepHistory() const;
    void setKeepHistory(bool value);
    bool getNotify() const;
    void setNotify(bool value);
    bool getConferenceAlwaysNotify() const;
    void setConferenceAlwaysNotify(bool value);
    bool getAutoSaveFiles() const;
    void setAutoSaveFiles(bool value);
    qint64 getMaxAutoAcceptSize() const;
    void setMaxAutoAcceptSize(qint64 value);
    bool getNotifySound() const;
    void setNotifySound(bool value);
    bool getBusySound() const;
    void setBusySound(bool value);
    QVariantMap getVersions() const;
    bool isUdpEnabled() const;
    void setUdpEnabled(bool enabled);
    bool isIpv6Enabled() const;
    void setIpv6Enabled(bool enabled);
    bool isLanDiscoveryEnabled() const;
    void setLanDiscoveryEnabled(bool enabled);
    int getProxyType() const;
    void setProxyType(int type);
    QString getProxyAddress() const;
    void setProxyAddress(const QString& address);
    int getProxyPort() const;
    void setProxyPort(int port);

    Q_INVOKABLE bool isEncrypted(const QString& profileName) const;
    /**
     * @brief loggedIn becomes true, or error() says why it didn't.
     *
     * With @p autoLogin, the service logs in by itself when it starts, unless the profile has a
     * password.
     */
    Q_INVOKABLE void createProfile(const QString& name, const QString& password, bool autoLogin);
    /** Like createProfile(). */
    Q_INVOKABLE void loadProfile(const QString& name, const QString& password, bool autoLogin);
    Q_INVOKABLE void logout();
    /** For the profile logged in: an empty password removes it. */
    Q_INVOKABLE void setPassword(const QString& password);
    Q_INVOKABLE void renameProfile(const QString& name);
    /** Logs out and deletes the profile logged in. */
    Q_INVOKABLE void deleteProfile();
    /** Saves the profile logged in to @p file, and says so in notice(), or in error() why not. */
    Q_INVOKABLE void exportProfile(const QUrl& file);
    /** The profile that importing @p file makes, or empty after error() says why it can't. */
    Q_INVOKABLE QString importedProfileName(const QUrl& file);
    /** Adds the Tox save file @p file as a profile, replacing the one of the same name. */
    Q_INVOKABLE void importProfile(const QUrl& file);
    /** A new random NoSpam: friend requests to the old Tox ID no longer arrive. */
    Q_INVOKABLE void newNospam();
    /** Deletes the chat history of the profile logged in. */
    Q_INVOKABLE void eraseHistory();
    /** The chat with a friend (by public key) or in a conference or group (by ID), owned by this object. */
    Q_INVOKABLE QmlChat* chat(ChatKind::Kind kind, const QString& id);
    /** @return Why the request can't be sent, or empty if it was. */
    Q_INVOKABLE QString addFriend(const QString& toxId, const QString& message);
    Q_INVOKABLE void acceptFriendRequest(const QString& publicKey);
    Q_INVOKABLE void rejectFriendRequest(const QString& publicKey);
    Q_INVOKABLE void removeFriend(const QString& publicKey, bool removeHistory);
    /** With a group's password, if it has one. */
    Q_INVOKABLE void acceptInvite(const QString& id, const QString& password = {});
    Q_INVOKABLE void rejectInvite(const QString& id);
    /**
     * @brief Makes a conference, and invites @p publicKey's friend to it unless empty.
     *
     * chatAdded() says its ID, or error() that it couldn't.
     */
    Q_INVOKABLE void createConference(const QString& title, const QString& publicKey);
    Q_INVOKABLE void leaveConference(const QString& id);
    /** Like createConference(), a public group. */
    Q_INVOKABLE void createGroup(const QString& name, const QString& publicKey);
    /**
     * @brief Joins a public group by its ID, with its password if it has one.
     *
     * @return Why it can't, or empty if chatAdded() will say it did.
     */
    Q_INVOKABLE QString joinGroup(const QString& id, const QString& password);
    Q_INVOKABLE void leaveGroup(const QString& id);
    /**
     * @brief Calls a friend, with the microphone if the user lets qTox use it, and the camera if
     *        @p video and the user lets qTox use that too.
     */
    Q_INVOKABLE void startCall(const QString& publicKey, bool video = false);
    /** Like startCall(). */
    Q_INVOKABLE void answerCall(bool video = false);
    Q_INVOKABLE void switchCamera();
    /** Like startCall(), in an audio conference. */
    Q_INVOKABLE void joinConferenceCall(const QString& id);
    Q_INVOKABLE void endCall();
    Q_INVOKABLE void toggleCallMic();
    Q_INVOKABLE void toggleCallSpeaker();
    Q_INVOKABLE void toggleCallLoudspeaker();
    /** The chat page shows this chat, which then needs no notifications while qTox is in front. */
    Q_INVOKABLE void showingChat(ChatKind::Kind kind, const QString& id);
    /** The chat page no longer shows this chat. */
    Q_INVOKABLE void hidingChat(ChatKind::Kind kind, const QString& id);
    /** Opens a remotemodels.h model's page, such as a chat, once someone is logged in. */
    void open(const QString& modelName);
    /** Asks which chat gets what another app shared: a file or text. */
    void receiveShare(const QUrl& file, const QString& text);
    /**
     * @brief What open() asked for, once.
     *
     * @return {page: "chat", kind, chatId}, {page: "requests"}, {page: "share", file, text}, or
     * empty.
     */
    Q_INVOKABLE QVariantMap takeOpenRequest();
    Q_INVOKABLE void copyToClipboard(const QString& text);
    /** Whether share() can share text with other apps. */
    Q_INVOKABLE bool canShare() const;
    /** Lets the user choose an app to send @p text to. */
    Q_INVOKABLE void share(const QString& text);
    /** Opens a received or sent file in the app for it, or says in error() why it can't. */
    Q_INVOKABLE void openFile(const QString& path);
    /** Copies a received or sent file to where the user chose, and says in notice() it did. */
    Q_INVOKABLE void saveFile(const QString& path, const QUrl& target);
    /** Like share(), a received or sent file. */
    Q_INVOKABLE void shareFile(const QString& path);
    /** Makes the picture in @p file the user's avatar, or says in error() why it can't. */
    Q_INVOKABLE void setAvatar(const QUrl& file);
    Q_INVOKABLE void removeAvatar();
    /**
     * @brief The avatar that an AvatarProvider ID names: self/HASH, or KIND/ID/HASH for a chat.
     *
     * The hash makes a new avatar a new image source. Without an avatar, the hash is empty and
     * the picture is the one the Widgets UI shows.
     */
    QImage avatar(const QString& providerId) const;
    /** AvatarProvider::source(), for the chat list's rows. */
    Q_INVOKABLE QString avatarSource(ChatKind::Kind kind, const QString& id, const QString& hash) const;
    /**
     * @brief On Android, leaves the user interface running in the background.
     *
     * Android's back button does that for other apps. Qt would quit instead, and its System.exit()
     * aborts in Android's renderer.
     */
    Q_INVOKABLE void moveToBack();

signals:
    void connectedChanged();
    void profilesChanged();
    void loggedInChanged();
    void profileChanged();
    void onlineChanged();
    void selfNameChanged();
    void selfIdChanged();
    void selfStatusMessageChanged();
    void selfStatusChanged();
    void selfAvatarChanged();
    void callChanged();
    void callVideoChanged();
    void chatFilterChanged();
    void friendRequestCountChanged();
    void inviteCountChanged();
    void languageChanged();
    void networkChanged();
    void settingsChanged();
    /** The service changed the language: retranslate. */
    void translationChanged();
    void error(const QString& title, const QString& text);
    /** Something done that the user doesn't see otherwise. */
    void notice(const QString& text);
    /** A conference or group that the user made or joined: its chat can open. */
    void chatAdded(ChatKind::Kind kind, const QString& id);
    void openRequested();

private:
    void updateActiveChat();
    /**
     * @brief Runs @p then once the user said whether qTox may use the microphone, and the camera
     *        if @p video: with whether it may.
     */
    void askForCall(bool video, std::function<void(bool video)> then);
    void askFor(const QPermission& permission, std::function<void(bool granted)> then);
    QImage avatarOrDefault(const QByteArray& avatar, ChatKind::Kind kind, const QString& id) const;

    QRemoteObjectNode& node;
    std::unique_ptr<ToxServiceReplica> service;
    std::unique_ptr<QAbstractItemModelReplica> chatList;
    std::unique_ptr<QSortFilterProxyModel> recentChats;
    std::unique_ptr<QSortFilterProxyModel> foundChats;
    QString chatFilter;
    // The friend's video, and how many frames came: a new number is a new image source.
    QImage videoFrame;
    quint64 videoFrames{0};
    QImage selfFrame;
    quint64 selfFrames{0};
    std::unique_ptr<QAbstractItemModelReplica> friendRequests;
    std::unique_ptr<QAbstractItemModelReplica> invites;
    // By remotemodels.h's chat name.
    QHash<QString, QmlChat*> chats;
    std::optional<std::pair<ChatKind::Kind, QString>> shownChat;
    QString openRequest;
    // What another app shared, for takeOpenRequest().
    QUrl sharedFile;
    QString sharedText;
};

/**
 * @brief Makes the one QmlApp the QML singleton QTox.
 */
struct QmlAppForeign
{
    Q_GADGET
    QML_FOREIGN(QmlApp)
    QML_SINGLETON
    QML_NAMED_ELEMENT(QTox)

public:
    /** Set before QML first uses QTox. */
    inline static QmlApp* instance = nullptr;

    static QmlApp* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
};
