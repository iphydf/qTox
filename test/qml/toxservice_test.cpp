/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/qml/toxservice.h"

#include "mock/mockbootstraplistgenerator.h"
#include "mock/mockcoresettings.h"
#include "src/core/core.h"
#include "src/core/corefile.h"
#include "src/core/groupid.h"
#include "src/core/toxid.h"
#include "src/model/conferenceinvite.h"
#include "src/model/groupinvite.h"
#include "src/model/identicon.h"
#include "src/model/status.h"
#include "src/qml/callstate.h"
#include "src/qml/chatkind.h"
#include "src/qml/inotifier.h"
#include "src/qml/peerrole.h"
#include "src/qml/qmlchat.h"
#include "src/qml/remotemodels.h"
#include "src/qml/voicestate.h"
#include "test/qml/testservice.h"

#include <QAbstractItemModel>
#include <QAbstractItemModelReplica>
#include <QBuffer>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include <limits>
#include <memory>
#include <tox/tox.h>
#include <tox/toxav.h>
#include <tuple>
#include <vector>

// The QML module is a static library's.
Q_IMPORT_QML_PLUGIN(qToxPlugin)

namespace {
// Long enough for two Tox instances on this computer to find each other, also again after a
// login, which took up to 68 seconds while another build ran.
constexpr int networkTimeout = 90000;

class RecordingNotifier : public INotifier
{
public:
    ~RecordingNotifier() override;

    void show(const QString& tag, const QString& title, const QString& text, const QImage& icon,
              Actions actions_) override
    {
        std::ignore = title;
        std::ignore = icon;
        shown.insert(tag, text);
        actions.insert(tag, actions_);
    }

    void cancel(const QString& tag) override
    {
        shown.remove(tag);
    }

    void showStatus(const QString& status) override
    {
        statuses.append(status);
    }

    void setRecording(bool microphone, bool camera) override
    {
        recording = microphone;
        filming = camera;
    }

    // The texts of the notifications on screen, by tag.
    QHash<QString, QString> shown;
    // What the user can do from them, by tag.
    QHash<QString, Actions> actions;
    // What the service's ongoing notification said, in order.
    QStringList statuses;
    bool recording{false};
    bool filming{false};
};

RecordingNotifier::~RecordingNotifier() = default;

/**
 * @brief A Tox that relays over TCP, as public bootstrap nodes do.
 */
class TcpRelay
{
public:
    TcpRelay()
    {
        const std::unique_ptr<Tox_Options, decltype(&tox_options_free)> options{tox_options_new(nullptr),
                                                                                tox_options_free};
        tox_options_set_local_discovery_enabled(options.get(), false);
        // Any free port: tox_new() fails on one in use.
        for (quint16 port = 33700; tox == nullptr && port < 33800; ++port) {
            tox_options_set_tcp_port(options.get(), port);
            tox.reset(tox_new(options.get(), nullptr));
        }
        if (tox != nullptr) {
            QObject::connect(&timer, &QTimer::timeout, &timer,
                             [this] { tox_iterate(tox.get(), nullptr); });
            timer.start(tox_iteration_interval(tox.get()));
        }
    }

    bool isRunning() const
    {
        return tox != nullptr;
    }

    QString dhtId() const
    {
        QByteArray id(TOX_PUBLIC_KEY_SIZE, 0);
        tox_self_get_dht_id(tox.get(), reinterpret_cast<uint8_t*>(id.data()));
        return QString::fromLatin1(id.toHex().toUpper());
    }

    quint16 udpPort() const
    {
        return tox_self_get_udp_port(tox.get(), nullptr);
    }

    quint16 tcpPort() const
    {
        return tox_self_get_tcp_port(tox.get(), nullptr);
    }

private:
    std::unique_ptr<Tox, decltype(&tox_kill)> tox{nullptr, tox_kill};
    QTimer timer;
};

/**
 * @brief A Tox that calls and answers with ToxAV, as other clients do.
 */
class Carol
{
public:
    Carol(const QString& nodeKey, quint16 nodePort)
    {
        const std::unique_ptr<Tox_Options, decltype(&tox_options_free)> options{tox_options_new(nullptr),
                                                                                tox_options_free};
        tox_options_set_local_discovery_enabled(options.get(), false);
        tox.reset(tox_new(options.get(), nullptr));
        if (tox == nullptr) {
            return;
        }
        const QByteArray name{"Carol"};
        tox_self_set_name(tox.get(), reinterpret_cast<const uint8_t*>(name.constData()),
                          name.size(), nullptr);
        const QByteArray key = QByteArray::fromHex(nodeKey.toLatin1());
        tox_bootstrap(tox.get(), "127.0.0.1", nodePort,
                      reinterpret_cast<const uint8_t*>(key.constData()), nullptr);
        av.reset(toxav_new(tox.get(), nullptr));
        toxav_callback_call(
            av.get(),
            [](ToxAV*, uint32_t, bool, bool, void* self) {
                static_cast<Carol*>(self)->invited = true;
            },
            this);
        // ToxAV calls only with somewhere for the sound to go.
        toxav_callback_audio_receive_frame(
            av.get(), [](ToxAV*, uint32_t, const int16_t*, size_t, uint8_t, uint32_t, void*) {},
            nullptr);
        toxav_callback_call_state(
            av.get(),
            [](ToxAV*, uint32_t, uint32_t state, void* self) {
                static_cast<Carol*>(self)->callState = state;
            },
            this);
        QObject::connect(&timer, &QTimer::timeout, &timer, [this] {
            tox_iterate(tox.get(), this);
            toxav_iterate(av.get());
        });
        timer.start(std::min(tox_iteration_interval(tox.get()), toxav_iteration_interval(av.get())));
    }

    bool isRunning() const
    {
        return av != nullptr;
    }

    QString publicKey() const
    {
        QByteArray key(TOX_PUBLIC_KEY_SIZE, 0);
        tox_self_get_public_key(tox.get(), reinterpret_cast<uint8_t*>(key.data()));
        return QString::fromLatin1(key.toHex().toUpper());
    }

    /** Sends a friend request: the friend is the first. */
    bool befriend(const QString& toxId)
    {
        const QByteArray address = QByteArray::fromHex(toxId.toLatin1());
        const QByteArray message{"Call me"};
        return tox_friend_add(tox.get(), reinterpret_cast<const uint8_t*>(address.constData()),
                              reinterpret_cast<const uint8_t*>(message.constData()), message.size(),
                              nullptr)
               == 0;
    }

    bool isFriendOnline() const
    {
        return tox_friend_get_connection_status(tox.get(), 0, nullptr) != TOX_CONNECTION_NONE;
    }

    bool call(bool video = false)
    {
        return toxav_call(av.get(), 0, audioBitrate, video ? videoBitrate : 0, nullptr);
    }

    /** A frame of one colour, in YUV as ToxAV takes it. */
    bool sendFrame(uint8_t y, uint8_t u, uint8_t v)
    {
        constexpr uint16_t width = 320;
        constexpr uint16_t height = 240;
        const std::vector<uint8_t> luma(width * height, y);
        const std::vector<uint8_t> blue(width * height / 4, u);
        const std::vector<uint8_t> red(width * height / 4, v);
        return toxav_video_send_frame(av.get(), 0, width, height, luma.data(), blue.data(),
                                      red.data(), nullptr);
    }

    bool answer()
    {
        return toxav_answer(av.get(), 0, audioBitrate, 0, nullptr);
    }

    bool hangUp()
    {
        return toxav_call_control(av.get(), 0, TOXAV_CALL_CONTROL_CANCEL, nullptr);
    }

    // Whether the friend called.
    bool invited{false};
    // The call's Toxav_Friend_Call_State flags.
    uint32_t callState{0};

private:
    static constexpr uint32_t audioBitrate = 48;
    static constexpr uint32_t videoBitrate = 5000;

