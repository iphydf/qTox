/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/qml/avatarprovider.h"
#include "src/qml/callvideoprovider.h"
#include "src/qml/chatkind.h"
#include "src/qml/previewprovider.h"
#include "src/qml/qmlchat.h"
#include "src/qml/qrprovider.h"
#include "src/qml/remotemodels.h"
#include "src/qml/systeminsets.h"
#include "src/qml/systemnotifier.h"
#include "test/qml/testservice.h"

#include <QAbstractItemModelReplica>
#include <QAccessible>
#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QQmlExpression>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QRemoteObjectHost>
#include <QRemoteObjectNode>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>

#include <algorithm>
#include <memory>
#include <tox/tox.h>
#include <utility>

Q_IMPORT_QML_PLUGIN(qToxPlugin)

namespace {
/** Whether @p list shows its end. Its atYEnd lags behind its content height. */
bool showsEnd(const QObject& list)
{
    const qreal bottom = list.property("contentY").toReal() + list.property("height").toReal();
    const qreal end = list.property("originY").toReal() + list.property("contentHeight").toReal();
    return bottom >= end - 1;
}

// No friend has it.
const QString strangerPk = QString{"AB"}.repeated(32);
// A friend who's never online. Toxcore only takes keys whose last byte is below 128, and 32 equal
// bytes with no nospam have the checksum 0.
const QString friendPk = QString{"12"}.repeated(32);
const QString friendId = friendPk + "00000000" + "0000";
} // namespace

/**
 * @brief The pages, as main() shows them, with a Tox service behind them.
 */
class TestPages : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void followingListView();
    void systemInsets();
    void tapBeforeLogin();
    void pages();
    void tapAfterLogin();
    void backKey();
    void sendKeepsKeyboard();
    void messages();
    void accessibility();
    void formattedMessage();
    void removeFriend();
    void selfAvatar();
    void toxId();
    void newChatPreview();
    void sharedText();
    void search();
    void searchChats();
    void picturePreview();
    void about();
    void logout();

private:
    QQuickItem* currentPage() const;
    bool showing(const char* page) const;
    bool runOnStack(const QString& code);
    QQuickItem* findText(const QRegularExpression& text) const;
    bool click(const QString& text) const;
    QList<QAccessibleInterface*> accessibleNodes() const;
    bool warned();

    SystemNotifier notifier;
    SystemInsets insets;
    std::unique_ptr<TestService> service;
    QQmlApplicationEngine engine;
    QList<QQmlError> warnings;
    QObject* stack{nullptr};
};

void TestPages::initTestCase()
{
    // Such as calls to the service that it never gets.
    QTest::failOnWarning(QRegularExpression{"remoteobjects|connectionToSource"});
    // Nothing listens there.
    service = std::make_unique<TestService>(notifier, strangerPk, 1);
    QmlAppForeign::instance = service->qtox.get();
    QQuickStyle::setStyle("Material");
    // As with a screen reader on: Qt Quick's controls only name themselves for one.
    QAccessible::setActive(true);
    connect(&engine, &QQmlEngine::warnings, this,
            [this](const QList<QQmlError>& newWarnings) { warnings += newWarnings; });
    engine.addImageProvider(QStringLiteral("avatar"), new AvatarProvider{*service->qtox});
    engine.addImageProvider(QStringLiteral("qr"), new QrProvider);
    engine.addImageProvider(QStringLiteral("preview"), new PreviewProvider);
    engine.addImageProvider(QStringLiteral("callvideo"), new CallVideoProvider{*service->qtox});
    engine.loadFromModule("qTox", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);

    for (auto* item : engine.rootObjects().first()->findChildren<QQuickItem*>()) {
        if (item->inherits("QQuickStackView")) {
            stack = item;
        }
    }
    QVERIFY(stack != nullptr);
    QTRY_VERIFY(showing("LoginPage"));
    QVERIFY(!warned());
}

void TestPages::cleanup()
{
    QVERIFY(!warned());
}