    std::unique_ptr<Tox, decltype(&tox_kill)> tox{nullptr, tox_kill};
    std::unique_ptr<ToxAV, decltype(&toxav_kill)> av{nullptr, toxav_kill};
    QTimer timer;
};

QVariant data(const QAbstractItemModel& model, int row, const QByteArray& role)
{
    return model.data(model.index(row, 0), model.roleNames().key(role));
}

/** The first row in @p chats of a chat of @p kind, or -1. */
int findChat(const QAbstractItemModel& chats, ChatKind::Kind kind)
{
    for (int row = 0; row < chats.rowCount(); ++row) {
        if (data(chats, row, "kind").toInt() == static_cast<int>(kind)) {
            return row;
        }
    }
    return -1;
}

/** A picture of one colour, as a PNG. */
QByteArray png(const QColor& colour, int side)
{
    QImage image{side, side, QImage::Format_RGB32};
    image.fill(colour);
    QByteArray data;
    QBuffer buffer{&data};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

/** The last message in @p chat, or nothing. */
/** The first peer in a chat who isn't the user, as QmlChat::getPeers() has them. */
QVariantMap otherPeer(const QmlChat& chat)
{
    for (const QVariant& peer : chat.getPeers()) {
        if (!peer.toMap().value("self").toBool()) {
            return peer.toMap();
        }
    }
    return {};
}

QString lastMessage(QmlChat& chat)
{
    const QAbstractItemModel& messages = *chat.getMessages();
    return data(messages, messages.rowCount() - 1, "text").toString();
}
} // namespace

/**
 * @brief Alice uses qTox's Tox service through the user interface's QmlApp. Bob is plain Tox.
 */
class TestToxService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void openRequests();
    void profiles();
    void settingsArePerProfile();
    void settingChangedJustBeforeLogout();
    void friendMessages();
    void friendChangesSavedAtOnce();
    void selfStatus();
    void typing();
    void notifications();
    void receiveFile();
    void sendFile();
    void conferences();
    void chatList();
    void conferenceManagement();
    void groups();
    void groupModeration();
    void chatListRows();
    void history();
    void removeHistory();
    void friendAlias();
    void friendAvatar();
    void selfAvatar();
    void fileThroughStatusChange();
    void statusChangeMessages();
    void autoAcceptFiles();
    void pauseFile();
    void replyFromNotification();
    void removeFriend();
    void calls();
    void logoutStatus();
    void autoLogin();
    void profileManagement();
    void nospam();
    void onlineThroughRelay();

private:
    QmlApp& qtox();
    QmlChat& chatWithBob();
    bool bobSays(const QString& text);
    bool serviceCaughtUp();
    QByteArray savedTox() const;
    QString savedToxId() const;

    // qTox's window in front: only then does the user interface tell the service which chat the
    // user reads.
    QWindow window;
    MockSettings bobSettings;
    MockBootstrapListGenerator bobNodes;
    ToxCorePtr bob;
    RecordingNotifier notifier;
    std::unique_ptr<TestService> alice;
};

void TestToxService::initTestCase()
{
    bob = Core::makeToxCore({}, bobSettings, bobNodes).first;
    QVERIFY(bob != nullptr);
    bob->start();

    const DhtServer bobsNode = MockBootstrapListGenerator::makeListFromSelf(*bob).first();
    alice = std::make_unique<TestService>(notifier, bobsNode.publicKey.toString(), bobsNode.udpPort);
    QTRY_VERIFY(qtox().isConnected());
    QCOMPARE(notifier.statuses, QStringList{"Not logged in"});
    // Alice finds only Bob, not the other Tox apps on this computer.
    qtox().setLanDiscoveryEnabled(false);

    window.show();
    window.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&window));
}

QmlApp& TestToxService::qtox()
{
    return *alice->qtox;
}

QmlChat& TestToxService::chatWithBob()
{
    return *qtox().chat(ChatKind::Kind::Friend, bob->getSelfPublicKey().toString());
}

/** Whether the service handled the user interface's calls so far, which it handles in order. */
bool TestToxService::serviceCaughtUp()
{
    static int calls = 0;
    const QString message = QStringLiteral("Call %1").arg(++calls);
    qtox().setSelfStatusMessage(message);
    return QTest::qWaitFor([&] { return qtox().getSelfStatusMessage() == message; });
}

/** Alice's Tox save file. */
QByteArray TestToxService::savedTox() const
{
    QFile save{alice->dir.filePath("alice.tox")};
    return save.open(QIODevice::ReadOnly) ? save.readAll() : QByteArray{};
}

/** The Tox ID in Alice's save file. */
QString TestToxService::savedToxId() const
{
    const QByteArray save = savedTox();
    const std::unique_ptr<Tox_Options, decltype(&tox_options_free)> options{tox_options_new(nullptr),
                                                                            tox_options_free};
    tox_options_set_savedata_type(options.get(), TOX_SAVEDATA_TYPE_TOX_SAVE);
    tox_options_set_savedata_data(options.get(), reinterpret_cast<const uint8_t*>(save.constData()),
                                  save.size());
    // Offline.
    tox_options_set_udp_enabled(options.get(), false);
    tox_options_set_local_discovery_enabled(options.get(), false);
    const std::unique_ptr<Tox, decltype(&tox_kill)> tox{tox_new(options.get(), nullptr), tox_kill};
    if (tox == nullptr) {
        return {};
    }
    QByteArray address(TOX_ADDRESS_SIZE, 0);
    tox_self_get_address(tox.get(), reinterpret_cast<uint8_t*>(address.data()));
    return QString::fromLatin1(address.toHex().toUpper());
}

/** @return Whether Alice got it. */
bool TestToxService::bobSays(const QString& text)
{
    ReceiptNum receipt;
    return bob->sendMessage(bob->getFriendList().first(), text, receipt)
           && QTest::qWaitFor([&] { return lastMessage(chatWithBob()) == text; }, networkTimeout);
}

void TestToxService::openRequests()
{
    const QSignalSpy requested{&qtox(), &QmlApp::openRequested};
    qtox().open(RemoteModels::chat(ChatKind::Kind::Conference, "ABCD"));
    QCOMPARE(requested.count(), 1);
    QCOMPARE(qtox().takeOpenRequest(),
             (QVariantMap{{"page", "chat"},
                          {"kind", static_cast<int>(ChatKind::Kind::Conference)},
                          {"chatId", "ABCD"}}));
    // Once only.
    QCOMPARE(qtox().takeOpenRequest(), QVariantMap{});

    qtox().open(RemoteModels::invites);
    QCOMPARE(qtox().takeOpenRequest(), (QVariantMap{{"page", "requests"}}));

    for (const QString& name : QStringList{"chat/9/ABCD", "chat/0/", "chat/0", "friends"}) {
        qtox().open(name);
        QCOMPARE(qtox().takeOpenRequest(), QVariantMap{});
    }
}

void TestToxService::profiles()
{
    qtox().createProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QVERIFY(qtox().getProfiles().contains("alice"));
    QVERIFY(ToxId::isValidToxId(qtox().getSelfId()));
    qtox().setSelfName("Alice");
    QTRY_COMPARE(qtox().getSelfName(), QStringLiteral("Alice"));

    const QString selfId = qtox().getSelfId();
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getSelfId(), selfId);
    QCOMPARE(qtox().getSelfName(), QStringLiteral("Alice"));
}

void TestToxService::settingsArePerProfile()
{
    // The proxy is a profile's, and the port alone doesn't turn it on.
    constexpr int port = 9050;
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().createProfile("carol", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getProxyPort(), 0);
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());

    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    qtox().setProxyPort(port);
    QTRY_COMPARE(qtox().getProxyPort(), port);
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());

    qtox().loadProfile("carol", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getProxyPort(), 0);
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());

    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getProxyPort(), port);
}

void TestToxService::settingChangedJustBeforeLogout()
{
    constexpr int port = 9051;
    qtox().setProxyPort(port);
    QVERIFY(serviceCaughtUp());
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    // Longer than Settings waits to save a change: half a second.
    QTest::qWait(1000);

    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getProxyPort(), port);
}

void TestToxService::friendMessages()
{
    bob->requestFriendship(ToxId{qtox().getSelfId()}, "Hi Alice");
    QTRY_COMPARE_WITH_TIMEOUT(qtox().getFriendRequestCount(), 1, networkTimeout);
    QCOMPARE(notifier.shown.value(RemoteModels::friendRequests), QStringLiteral("Hi Alice"));

    qtox().acceptFriendRequest(bob->getSelfPublicKey().toString());
    QTRY_COMPARE(qtox().getFriendRequestCount(), 0);
    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, 0, "online").toBool(), networkTimeout);

    const QSignalSpy bobReceived{bob.get(), &Core::friendMessageReceived};
    chatWithBob().sendMessage("Hello Bob");
    QTRY_COMPARE_WITH_TIMEOUT(bobReceived.count(), 1, networkTimeout);
    QCOMPARE(bobReceived[0][1].toString(), QStringLiteral("Hello Bob"));

    QVERIFY(bobSays("Hello Alice"));
}

void TestToxService::friendChangesSavedAtOnce()
{
    // Android ends the service without a logout, which would save them.
    bob->setUsername("Bobby");
    bob->setStatusMessage("Away for a bit");
    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_COMPARE_WITH_TIMEOUT(data(chats, 0, "statusMessage").toString(),
                              QStringLiteral("Away for a bit"), networkTimeout);
    QTRY_COMPARE(data(chats, 0, "name").toString(), QStringLiteral("Bobby"));

    QTRY_VERIFY(savedTox().contains("Bobby"));
    QTRY_VERIFY(savedTox().contains("Away for a bit"));
}

void TestToxService::selfStatus()
{
    const QSignalSpy bobSees{bob.get(), &Core::friendStatusChanged};
    qtox().setSelfStatus(static_cast<int>(Status::Status::Busy));
    QTRY_COMPARE(qtox().getSelfStatus(), static_cast<int>(Status::Status::Busy));
    QTRY_VERIFY_WITH_TIMEOUT(!bobSees.isEmpty()
                                 && bobSees.last()[1].value<Status::Status>() == Status::Status::Busy,
                             networkTimeout);

    // Core takes only what friends can see.
    qtox().setSelfStatus(static_cast<int>(Status::Status::Offline));
    QVERIFY(serviceCaughtUp());
    QCOMPARE(qtox().getSelfStatus(), static_cast<int>(Status::Status::Busy));

    qtox().setSelfStatus(static_cast<int>(Status::Status::Online));
    QTRY_COMPARE(qtox().getSelfStatus(), static_cast<int>(Status::Status::Online));
}

void TestToxService::typing()
{
    QSignalSpy bobSees{bob.get(), &Core::friendTypingChanged};
    chatWithBob().setDraft("H");
    QTRY_VERIFY_WITH_TIMEOUT(!bobSees.isEmpty() && bobSees.last()[1].toBool(), networkTimeout);
    chatWithBob().setDraft({});
    QTRY_VERIFY_WITH_TIMEOUT(!bobSees.last()[1].toBool(), networkTimeout);

    // Not when the user turned it off.
    qtox().setTypingNotification(false);
    QTRY_VERIFY(!qtox().getTypingNotification());
    bobSees.clear();
    chatWithBob().setDraft("H");
    QVERIFY(serviceCaughtUp());
    QVERIFY(!QTest::qWaitFor([&] { return !bobSees.isEmpty(); }, 1000));
    chatWithBob().setDraft({});
    qtox().setTypingNotification(true);
    QTRY_VERIFY(qtox().getTypingNotification());

    bob->sendTyping(bob->getFriendList().first(), true);
    QTRY_VERIFY_WITH_TIMEOUT(chatWithBob().isTyping(), networkTimeout);
    bob->sendTyping(bob->getFriendList().first(), false);
    QTRY_VERIFY_WITH_TIMEOUT(!chatWithBob().isTyping(), networkTimeout);
}

void TestToxService::friendAvatar()
{
    const QAbstractItemModel& chats = *qtox().getChats();
    const QString identicon = QStringLiteral("%1/%2/")
                                  .arg(static_cast<int>(ChatKind::Kind::Friend))
                                  .arg(bob->getSelfPublicKey().toString());
    QCOMPARE(qtox().avatar(identicon), Identicon{bob->getSelfPublicKey().getByteArray()}.toImage(64));

    QTRY_VERIFY_WITH_TIMEOUT(data(chats, 0, "online").toBool(), networkTimeout);
    const QByteArray avatar = png(Qt::red, 32);
    bob->getCoreFile()->sendAvatarFile(bob->getFriendList().first(), avatar);
    QTRY_COMPARE_WITH_TIMEOUT(data(chats, 0, "avatar").toByteArray(), avatar, networkTimeout);
    const QString source =
        QStringLiteral("%1/%2/%3")
            .arg(static_cast<int>(ChatKind::Kind::Friend))
            .arg(bob->getSelfPublicKey().toString(), data(chats, 0, "avatarHash").toString());
    QCOMPARE(qtox().avatar(source).pixelColor(16, 16), QColor{Qt::red});

    // The profile keeps it.
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QTRY_COMPARE(data(chats, 0, "avatar").toByteArray(), avatar);
}

void TestToxService::selfAvatar()
{
    QTemporaryFile picture;
    QVERIFY(picture.open());
    picture.write(png(Qt::blue, 512));
    picture.close();
    qtox().setAvatar(QUrl::fromLocalFile(picture.fileName()));
    QTRY_VERIFY(qtox().hasOwnAvatar());
    const QImage shown = qtox().avatar(qtox().getSelfAvatar().remove("image://avatar/"));
    QCOMPARE(shown.pixelColor(0, 0), QColor{Qt::blue});

    const QSignalSpy errors{&qtox(), &QmlApp::error};
    QTemporaryFile notPicture;
    QVERIFY(notPicture.open());
    notPicture.write("Not a picture");
    notPicture.close();
    qtox().setAvatar(QUrl::fromLocalFile(notPicture.fileName()));
    QCOMPARE(errors.count(), 1);

    // The identicon, as the Widgets UI shows it.
    qtox().removeAvatar();
    QTRY_VERIFY(!qtox().hasOwnAvatar());
    QCOMPARE(qtox().avatar(qtox().getSelfAvatar().remove("image://avatar/")),
             Identicon{ToxPk{qtox().getSelfId().left(ToxPk::numHexChars)}.getByteArray()}.toImage(64));
}

void TestToxService::notifications()
{
    const QString tag = RemoteModels::chat(ChatKind::Kind::Friend, bob->getSelfPublicKey().toString());
    QCOMPARE(notifier.shown.value(tag), QStringLiteral("Hello Alice"));

    // Reading the chat clears its notification, and needs no more.
    qtox().showingChat(ChatKind::Kind::Friend, bob->getSelfPublicKey().toString());
    QTRY_VERIFY(!notifier.shown.contains(tag));
    QVERIFY(bobSays("Are you there?"));
    QVERIFY(!notifier.shown.contains(tag));

    qtox().hidingChat(ChatKind::Kind::Friend, bob->getSelfPublicKey().toString());
    QVERIFY(serviceCaughtUp());
    QVERIFY(bobSays("Bye then"));
    QTRY_COMPARE(notifier.shown.value(tag), QStringLiteral("Bye then"));
}

void TestToxService::receiveFile()
{
    const QByteArray contents = QByteArray{"Hello, file."}.repeated(10000);
    QTemporaryFile sent;
    QVERIFY(sent.open());
    sent.write(contents);
    sent.close();
    bob->getCoreFile()->sendFile(bob->getFriendList().first(), "letter.txt", sent.fileName(),
                                 contents.size());

    const QAbstractItemModel& messages = *chatWithBob().getMessages();
    const auto last = [&](const QByteArray& role) {
        return data(messages, messages.rowCount() - 1, role);
    };
    QTRY_COMPARE_WITH_TIMEOUT(last("text").toString(), QStringLiteral("letter.txt"), networkTimeout);
    QTRY_COMPARE(last("fileStatus").toString(), QStringLiteral("waiting"));
    QTRY_COMPARE(last("fileSize").toLongLong(), contents.size());
    const QString tag = RemoteModels::chat(ChatKind::Kind::Friend, bob->getSelfPublicKey().toString());
    QTRY_VERIFY(notifier.shown.value(tag).startsWith(QStringLiteral("letter.txt (")));

    // The user interface asks for each value when it reads it first.
    QTRY_VERIFY(last("fileNum").isValid());
    chatWithBob().acceptFile(last("fileNum").toInt());
    QTRY_COMPARE_WITH_TIMEOUT(last("fileStatus").toString(), QStringLiteral("finished"),
                              networkTimeout);
    QTRY_COMPARE(last("fileTransferred").toLongLong(), contents.size());
    QTRY_VERIFY(!last("filePath").toString().isEmpty());
    QFile received{last("filePath").toString()};
    QCOMPARE(QFileInfo{received}.dir().absolutePath(), QDir{alice->dir.path()}.absolutePath());
    QVERIFY(received.open(QIODevice::ReadOnly));
    QCOMPARE(received.readAll(), contents);
}