/** Whether QML warned since the last call, and what about. */
bool TestPages::warned()
{
    const QList<QQmlError> newWarnings = std::exchange(warnings, {});
    for (const QQmlError& warning : newWarnings) {
        qWarning() << warning;
    }
    return !newWarnings.isEmpty();
}

QQuickItem* TestPages::currentPage() const
{
    return stack->property("currentItem").value<QQuickItem*>();
}

bool TestPages::showing(const char* page) const
{
    // QML names its types' classes after their files.
    const QQuickItem* current = currentPage();
    return current != nullptr && QByteArray{current->metaObject()->className()}.startsWith(page);
}

/** Runs @p code with the stack view's methods in scope, as if in its file. */
bool TestPages::runOnStack(const QString& code)
{
    QQmlExpression expression{qmlContext(stack), stack, code};
    expression.evaluate();
    if (expression.hasError()) {
        qWarning() << expression.error();
        return false;
    }
    return true;
}

/** The visible label, text field or control, popups included, whose text matches @p text. */
QQuickItem* TestPages::findText(const QRegularExpression& text) const
{
    QList<QQuickItem*> items{qobject_cast<QQuickWindow*>(engine.rootObjects().first())->contentItem()};
    while (!items.isEmpty()) {
        QQuickItem* item = items.takeLast();
        if (!item->isVisible()) {
            continue;
        }
        if ((item->inherits("QQuickText") || item->inherits("QQuickTextInput")
             || item->inherits("QQuickControl"))
            && text.match(item->property("text").toString()).hasMatch()) {
            return item;
        }
        items += item->childItems();
    }
    return nullptr;
}

/**
 * @brief Clicks once, when there is one, the visible label or control whose whole text is @p text.
 *
 * Not in QTRY_VERIFY(), which runs its expression again after it's true.
 */
bool TestPages::click(const QString& text) const
{
    const QRegularExpression pattern{
        QRegularExpression::anchoredPattern(QRegularExpression::escape(text))};
    QQuickItem* item = nullptr;
    if (!QTest::qWaitFor([&] { return (item = findText(pattern)) != nullptr; })) {
        return false;
    }
    // Into view, as the user scrolls.
    for (QQuickItem* parent = item->parentItem(); parent != nullptr; parent = parent->parentItem()) {
        if (parent->inherits("QQuickFlickable")) {
            const auto* content = parent->property("contentItem").value<QQuickItem*>();
            const qreal top = item->mapToItem(content, QPointF{}).y();
            const qreal end =
                std::max(0.0, parent->property("contentHeight").toReal() - parent->height());
            parent->setProperty("contentY", std::clamp(top - parent->height() / 2, 0.0, end));
            break;
        }
    }
    QTest::mouseClick(item->window(), Qt::LeftButton, {},
                      item->mapToScene(item->boundingRect().center()).toPoint());
    return true;
}

/** What a screen reader reaches in the window: the visible accessible nodes, popups included. */
QList<QAccessibleInterface*> TestPages::accessibleNodes() const
{
    QList<QAccessibleInterface*> nodes;
    QList<QAccessibleInterface*> todo{
        QAccessible::queryAccessibleInterface(qobject_cast<QWindow*>(engine.rootObjects().first()))};
    while (!todo.isEmpty()) {
        QAccessibleInterface* node = todo.takeLast();
        if (node == nullptr || node->state().invisible) {
            continue;
        }
        nodes += node;
        for (int i = 0; i < node->childCount(); ++i) {
            todo += node->child(i);
        }
    }
    return nodes;
}