void TestToxService::fileThroughStatusChange()
{
    const QAbstractItemModel& chats = *qtox().getChats();
    const int row = findChat(chats, ChatKind::Kind::Friend);
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, row, "online").toBool(), networkTimeout);
    const QByteArray contents = QByteArray{"Back soon."}.repeated(1000);
    QTemporaryFile sent;
    QVERIFY(sent.open());
    sent.write(contents);
    sent.close();
    bob->getCoreFile()->sendFile(bob->getFriendList().first(), "note.txt", sent.fileName(),
                                 contents.size());
    const QAbstractItemModel& messages = *chatWithBob().getMessages();
    const auto last = [&](const QByteArray& role) {
        return data(messages, messages.rowCount() - 1, role);
    };
    QTRY_COMPARE_WITH_TIMEOUT(last("text").toString(), QStringLiteral("note.txt"), networkTimeout);
    QTRY_VERIFY(last("fileNum").isValid());

    // Toxcore keeps the transfer when the friend's status changes.
    bob->setStatus(Status::Status::Away);
    QTRY_COMPARE_WITH_TIMEOUT(data(chats, row, "status").toInt(),
                              static_cast<int>(Status::Status::Away), networkTimeout);
    chatWithBob().acceptFile(last("fileNum").toInt());
    QTRY_COMPARE_WITH_TIMEOUT(last("fileStatus").toString(), QStringLiteral("finished"),
                              networkTimeout);

    bob->setStatus(Status::Status::Online);
    QTRY_COMPARE_WITH_TIMEOUT(data(chats, row, "status").toInt(),
                              static_cast<int>(Status::Status::Online), networkTimeout);
}

void TestToxService::sendFile()
{
    const QByteArray contents = QByteArray{"Hello, Bob."}.repeated(10000);
    QTemporaryFile sent;
    QVERIFY(sent.open());
    sent.write(contents);
    sent.close();

    QTemporaryDir bobsFiles;
    connect(bob->getCoreFile(), &CoreFile::fileReceiveRequested, this, [&](const ToxFile& file) {
        bob->getCoreFile()->acceptFileRecvRequest(file.friendId, file.fileNum,
                                                  bobsFiles.filePath(file.fileName));
    });
    chatWithBob().sendFile(QUrl::fromLocalFile(sent.fileName()));

    const QAbstractItemModel& messages = *chatWithBob().getMessages();
    const auto last = [&](const QByteArray& role) {
        return data(messages, messages.rowCount() - 1, role);
    };
    QTRY_COMPARE(last("text").toString(), QFileInfo{sent.fileName()}.fileName());
    QTRY_COMPARE_WITH_TIMEOUT(last("fileStatus").toString(), QStringLiteral("finished"),
                              networkTimeout);
    QTRY_VERIFY(last("isSelf").toBool());
    QFile received{bobsFiles.filePath(QFileInfo{sent.fileName()}.fileName())};
    QTRY_VERIFY(received.exists());
    QVERIFY(received.open(QIODevice::ReadOnly));
    QTRY_COMPARE(received.size(), contents.size());
    QCOMPARE(received.readAll(), contents);
    disconnect(bob->getCoreFile(), nullptr, this, nullptr);
}

void TestToxService::conferences()
{
    // The service takes only IDs that are IDs.
    qtox().leaveConference("ABCD");
    QVERIFY(serviceCaughtUp());

    const int conference = bob->createConference(TOX_CONFERENCE_TYPE_TEXT);
    bob->conferenceInviteFriend(bob->getFriendList().first(), conference);
    QTRY_COMPARE_WITH_TIMEOUT(qtox().getInviteCount(), 1, networkTimeout);
    QVERIFY(notifier.shown.contains(RemoteModels::invites));

    const QAbstractItemModel& invites = *qtox().getInvites();
    QTRY_VERIFY(!data(invites, 0, "inviteId").toString().isEmpty());
    qtox().acceptInvite(data(invites, 0, "inviteId").toString());
    QTRY_COMPARE(qtox().getInviteCount(), 0);
    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_VERIFY(
        findChat(chats, ChatKind::Kind::Conference) >= 0
        && !data(chats, findChat(chats, ChatKind::Kind::Conference), "chatId").toString().isEmpty());
    const QString id = data(chats, findChat(chats, ChatKind::Kind::Conference), "chatId").toString();

    const QSignalSpy bobReceived{bob.get(), &Core::conferenceMessageReceived};
    qtox().chat(ChatKind::Kind::Conference, id)->sendMessage("Hello everyone");
    QTRY_COMPARE_WITH_TIMEOUT(bobReceived.count(), 1, networkTimeout);
    QCOMPARE(bobReceived[0][2].toString(), QStringLiteral("Hello everyone"));
}

void TestToxService::chatList()
{
    // Alice wrote in the conference last.
    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_COMPARE(data(chats, 0, "lastMessage").toString(), QStringLiteral("Hello everyone"));
    QCOMPARE(data(chats, 0, "kind").toInt(), static_cast<int>(ChatKind::Kind::Conference));
    QVERIFY(data(chats, 0, "lastIsSelf").toBool());
    // The file Alice sent, after "Bye then" and the file Bob offered.
    QVERIFY(data(chats, 1, "lastIsSelf").toBool());
    QCOMPARE(data(chats, 1, "unread").toInt(), 2);

    QVERIFY(bobSays("Anyone there?"));
    QTRY_COMPARE(data(chats, 0, "lastMessage").toString(), QStringLiteral("Anyone there?"));
    QCOMPARE(data(chats, 0, "chatId").toString(), bob->getSelfPublicKey().toString());
    QTRY_COMPARE(data(chats, 0, "unread").toInt(), 3);

    qtox().showingChat(ChatKind::Kind::Friend, bob->getSelfPublicKey().toString());
    QTRY_COMPARE(data(chats, 0, "unread").toInt(), 0);
    QVERIFY(bobSays("Hi again"));
    QVERIFY(serviceCaughtUp());
    QCOMPARE(data(chats, 0, "unread").toInt(), 0);
    qtox().hidingChat(ChatKind::Kind::Friend, bob->getSelfPublicKey().toString());

    qtox().leaveConference(data(chats, 1, "chatId").toString());
    QTRY_COMPARE(chats.rowCount(), 1);
    QCOMPARE(findChat(chats, ChatKind::Kind::Conference), -1);
}

void TestToxService::conferenceManagement()
{
    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, 0, "online").toBool(), networkTimeout);
    const QString bobPk = bob->getSelfPublicKey().toString();
    QObject::connect(bob.get(), &Core::conferenceInviteReceived, bob.get(),
                     [this](const ConferenceInvite& invite) { bob->joinConference(invite); });
    const QSignalSpy bobTitles{bob.get(), &Core::conferenceTitleChanged};

    const QSignalSpy created{&qtox(), &QmlApp::chatAdded};
    qtox().createConference("Plans", bobPk);
    QTRY_COMPARE(created.count(), 1);
    const QString id = created[0][1].toString();
    QmlChat& conference = *qtox().chat(ChatKind::Kind::Conference, id);
    QTRY_COMPARE(conference.getName(), QStringLiteral("Plans"));
    // Bob joins.
    QTRY_COMPARE_WITH_TIMEOUT(conference.getPeers().size(), 2, networkTimeout);
    QStringList peers;
    for (const QVariant& peer : conference.getPeers()) {
        peers.append(peer.toMap().value("publicKey").toString());
    }
    QVERIFY(peers.contains(bobPk));

    conference.setTitle("New plans");
    QTRY_COMPARE(conference.getName(), QStringLiteral("New plans"));
    QTRY_VERIFY_WITH_TIMEOUT(!bobTitles.isEmpty()
                                 && bobTitles.last()[2].toString() == QStringLiteral("New plans"),
                             networkTimeout);

    // Its call, which is on at once.
    QTRY_VERIFY(conference.hasAudio());
    qtox().joinConferenceCall(id);
    QTRY_COMPARE(qtox().getCallState(), static_cast<int>(CallState::State::Active));
    QCOMPARE(qtox().getCallKind(), static_cast<int>(ChatKind::Kind::Conference));
    QVERIFY(notifier.recording);
    qtox().toggleCallMic();
    QTRY_VERIFY(qtox().isCallMicMuted());
    // Leaving ends it.
    qtox().leaveConference(id);
    QTRY_COMPARE(qtox().getCallState(), static_cast<int>(CallState::State::None));
    QVERIFY(!notifier.recording);

    // Bob's invites, joined without asking.
    QmlChat& friendChat = chatWithBob();
    friendChat.setAutoAcceptInvites(true);
    QTRY_VERIFY(friendChat.getAutoAcceptInvites());
    const int bobsConference = bob->createConference(TOX_CONFERENCE_TYPE_TEXT);
    bob->conferenceInviteFriend(bob->getFriendList().first(), bobsConference);
    QTRY_VERIFY_WITH_TIMEOUT(findChat(chats, ChatKind::Kind::Conference) >= 0, networkTimeout);
    QCOMPARE(qtox().getInviteCount(), 0);
    QTRY_VERIFY(
        !data(chats, findChat(chats, ChatKind::Kind::Conference), "chatId").toString().isEmpty());
    qtox().leaveConference(
        data(chats, findChat(chats, ChatKind::Kind::Conference), "chatId").toString());
    QTRY_COMPARE(findChat(chats, ChatKind::Kind::Conference), -1);

    // The profile keeps it.
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    // Then the service has Bob.
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, 0, "online").toBool(), networkTimeout);
    QTRY_VERIFY(chatWithBob().getAutoAcceptInvites());
    chatWithBob().setAutoAcceptInvites(false);
    QTRY_VERIFY(!chatWithBob().getAutoAcceptInvites());
}

void TestToxService::groups()
{
    // The service takes only IDs that are IDs.
    qtox().leaveGroup("ABCD");
    QVERIFY(!qtox().joinGroup("ABCD", {}).isEmpty());
    QVERIFY(serviceCaughtUp());

    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, findChat(chats, ChatKind::Kind::Friend), "online").toBool(),
                             networkTimeout);
    const int bobsGroup = bob->createGroup("Bob's group");
    bob->changeGroupTopic(bobsGroup, "Plans");
    bob->groupInviteFriend(bob->getFriendList().first(), bobsGroup);
    QTRY_COMPARE_WITH_TIMEOUT(qtox().getInviteCount(), 1, networkTimeout);
    QVERIFY(notifier.shown.contains(RemoteModels::invites));
    const QAbstractItemModel& invites = *qtox().getInvites();
    QTRY_COMPARE(data(invites, 0, "kind").toInt(), static_cast<int>(ChatKind::Kind::Group));
    QTRY_COMPARE(data(invites, 0, "name").toString(), QStringLiteral("Bob's group"));
    QTRY_VERIFY(!data(invites, 0, "inviteId").toString().isEmpty());
    qtox().acceptInvite(data(invites, 0, "inviteId").toString());
    QTRY_COMPARE(qtox().getInviteCount(), 0);
    const QString id = bob->getGroupPersistentId(bobsGroup).toString();
    QTRY_VERIFY(findChat(chats, ChatKind::Kind::Group) >= 0
                && data(chats, findChat(chats, ChatKind::Kind::Group), "chatId").toString() == id);
    QmlChat& group = *qtox().chat(ChatKind::Kind::Group, id);
    QTRY_COMPARE(group.getName(), QStringLiteral("Bob's group"));
    QTRY_COMPARE_WITH_TIMEOUT(group.getTopic(), QStringLiteral("Plans"), networkTimeout);
    // Bob founded it.
    QTRY_COMPARE_WITH_TIMEOUT(group.getPeers().size(), 2, networkTimeout);
    for (const QVariant& peer : group.getPeers()) {
        const QVariantMap map = peer.toMap();
        QCOMPARE(map.value("role").toInt(),
                 static_cast<int>(map.value("self").toBool() ? PeerRole::Role::User
                                                             : PeerRole::Role::Founder));
    }

    const QSignalSpy bobReceived{bob.get(), &Core::groupMessageReceived};
    group.sendMessage("Hello group");
    QTRY_COMPARE_WITH_TIMEOUT(bobReceived.count(), 1, networkTimeout);
    QCOMPARE(bobReceived[0][2].toString(), QStringLiteral("Hello group"));
    QTRY_COMPARE(data(chats, 0, "lastMessage").toString(), QStringLiteral("Hello group"));
    QVERIFY(data(chats, 0, "lastIsSelf").toBool());
    bob->sendGroupMessage(bobsGroup, "Welcome");
    QTRY_COMPARE_WITH_TIMEOUT(lastMessage(group), QStringLiteral("Welcome"), networkTimeout);

    // Privately, both ways.
    const QVariantMap bobInGroup = otherPeer(group);
    const QSignalSpy bobGotPrivate{bob.get(), &Core::groupPrivateMessageReceived};
    group.setPrivateTo(bobInGroup.value("publicKey").toString());
    QCOMPARE(group.getPrivateToName(), bobInGroup.value("name").toString());
    group.sendMessage("Just you");
    QTRY_COMPARE_WITH_TIMEOUT(bobGotPrivate.count(), 1, networkTimeout);
    QCOMPARE(bobGotPrivate[0][2].toString(), QStringLiteral("Just you"));
    QCOMPARE(bobReceived.count(), 1);
    const QAbstractItemModel& messages = *group.getMessages();
    QTRY_COMPARE(data(messages, messages.rowCount() - 1, "recipient").toString(),
                 bobInGroup.value("name").toString());
    QVERIFY(data(messages, messages.rowCount() - 1, "isPrivate").toBool());
    group.setPrivateTo({});
    bob->sendGroupPrivateMessage(bobsGroup, qvariant_cast<ToxPk>(bobGotPrivate[0][1]), "Just me");
    QTRY_COMPARE_WITH_TIMEOUT(lastMessage(group), QStringLiteral("Just me"), networkTimeout);
    QTRY_VERIFY(data(messages, messages.rowCount() - 1, "isPrivate").toBool());
    // A reply from it would go to everyone.
    const QString tag = RemoteModels::chat(ChatKind::Kind::Group, id);
    QTRY_VERIFY(notifier.shown.contains(tag));
    QCOMPARE(notifier.actions.value(tag), INotifier::Actions::None);

    // Only moderators may write to everyone, but Alice may still write privately.
    QVERIFY(group.canWrite());
    QVERIFY(bob->setGroupVoiceState(bobsGroup, GroupVoiceState::Moderator));
    QTRY_VERIFY_WITH_TIMEOUT(!group.canWrite(), networkTimeout);
    group.setPrivateTo(bobInGroup.value("publicKey").toString());
    QVERIFY(group.canWrite());
    group.setPrivateTo({});

    const QSignalSpy bobSawName{bob.get(), &Core::groupPeerNameChanged};
    group.setNickname("Ally");
    QTRY_COMPARE(group.getNickname(), QStringLiteral("Ally"));
    QTRY_VERIFY_WITH_TIMEOUT(!bobSawName.isEmpty()
                                 && bobSawName.last()[2].toString() == QStringLiteral("Ally"),
                             networkTimeout);

    // The profile keeps it.
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QTRY_VERIFY(findChat(chats, ChatKind::Kind::Group) >= 0);
    QTRY_COMPARE(data(chats, findChat(chats, ChatKind::Kind::Group), "chatId").toString(), id);
    qtox().leaveGroup(id);
    QTRY_COMPARE(findChat(chats, ChatKind::Kind::Group), -1);

    // A password Tox rejects.
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, findChat(chats, ChatKind::Kind::Friend), "online").toBool(),
                             networkTimeout);
    const int locked = bob->createGroup("Locked");
    QVERIFY(bob->setGroupPassword(locked, "secret"));
    bob->groupInviteFriend(bob->getFriendList().first(), locked);
    QTRY_COMPARE_WITH_TIMEOUT(qtox().getInviteCount(), 1, networkTimeout);
    QTRY_VERIFY(!data(invites, 0, "inviteId").toString().isEmpty());
    const QSignalSpy errors{&qtox(), &QmlApp::error};
    qtox().acceptInvite(data(invites, 0, "inviteId").toString(), "guess");
    QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, networkTimeout);
    QCOMPARE(errors[0][1].toString(), QStringLiteral("Wrong password."));
    QTRY_COMPARE(findChat(chats, ChatKind::Kind::Group), -1);
}