void TestPages::followingListView()
{
    // Rows of a height, shared as the service shares a chat's messages.
    QStandardItemModel rows;
    rows.setItemRoleNames({{Qt::UserRole, "size"}});
    const auto addRow = [&rows] {
        auto* row = new QStandardItem;
        row->setData(40, Qt::UserRole);
        rows.appendRow(row);
    };
    for (int i = 0; i < 100; ++i) {
        addRow();
    }
    const QUrl url{QStringLiteral("local:%1/rows").arg(service->dir.path())};
    QRemoteObjectHost host{url};
    host.enableRemoting(&rows, "rows", {Qt::UserRole});
    QRemoteObjectNode node;
    node.connectToNode(url);
    const std::unique_ptr<QAbstractItemModelReplica> replica{
        node.acquireModel("rows", QtRemoteObjects::FetchRootSize)};

    QQmlComponent component{&engine};
    component.setData(R"(
        import QtQuick
        import qTox

        Window {
            property alias list: list
            property alias model: list.model

            width: 100
            height: 400
            visible: true

            FollowingListView {
                id: list

                anchors.fill: parent
                delegate: Item {
                    required property int size

                    width: 100
                    // As a message before its text arrives.
                    height: size > 0 ? size : 20
                }
            }
        }
    )",
                      QUrl{});
    const std::unique_ptr<QObject> root{component.create()};
    auto* window = qobject_cast<QQuickWindow*>(root.get());
    QVERIFY2(window != nullptr, qPrintable(component.errorString()));
    QVERIFY(QTest::qWaitForWindowExposed(window));
    root->setProperty("model", QVariant::fromValue(replica.get()));
    const QObject& list = *root->property("list").value<QObject*>();
    QTRY_COMPARE(list.property("count").toInt(), 100);
    QTRY_VERIFY(showsEnd(list));
    // Not before the user scrolls up.
    QVERIFY(!replica->hasData(replica->index(0, 0), Qt::UserRole));

    // As when the keyboard opens.
    window->setHeight(200);
    QTRY_COMPARE(list.property("height").toReal(), 200.0);
    QVERIFY(showsEnd(list));

    // A new message.
    addRow();
    QQuickItem* last = nullptr;
    QTRY_VERIFY(
        QMetaObject::invokeMethod(const_cast<QObject*>(&list), "itemAtIndex", qReturnArg(last), 100)
        && last != nullptr && last->height() == 40);
    // Long enough for the list to have laid it out.
    QTest::qWait(100);
    QVERIFY(showsEnd(list));
}

void TestPages::systemInsets()
{
    const auto& page = *qobject_cast<QQuickItem*>(stack);
    const int windowHeight = qobject_cast<QQuickWindow*>(engine.rootObjects().first())->height();

    // As with the keyboard open under the status bar.
    insets.set({0, 24, 0, 300});
    QTRY_COMPARE(page.y(), 24.0);
    QCOMPARE(page.y() + page.height(), windowHeight - 300.0);

    insets.set({});
    QTRY_COMPARE(page.height(), qreal(windowHeight));
}

void TestPages::tapBeforeLogin()
{
    service->qtox->open(RemoteModels::chat(ChatKind::Kind::Friend, strangerPk));
    service->qtox->createProfile("pages", "", false);
    QTRY_VERIFY(showing("ChatPage"));
    QCOMPARE(currentPage()->property("chatId").toString(), strangerPk);
    QCOMPARE(stack->property("depth").toInt(), 2);

    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
}

void TestPages::pages()
{
    // The ones the friends page opens without arguments.
    for (const char* page : {"AddFriendPage", "RequestsPage", "SettingsPage"}) {
        QVERIFY(runOnStack(QStringLiteral("push('qrc:/qt/qml/qTox/%1.qml')").arg(page)));
        QTRY_VERIFY2(showing(page), page);
        QVERIFY(runOnStack("pop()"));
        QTRY_VERIFY(showing("FriendsPage"));
    }
}

void TestPages::tapAfterLogin()
{
    service->qtox->open(RemoteModels::chat(ChatKind::Kind::Friend, strangerPk));
    QTRY_VERIFY(showing("ChatPage"));

    // From another page: the friends page comes back first.
    service->qtox->open(RemoteModels::friendRequests);
    QTRY_VERIFY(showing("RequestsPage"));
    QCOMPARE(stack->property("depth").toInt(), 2);
}