void TestToxService::groupModeration()
{
    // Alice's own, which Bob joins.
    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, findChat(chats, ChatKind::Kind::Friend), "online").toBool(),
                             networkTimeout);
    uint32_t bobsNumber = std::numeric_limits<uint32_t>::max();
    QObject::connect(bob.get(), &Core::groupInviteReceived, bob.get(),
                     [this, &bobsNumber](const GroupInvite& invite) {
                         bobsNumber = bob->joinGroup(invite);
                     });
    const QSignalSpy bobTopics{bob.get(), &Core::groupTopicChanged};
    const QSignalSpy bobPrivacy{bob.get(), &Core::groupPrivacyStateChanged};
    const QSignalSpy added{&qtox(), &QmlApp::chatAdded};
    qtox().createGroup("Alice's group", bob->getSelfPublicKey().toString());
    QTRY_COMPARE(added.count(), 1);
    QCOMPARE(added[0][0].value<ChatKind::Kind>(), ChatKind::Kind::Group);
    const QString own = added[0][1].toString();
    QmlChat& ownGroup = *qtox().chat(ChatKind::Kind::Group, own);
    QTRY_COMPARE_WITH_TIMEOUT(ownGroup.getPeers().size(), 2, networkTimeout);
    QCOMPARE(ownGroup.getSelfRole(), PeerRole::Role::Founder);
    ownGroup.setTopic("Dinner");
    QTRY_COMPARE(ownGroup.getTopic(), QStringLiteral("Dinner"));
    QTRY_VERIFY_WITH_TIMEOUT(!bobTopics.isEmpty()
                                 && bobTopics.last()[1].toString() == QStringLiteral("Dinner"),
                             networkTimeout);

    // The founder's settings.
    ownGroup.setPrivate(true);
    QTRY_VERIFY(ownGroup.isPrivate());
    QTRY_VERIFY_WITH_TIMEOUT(!bobPrivacy.isEmpty()
                                 && bobPrivacy.last()[1].value<GroupPrivacyState>()
                                        == GroupPrivacyState::Private,
                             networkTimeout);
    ownGroup.setPassword("secret");
    QTRY_VERIFY(ownGroup.isPasswordSet());
    ownGroup.setPassword({});
    QTRY_VERIFY(!ownGroup.isPasswordSet());
    const bool topicLocked = ownGroup.isTopicLocked();
    ownGroup.setTopicLocked(!topicLocked);
    QTRY_COMPARE(ownGroup.isTopicLocked(), !topicLocked);
    ownGroup.setVoiceState(VoiceState::State::Founder);
    QTRY_COMPARE(ownGroup.getVoiceState(), VoiceState::State::Founder);
    QVERIFY(ownGroup.canWrite());
    ownGroup.setPeerLimit(5);
    QTRY_COMPARE(ownGroup.getPeerLimit(), 5);

    // Bob, moderated.
    const QString bobKey = otherPeer(ownGroup).value("publicKey").toString();
    QVERIFY(ownGroup.canGiveRole(PeerRole::Role::User, PeerRole::Role::Moderator));
    QVERIFY(!ownGroup.canGiveRole(PeerRole::Role::User, PeerRole::Role::Founder));
    ownGroup.setPeerRole(bobKey, PeerRole::Role::Moderator);
    QTRY_COMPARE(otherPeer(ownGroup).value("role").toInt(),
                 static_cast<int>(PeerRole::Role::Moderator));
    QTRY_VERIFY_WITH_TIMEOUT(bob->getGroupPeerRole(static_cast<int>(bobsNumber),
                                                   bob->getGroupSelfPk(static_cast<int>(bobsNumber)))
                                 == GroupRole::Moderator,
                             networkTimeout);
    ownGroup.kick(bobKey);
    QTRY_COMPARE(ownGroup.getPeers().size(), 1);

    qtox().leaveGroup(own);
    QTRY_COMPARE(findChat(chats, ChatKind::Kind::Group), -1);
    QObject::disconnect(bob.get(), &Core::groupInviteReceived, bob.get(), nullptr);
}

void TestToxService::chatListRows()
{
    // The friends page, as the user sees it.
    QmlAppForeign::instance = &qtox();
    QQmlEngine engine;
    QQmlComponent component{&engine};
    component.setData(R"(
        import QtQuick
        import qTox
        Window {
            width: 400
            height: 800
            visible: true
            FriendsPage {
                anchors.fill: parent
            }
        })",
                      QUrl{});
    const std::unique_ptr<QObject> window{component.create()};
    QVERIFY2(window != nullptr, qPrintable(component.errorString()));
    const auto shown = [&window] {
        QStringList texts;
        QList<QQuickItem*> items{qobject_cast<QQuickWindow*>(window.get())->contentItem()};
        while (!items.isEmpty()) {
            const QQuickItem* item = items.takeLast();
            if (item->isVisible() && item->inherits("QQuickText")) {
                texts.append(item->property("text").toString());
            }
            items += item->childItems();
        }
        return texts;
    };

    chatWithBob().setAutoAcceptInvites(true);
    QTRY_VERIFY(chatWithBob().getAutoAcceptInvites());
    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, 0, "online").toBool(), networkTimeout);
    const int conference = bob->createConference(TOX_CONFERENCE_TYPE_TEXT);
    bob->changeConferenceTitle(conference, "Night");
    bob->conferenceInviteFriend(bob->getFriendList().first(), conference);
    QTRY_VERIFY_WITH_TIMEOUT(shown().contains("Night"), networkTimeout);
    // Until Alice is in it.
    QTRY_VERIFY_WITH_TIMEOUT(bob->getConferenceNumberPeers(conference) == 2, networkTimeout);
    bob->sendConferenceMessage(conference, "Hello from Bob");
    QTRY_VERIFY2_WITH_TIMEOUT(shown().contains("Bobby: Hello from Bob"),
                              qPrintable(shown().join(" | ")), networkTimeout);

    // Also from the history.
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QTRY_VERIFY2(shown().contains("Bobby: Hello from Bob"), qPrintable(shown().join(" | ")));
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, findChat(chats, ChatKind::Kind::Friend), "online").toBool(),
                             networkTimeout);

    chatWithBob().setAutoAcceptInvites(false);
    QTRY_VERIFY(
        !data(chats, findChat(chats, ChatKind::Kind::Conference), "chatId").toString().isEmpty());
    qtox().leaveConference(
        data(chats, findChat(chats, ChatKind::Kind::Conference), "chatId").toString());
    QTRY_COMPARE(findChat(chats, ChatKind::Kind::Conference), -1);
}

void TestToxService::history()
{
    // More than a chat shows at first, which it loads from the history as the user scrolls up.
    constexpr int count = 150;
    ReceiptNum receipt;
    for (int i = 1; i <= count; ++i) {
        QVERIFY(bob->sendMessage(bob->getFriendList().first(), QString::number(i), receipt));
    }
    QTRY_COMPARE_WITH_TIMEOUT(lastMessage(chatWithBob()), QString::number(count), networkTimeout);
    const int rows = chatWithBob().getMessages()->rowCount();

    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    auto& messages = static_cast<QAbstractItemModelReplica&>(*chatWithBob().getMessages());
    QTRY_COMPARE(messages.rowCount(), rows);
    QTRY_COMPARE(data(messages, rows - 1, "text").toString(), QString::number(count));
    // Not before the user scrolls up.
    QVERIFY(!messages.hasData(messages.index(0, 0), messages.roleNames().key("text")));
    QTRY_COMPARE(data(messages, 0, "text").toString(), QStringLiteral("Hello Bob"));

    // Back from the newest, also through what the chat hasn't loaded.
    QSignalSpy found{&chatWithBob(), &QmlChat::found};
    chatWithBob().find("hello bob", rows - 1, true);
    QTRY_COMPARE(found.count(), 1);
    QCOMPARE(found[0][0].toInt(), 0);
    chatWithBob().find("75", rows - 1, true);
    QTRY_COMPARE(found.count(), 2);
    QTRY_COMPARE(data(messages, found[1][0].toInt(), "text").toString(), QStringLiteral("75"));
    chatWithBob().find("Nobody wrote this", rows - 1, true);
    QTRY_COMPARE(found.count(), 3);
    QCOMPARE(found[2][0].toInt(), -1);
}

void TestToxService::removeHistory()
{
    QAbstractItemModel& messages = *chatWithBob().getMessages();
    QVERIFY(messages.rowCount() > 1);
    chatWithBob().removeHistory();
    // Only that it was cleared.
    QTRY_COMPARE(messages.rowCount(), 1);
    QTRY_COMPARE(data(messages, 0, "kind").toString(), QStringLiteral("system"));

    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QVERIFY(serviceCaughtUp());
    QCOMPARE(chatWithBob().getMessages()->rowCount(), 0);
}

void TestToxService::friendAlias()
{
    chatWithBob().setAlias("Robert");
    QTRY_COMPARE(chatWithBob().getName(), QStringLiteral("Robert"));
    QCOMPARE(chatWithBob().getUserName(), QStringLiteral("Bobby"));
    chatWithBob().setNote("From the conference");
    QTRY_COMPARE(chatWithBob().getNote(), QStringLiteral("From the conference"));

    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QTRY_COMPARE(chatWithBob().getName(), QStringLiteral("Robert"));
    QTRY_COMPARE(chatWithBob().getNote(), QStringLiteral("From the conference"));

    chatWithBob().setAlias({});
    QTRY_COMPARE(chatWithBob().getName(), QStringLiteral("Bobby"));
}

void TestToxService::statusChangeMessages()
{
    qtox().setStatusChangeMessages(true);
    QTRY_VERIFY(qtox().getStatusChangeMessages());
    bob->setStatus(Status::Status::Busy);
    QTRY_VERIFY_WITH_TIMEOUT(lastMessage(chatWithBob()).contains(QStringLiteral("busy")),
                             networkTimeout);
    bob->setStatus(Status::Status::Online);
    QTRY_VERIFY_WITH_TIMEOUT(lastMessage(chatWithBob()).contains(QStringLiteral("online")),
                             networkTimeout);
    qtox().setStatusChangeMessages(false);
    QTRY_VERIFY(!qtox().getStatusChangeMessages());
}

void TestToxService::autoAcceptFiles()
{
    chatWithBob().setAutoAcceptFiles(true);
    QTRY_VERIFY(chatWithBob().getAutoAcceptFiles());
    const QByteArray contents = QByteArray{"Saved."}.repeated(100);
    QTemporaryFile sent;
    QVERIFY(sent.open());
    sent.write(contents);
    sent.close();
    bob->getCoreFile()->sendFile(bob->getFriendList().first(), "auto.txt", sent.fileName(),
                                 contents.size());

    const QAbstractItemModel& messages = *chatWithBob().getMessages();
    const auto last = [&](const QByteArray& role) {
        return data(messages, messages.rowCount() - 1, role);
    };
    QTRY_COMPARE_WITH_TIMEOUT(last("text").toString(), QStringLiteral("auto.txt"), networkTimeout);
    QTRY_COMPARE_WITH_TIMEOUT(last("fileStatus").toString(), QStringLiteral("finished"),
                              networkTimeout);
    QTRY_VERIFY(!last("filePath").toString().isEmpty());
    QFile received{last("filePath").toString()};
    QVERIFY(received.open(QIODevice::ReadOnly));
    QCOMPARE(received.readAll(), contents);

    chatWithBob().setAutoAcceptFiles(false);
    QTRY_VERIFY(!chatWithBob().getAutoAcceptFiles());
}

void TestToxService::pauseFile()
{
    const QByteArray contents = QByteArray{"Wait."}.repeated(100000);
    QTemporaryFile sent;
    QVERIFY(sent.open());
    sent.write(contents);
    sent.close();
    bob->getCoreFile()->sendFile(bob->getFriendList().first(), "later.txt", sent.fileName(),
                                 contents.size());
    const QAbstractItemModel& messages = *chatWithBob().getMessages();
    const auto last = [&](const QByteArray& role) {
        return data(messages, messages.rowCount() - 1, role);
    };
    QTRY_COMPARE_WITH_TIMEOUT(last("text").toString(), QStringLiteral("later.txt"), networkTimeout);
    QTRY_VERIFY(last("fileNum").isValid());

    // Before any of it arrives.
    chatWithBob().acceptFile(last("fileNum").toInt());
    chatWithBob().pauseFile(last("fileNum").toInt());
    QTRY_COMPARE(last("fileStatus").toString(), QStringLiteral("paused"));
    QTRY_VERIFY(last("filePausedByUser").toBool());
    QVERIFY(serviceCaughtUp());
    QTest::qWait(1000);
    QCOMPARE(last("fileStatus").toString(), QStringLiteral("paused"));

    chatWithBob().pauseFile(last("fileNum").toInt());
    QTRY_COMPARE_WITH_TIMEOUT(last("fileStatus").toString(), QStringLiteral("finished"),
                              networkTimeout);
    QFile received{last("filePath").toString()};
    QVERIFY(received.open(QIODevice::ReadOnly));
    QCOMPARE(received.readAll(), contents);
}

void TestToxService::replyFromNotification()
{
    const QString tag = RemoteModels::chat(ChatKind::Kind::Friend, bob->getSelfPublicKey().toString());
    QTRY_VERIFY_WITH_TIMEOUT(data(*qtox().getChats(), 0, "online").toBool(), networkTimeout);
    QVERIFY(bobSays("Still there?"));
    QTRY_COMPARE(notifier.shown.value(tag), QStringLiteral("Still there?"));
    QCOMPARE(notifier.actions.value(tag), INotifier::Actions::Reply);

    const QSignalSpy bobReceived{bob.get(), &Core::friendMessageReceived};
    alice->backend->getService().reply(tag, "Yes");
    QTRY_COMPARE_WITH_TIMEOUT(bobReceived.count(), 1, networkTimeout);
    QCOMPARE(bobReceived[0][1].toString(), QStringLiteral("Yes"));
    QVERIFY(!notifier.shown.contains(tag));
    QTRY_COMPARE(data(*qtox().getChats(), 0, "unread").toInt(), 0);
}

void TestToxService::removeFriend()
{
    const QAbstractItemModel& chats = *qtox().getChats();
    QCOMPARE(chats.rowCount(), 1);
    qtox().removeFriend(bob->getSelfPublicKey().toString(), true);
    QTRY_COMPARE(chats.rowCount(), 0);

    // Also from the save file: the service loads the friends when it logs in.
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QVERIFY(serviceCaughtUp());
    QCOMPARE(chats.rowCount(), 0);
}