void TestPages::backKey()
{
    // The page tapAfterLogin() left open. Qt gives Android's back button as this key.
    QTRY_VERIFY(showing("RequestsPage"));
    auto* window = qobject_cast<QWindow*>(engine.rootObjects().first());
    // Shortcuts are the active window's.
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QTest::keyClick(window, Qt::Key_Back);
    QTRY_VERIFY(showing("FriendsPage"));
}

void TestPages::sendKeepsKeyboard()
{
    service->qtox->open(RemoteModels::chat(ChatKind::Kind::Friend, strangerPk));
    QTRY_VERIFY(showing("ChatPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QQuickItem* input = nullptr;
    QQuickItem* send = nullptr;
    for (auto* item : currentPage()->findChildren<QQuickItem*>()) {
        if (item->inherits("QQuickTextField") && item->property("placeholderText") == "Message") {
            input = item;
        } else if (item->inherits("QQuickButton") && item->property("text") == "Send") {
            send = item;
        }
    }
    QVERIFY(input != nullptr && send != nullptr);
    input->forceActiveFocus();
    input->setProperty("text", "Hello");

    // The keyboard stays while the text field has the focus. Else it closes when the button is
    // pressed, which moves the button away before it's released.
    QTest::mouseClick(send->window(), Qt::LeftButton, {},
                      send->mapToScene(send->boundingRect().center()).toPoint());
    QTRY_COMPARE(input->property("text").toString(), QString{});
    QVERIFY(input->hasActiveFocus());
}

void TestPages::messages()
{
    QCOMPARE(service->qtox->addFriend(friendId, {}), QString{});
    QTRY_COMPARE(service->qtox->getChats()->rowCount(), 1);
    service->qtox->open(RemoteModels::chat(ChatKind::Kind::Friend, friendPk));
    QTRY_VERIFY(showing("ChatPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QVERIFY(QMetaObject::invokeMethod(currentPage()->property("chat").value<QObject*>(),
                                      "sendMessage", QString{"Hello"}));

    QQuickItem* message = nullptr;
    QTRY_VERIFY((message = findText(QRegularExpression{"^Hello$"})) != nullptr);
    // A text message makes no buttons or menu: making them for each message slowed opening a chat.
    const QObject& bubble = *message->parentItem();
    for (const QObject* child : bubble.findChildren<QObject*>()) {
        QVERIFY2(!child->inherits("QQuickAbstractButton") && !child->inherits("QQuickPopup"),
                 child->metaObject()->className());
    }
    QTRY_VERIFY(findText(QRegularExpression{"^Today$"}));
    // The friend is offline.
    const QQuickItem* state = findText(QRegularExpression{"^Sending · "});
    QVERIFY(state != nullptr);
    // Inside the bubble, also when wider than the message.
    QVERIFY(state->x() >= 0);

    const QPoint point = message->mapToScene(message->boundingRect().center()).toPoint();
    QTest::mousePress(message->window(), Qt::LeftButton, {}, point);
    QQuickItem* copy = nullptr;
    QTRY_VERIFY((copy = findText(QRegularExpression{"^Copy$"})) != nullptr);
    QTest::mouseRelease(message->window(), Qt::LeftButton, {}, point);
    QTest::mouseClick(copy->window(), Qt::LeftButton, {},
                      copy->mapToScene(copy->boundingRect().center()).toPoint());
    QTRY_COMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("Hello"));

    // The chat list shows the last message.
    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QVERIFY(findText(QRegularExpression{"^Hello$"}));
}

void TestPages::accessibility()
{
    // The chat messages() left: one message, which the offline friend hasn't got.
    service->qtox->open(RemoteModels::chat(ChatKind::Kind::Friend, friendPk));
    QTRY_VERIFY(showing("ChatPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());

    // Every button and row says what it is in words, not as a symbol such as ⋮.
    const QRegularExpression word{QStringLiteral("\\p{L}")};
    // The first button or row whose name has no letters, and where it is; else nothing.
    const auto unnamed = [&] {
        for (QAccessibleInterface* node : accessibleNodes()) {
            const QString name = node->text(QAccessible::Name);
            if ((node->role() == QAccessible::Button || node->role() == QAccessible::ListItem)
                && !word.match(name).hasMatch()) {
                return QStringLiteral("\"%1\" in %2")
                    .arg(name, node->object()->parent()->metaObject()->className());
            }
        }
        return QString{};
    };
    QCOMPARE(unnamed(), QString{});

    // A message reads in one go, and a tap opens the menu that a long press opens.
    QAccessibleInterface* message = nullptr;
    QTRY_VERIFY([&] {
        for (QAccessibleInterface* node : accessibleNodes()) {
            if (node->role() == QAccessible::ListItem
                && node->text(QAccessible::Name).startsWith("You, Hello, Sending, ")) {
                message = node;
            }
        }
        return message != nullptr;
    }());
    QVERIFY(message->actionInterface() != nullptr);
    QVERIFY(message->actionInterface()->actionNames().contains(
        QAccessibleActionInterface::pressAction()));
    message->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    QTRY_VERIFY(findText(QRegularExpression{"^Copy$"}));
    QTest::keyClick(qobject_cast<QWindow*>(engine.rootObjects().first()), Qt::Key_Escape);
    QTRY_VERIFY(!findText(QRegularExpression{"^Copy$"}));

    // A chat in the list reads in one go too.
    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QCOMPARE(unnamed(), QString{});
    const QList<QAccessibleInterface*> rows = accessibleNodes();
    const auto row = std::find_if(rows.begin(), rows.end(), [](QAccessibleInterface* node) {
        return node->role() == QAccessible::ListItem
               && node->text(QAccessible::Name).contains("Offline, Hello, ");
    });
    QVERIFY(row != rows.end());

    // Which a screen reader opens: Qt offers it a press only for what says it has one.
    QVERIFY(
        (*row)->actionInterface()->actionNames().contains(QAccessibleActionInterface::pressAction()));
    (*row)->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    QTRY_VERIFY(showing("ChatPage"));
    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
}

void TestPages::formattedMessage()
{
    service->qtox->open(RemoteModels::chat(ChatKind::Kind::Friend, friendPk));
    QTRY_VERIFY(showing("ChatPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    const QString toxId = strangerPk + QStringLiteral("0000000000AB");
    QVERIFY(QMetaObject::invokeMethod(currentPage()->property("chat").value<QObject*>(),
                                      "sendMessage", QStringLiteral("*Add* tox:%1").arg(toxId)));

    // As the Widgets UI formats it.
    QQuickItem* message = nullptr;
    QTRY_VERIFY((message = findText(QRegularExpression{"^<b>\\*Add\\*</b> <a href="})) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(message, "linkActivated", QStringLiteral("tox:%1").arg(toxId)));
    QTRY_VERIFY(showing("AddFriendPage"));
    QVERIFY(findText(QRegularExpression{QRegularExpression::anchoredPattern(toxId)}) != nullptr);

    QVERIFY(runOnStack("popToIndex(0)"));
    QTRY_VERIFY(showing("FriendsPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
}

void TestPages::removeFriend()
{
    service->qtox->open(RemoteModels::chat(ChatKind::Kind::Friend, friendPk));
    QTRY_VERIFY(showing("ChatPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QVERIFY(click("⋮"));
    QVERIFY(click("Details"));
    QTRY_VERIFY(showing("FriendDetailsPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QVERIFY(findText(QRegularExpression{friendPk}));

    QVERIFY(click("Remove friend"));
    QVERIFY(click("Remove"));
    QTRY_VERIFY(showing("FriendsPage"));
    QTRY_COMPARE(service->qtox->getChats()->rowCount(), 0);
    QTRY_VERIFY(!stack->property("busy").toBool());
}

void TestPages::selfAvatar()
{
    QTemporaryFile picture;
    QVERIFY(picture.open());
    QImage image{64, 64, QImage::Format_RGB32};
    image.fill(Qt::blue);
    QVERIFY(image.save(&picture, "PNG"));
    picture.close();
    service->qtox->setAvatar(QUrl::fromLocalFile(picture.fileName()));
    QTRY_VERIFY(service->qtox->hasOwnAvatar());

    QVERIFY(runOnStack("push('qrc:/qt/qml/qTox/SettingsPage.qml')"));
    QTRY_VERIFY(showing("SettingsPage"));
    const auto shown = [this] {
        for (auto* item : currentPage()->findChildren<QQuickItem*>()) {
            if (item->inherits("QQuickImage")
                && item->property("source").toString().startsWith("image://avatar/self/")) {
                return item->property("status").toInt() == 1;
            }
        }
        return false;
    };
    QTRY_VERIFY(shown());
    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
}

void TestPages::toxId()
{
    QVERIFY(runOnStack("push('qrc:/qt/qml/qTox/ToxIdPage.qml')"));
    QTRY_VERIFY(showing("ToxIdPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    const auto qr = [this]() -> QQuickItem* {
        for (auto* item : currentPage()->findChildren<QQuickItem*>()) {
            if (item->inherits("QQuickImage")
                && item->property("source").toString().startsWith("image://qr/")) {
                return item;
            }
        }
        return nullptr;
    };
    QTRY_VERIFY(qr() != nullptr && qr()->property("status").toInt() == 1);

    // The top left finder pattern, 7 modules wide, after a blank border of 4.
    QrProvider provider;
    QSize size;
    const QImage code =
        provider.requestImage(qr()->property("source").toString().remove("image://qr/"), &size,
                              QSize{400, 400});
    QCOMPARE(size, code.size());
    int border = 0;
    while (border < code.width() && code.pixelColor(border, border) == Qt::white) {
        ++border;
    }
    const int module = border / 4;
    QVERIFY(module > 1);
    QCOMPARE(border % 4, 0);
    QCOMPARE(code.pixelColor(border + 7 * module - 1, border), QColor{Qt::black});
    QCOMPARE(code.pixelColor(border + 7 * module, border), QColor{Qt::white});

    const QString selfId = service->qtox->getSelfId();
    QVERIFY(click("New NoSpam"));
    QVERIFY(click("Change"));
    QTRY_VERIFY(service->qtox->getSelfId() != selfId);
    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
}

void TestPages::newChatPreview()
{
    QVERIFY(showing("FriendsPage"));
    const QSignalSpy created{service->qtox.get(), &QmlApp::chatAdded};
    service->qtox->createConference("Plans", {});
    QTRY_COMPARE(created.count(), 1);
    QTRY_VERIFY(findText(QRegularExpression{"^Plans$"}) != nullptr);

    // Its row came after the list showed: the preview is its own, not that of the row before.
    service->qtox->chat(ChatKind::Kind::Conference, created[0][1].toString())->sendMessage("Agenda");
    QTRY_VERIFY(findText(QRegularExpression{"Agenda"}) != nullptr);

    // A new row that moves to the top, above one with a message.
    service->qtox->createConference("Later", {});
    QTRY_COMPARE(created.count(), 2);
    QTRY_VERIFY(findText(QRegularExpression{"^Later$"}) != nullptr);
    service->qtox->chat(ChatKind::Kind::Conference, created[1][1].toString())->sendMessage("Tomorrow");
    QTRY_VERIFY(findText(QRegularExpression{"Tomorrow"}) != nullptr);
    QVERIFY(findText(QRegularExpression{"Agenda"}) != nullptr);
}

void TestPages::sharedText()
{
    QVERIFY(showing("FriendsPage"));
    service->qtox->receiveShare({}, "Look at this");
    QTRY_VERIFY(showing("SharePage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QVERIFY(click("Plans"));
    QTRY_VERIFY(showing("ChatPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QVERIFY(findText(QRegularExpression{"^Look at this$"}) != nullptr);

    // Back to the chats, not to the choice.
    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
}

void TestPages::search()
{
    QTRY_VERIFY(!stack->property("busy").toBool());
    QVERIFY(click("Plans"));
    QTRY_VERIFY(showing("ChatPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
    QVERIFY(click("⋮"));
    QVERIFY(click("Search"));
    QQuickItem* field = nullptr;
    for (auto* item : currentPage()->findChildren<QQuickItem*>()) {
        if (item->inherits("QQuickTextField") && item->property("placeholderText") == "Search") {
            field = item;
        }
    }
    QVERIFY(field != nullptr);
    QTRY_VERIFY(field->isVisible());
    field->setProperty("text", "agen");
    QVERIFY(QMetaObject::invokeMethod(field, "forceActiveFocus"));
    QTest::keyClick(field->window(), Qt::Key_Return);
    QTRY_COMPARE(currentPage()->property("foundRow").toInt(), 0);

    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
    QTRY_VERIFY(!stack->property("busy").toBool());
}

void TestPages::searchChats()
{
    QVERIFY(showing("FriendsPage"));
    QTRY_VERIFY(findText(QRegularExpression{"^Plans$"}) != nullptr);
    QTRY_VERIFY(findText(QRegularExpression{"^Later$"}) != nullptr);
    QVERIFY(click("⋮"));
    QVERIFY(click("Search"));
    service->qtox->setChatFilter("pla");
    QTRY_VERIFY(findText(QRegularExpression{"^Later$"}) == nullptr);
    QVERIFY(findText(QRegularExpression{"^Plans$"}) != nullptr);
    service->qtox->setChatFilter("nobody");
    QTRY_VERIFY(findText(QRegularExpression{"^No chats with that name.$"}) != nullptr);
    service->qtox->setChatFilter({});
    QTRY_VERIFY(findText(QRegularExpression{"^Later$"}) != nullptr);

    // Android's back button closes the search first.
    QVERIFY(currentPage()->property("searching").toBool());
    QVERIFY(QMetaObject::invokeMethod(engine.rootObjects().first(), "close"));
    QTRY_VERIFY(!currentPage()->property("searching").toBool());
    QVERIFY(showing("FriendsPage"));
}

void TestPages::picturePreview()
{
    QTemporaryDir dir;
    QImage picture{800, 600, QImage::Format_RGB32};
    picture.fill(Qt::green);
    QVERIFY(picture.save(dir.filePath("photo.jpg")));
    PreviewProvider provider;
    QSize size;
    const QImage shown = provider.requestImage(QString::fromLatin1(QUrl::toPercentEncoding(
                                                   dir.filePath("photo.jpg"))),
                                               &size, QSize{240, 240});
    QCOMPARE(shown.size(), (QSize{240, 180}));
    QCOMPARE(size, shown.size());

    // Only pictures, by their name.
    QVERIFY(QFile::copy(dir.filePath("photo.jpg"), dir.filePath("photo.txt")));
    QVERIFY(
        provider
            .requestImage(QString::fromLatin1(QUrl::toPercentEncoding(dir.filePath("photo.txt"))),
                          &size, {})
            .isNull());
}

void TestPages::about()
{
    QVERIFY(runOnStack("push('qrc:/qt/qml/qTox/AboutPage.qml')"));
    QTRY_VERIFY(showing("AboutPage"));
    QVERIFY(findText(QRegularExpression{QStringLiteral("^toxcore version: %1\\.%2\\.%3$")
                                            .arg(tox_version_major())
                                            .arg(tox_version_minor())
                                            .arg(tox_version_patch())}));
    QVERIFY(findText(QRegularExpression{QStringLiteral("^Qt version: %1$").arg(QT_VERSION_STR)}));
    QVERIFY(runOnStack("pop()"));
    QTRY_VERIFY(showing("FriendsPage"));
}

void TestPages::logout()
{
    service->qtox->logout();
    QTRY_VERIFY(showing("LoginPage"));
    QCOMPARE(stack->property("depth").toInt(), 1);
}

QTEST_MAIN(TestPages)
#include "pages_test.moc"