void TestToxService::calls()
{
    const DhtServer bobsNode = MockBootstrapListGenerator::makeListFromSelf(*bob).first();
    Carol carol{bobsNode.publicKey.toString(), bobsNode.udpPort};
    QVERIFY(carol.isRunning());
    QVERIFY(carol.befriend(qtox().getSelfId()));
    QTRY_COMPARE_WITH_TIMEOUT(qtox().getFriendRequestCount(), 1, networkTimeout);
    qtox().acceptFriendRequest(carol.publicKey());
    QTRY_VERIFY_WITH_TIMEOUT(carol.isFriendOnline(), networkTimeout);
    const QAbstractItemModel& chats = *qtox().getChats();
    QTRY_VERIFY_WITH_TIMEOUT(data(chats, 0, "online").toBool(), networkTimeout);
    const auto callState = [this] { return static_cast<CallState::State>(qtox().getCallState()); };

    // Carol calls.
    QVERIFY(carol.call());
    QTRY_COMPARE_WITH_TIMEOUT(callState(), CallState::State::Incoming, networkTimeout);
    QCOMPARE(qtox().getCallChat(), carol.publicKey());
    const QString tag = RemoteModels::chat(ChatKind::Kind::Friend, carol.publicKey());
    QTRY_COMPARE(notifier.shown.value(tag), QStringLiteral("Incoming call"));
    QCOMPARE(notifier.actions.value(tag), INotifier::Actions::AnswerOrDecline);
    QVERIFY(!notifier.recording);
    // Ringing goes where ringtones go.
    qtox().toggleCallLoudspeaker();
    QTest::qWait(100);
    QVERIFY(!qtox().isCallLoudspeaker());
    qtox().answerCall();
    QTRY_COMPARE(callState(), CallState::State::Active);
    QVERIFY(qtox().getCallStarted().isValid());
    QVERIFY(notifier.recording);
    QVERIFY(!notifier.shown.contains(tag));
    QTRY_VERIFY_WITH_TIMEOUT((carol.callState & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A) != 0,
                             networkTimeout);
    qtox().toggleCallMic();
    QTRY_VERIFY(qtox().isCallMicMuted());
    QVERIFY(carol.hangUp());
    QTRY_COMPARE_WITH_TIMEOUT(callState(), CallState::State::None, networkTimeout);
    QVERIFY(!notifier.recording);
    QVERIFY(!qtox().isCallMicMuted());
    QmlChat& chatWithCarol = *qtox().chat(ChatKind::Kind::Friend, carol.publicKey());
    QTRY_VERIFY(lastMessage(chatWithCarol).contains(QStringLiteral("Call duration")));

    // Alice calls, with her camera.
    qtox().startCall(carol.publicKey(), true);
    QTRY_COMPARE(callState(), CallState::State::Outgoing);
    QTRY_VERIFY(qtox().callSendsVideo());
    QVERIFY(notifier.filming);
    qtox().toggleCallLoudspeaker();
    QTRY_VERIFY(qtox().isCallLoudspeaker());
    QTRY_VERIFY_WITH_TIMEOUT(carol.invited, networkTimeout);
    QVERIFY(carol.answer());
    QTRY_COMPARE_WITH_TIMEOUT(callState(), CallState::State::Active, networkTimeout);
    QVERIFY(qtox().isCallLoudspeaker());
    qtox().endCall();
    QTRY_COMPARE(callState(), CallState::State::None);
    QTRY_VERIFY(!qtox().isCallLoudspeaker());
    QTRY_VERIFY(!qtox().callSendsVideo());
    QVERIFY(!notifier.filming);
    QTRY_COMPARE_WITH_TIMEOUT(carol.callState, uint32_t{TOXAV_FRIEND_CALL_STATE_FINISHED},
                              networkTimeout);

    // Carol's video, answered without Alice's.
    QVERIFY(carol.call(true));
    QTRY_COMPARE_WITH_TIMEOUT(callState(), CallState::State::Incoming, networkTimeout);
    QTRY_VERIFY(qtox().callOffersVideo());
    QTRY_COMPARE(notifier.shown.value(tag), QStringLiteral("Incoming video call"));
    qtox().answerCall();
    QTRY_COMPARE(callState(), CallState::State::Active);
    QVERIFY(!qtox().callSendsVideo());
    QVERIFY(!notifier.filming);
    // Red, until a frame arrives.
    QVERIFY(QTest::qWaitFor(
        [&carol, this] { return carol.sendFrame(76, 85, 255) && !qtox().getCallVideo().isEmpty(); },
        networkTimeout));
    const QImage frame = qtox().callVideoFrame();
    const QColor middle = frame.pixelColor(frame.width() / 2, frame.height() / 2);
    QVERIFY2(middle.red() > 200 && middle.green() < 80 && middle.blue() < 80,
             qPrintable(middle.name()));
    qtox().endCall();
    QTRY_COMPARE(callState(), CallState::State::None);
    QTRY_VERIFY(qtox().getCallVideo().isEmpty());

    qtox().removeFriend(carol.publicKey(), true);
    QTRY_COMPARE(chats.rowCount(), 0);
}

void TestToxService::logoutStatus()
{
    QTRY_VERIFY_WITH_TIMEOUT(qtox().isOnline(), networkTimeout);
    notifier.statuses.clear();
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    QCOMPARE(notifier.statuses, QStringList{"Not logged in"});
}

void TestToxService::autoLogin()
{
    qtox().loadProfile("alice", "", true);
    QTRY_VERIFY(qtox().isLoggedIn());
    alice->restart();
    QTRY_VERIFY(qtox().isConnected());
    QVERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getSelfName(), QStringLiteral("Alice"));

    // Whoever logs out stays logged out.
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    alice->restart();
    QTRY_VERIFY(qtox().isConnected());
    QVERIFY(!qtox().isLoggedIn());

    // The service doesn't have the password.
    qtox().createProfile("dave", "secret", true);
    QTRY_VERIFY(qtox().isLoggedIn());
    alice->restart();
    QTRY_VERIFY(qtox().isConnected());
    QVERIFY(!qtox().isLoggedIn());
}

void TestToxService::profileManagement()
{
    const QSignalSpy errors{&qtox(), &QmlApp::error};
    qtox().loadProfile("carol", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getProfileName(), QStringLiteral("carol"));
    const QString selfId = qtox().getSelfId();

    qtox().setPassword("secret");
    QTRY_VERIFY(qtox().hasPassword());
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("carol", "", false);
    QTRY_COMPARE(errors.count(), 1);
    QVERIFY(!qtox().isLoggedIn());
    qtox().loadProfile("carol", "secret", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getSelfId(), selfId);
    qtox().setPassword("");
    QTRY_VERIFY(!qtox().hasPassword());

    // As a file name.
    qtox().renameProfile("carol/2");
    QTRY_COMPARE(qtox().getProfileName(), QStringLiteral("carol_2"));
    QVERIFY(!qtox().getProfiles().contains("carol"));
    QVERIFY(qtox().getProfiles().contains("carol_2"));
    QCOMPARE(errors.count(), 1);

    const QSignalSpy notices{&qtox(), &QmlApp::notice};
    // Not where the profiles are.
    const QTemporaryDir backups;
    const QString exported = backups.filePath("backup.tox");
    qtox().exportProfile(QUrl::fromLocalFile(exported));
    QTRY_COMPARE(notices.count(), 1);
    QFile saved{alice->dir.filePath("carol_2.tox")};
    QFile backup{exported};
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QVERIFY(backup.open(QIODevice::ReadOnly));
    QCOMPARE(backup.readAll(), saved.readAll());

    qtox().deleteProfile();
    QTRY_VERIFY(!qtox().isLoggedIn());
    QVERIFY(!qtox().getProfiles().contains("carol_2"));
    // Longer than Settings waits to save a change: half a second.
    QTest::qWait(1000);
    for (const QString& file : QStringList{"carol_2.tox", "carol_2.ini", "carol_2.db"}) {
        QVERIFY2(!QFile::exists(alice->dir.filePath(file)), qPrintable(file));
    }

    qtox().importProfile(QUrl::fromLocalFile(exported));
    QTRY_VERIFY(qtox().getProfiles().contains("backup"));
    qtox().loadProfile("backup", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    QCOMPARE(qtox().getSelfId(), selfId);
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());

    QCOMPARE(qtox().importedProfileName(QUrl::fromLocalFile(backups.filePath("notes.txt"))), QString{});
    QCOMPARE(errors.count(), 2);
}

void TestToxService::nospam()
{
    qtox().loadProfile("alice", "", false);
    QTRY_VERIFY(qtox().isLoggedIn());
    const QString selfId = qtox().getSelfId();
    qtox().newNospam();
    QTRY_VERIFY(qtox().getSelfId() != selfId);
    QCOMPARE(ToxId{qtox().getSelfId()}.getPublicKey(), ToxId{selfId}.getPublicKey());
    // Android ends the service without the logout that would save it.
    QTRY_COMPARE(savedToxId(), qtox().getSelfId());
    qtox().logout();
    QTRY_VERIFY(!qtox().isLoggedIn());
}

void TestToxService::onlineThroughRelay()
{
    // Without UDP, Alice reaches the network only through the relay.
    const TcpRelay relay;
    QVERIFY(relay.isRunning());
    alice->useBootstrapNode(relay.dhtId(), relay.udpPort(), relay.tcpPort());
    qtox().setUdpEnabled(false);
    QTRY_VERIFY(!qtox().isUdpEnabled());

    notifier.statuses.clear();
    qtox().loadProfile("alice", "", false);
    // Slower than over UDP: up to 78 seconds on a busy computer.
    QTRY_COMPARE_WITH_TIMEOUT(notifier.statuses, (QStringList{"Connecting…", "Online via TCP relay"}),
                              2 * networkTimeout);
}

QTEST_MAIN(TestToxService)
#include "toxservice_test.moc"
