/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "contentdialog.h"

#include "splitterrestorer.h"

#include "src/conferencelist.h"
#include "src/core/core.h"
#include "src/friendlist.h"
#include "src/model/chatroom/friendchatroom.h"
#include "src/model/conference.h"
#include "src/model/friend.h"
#include "src/model/status.h"
#include "src/persistence/settings.h"
#include "src/widget/conferencewidget.h"
#include "src/widget/contentlayout.h"
#include "src/widget/form/chatform.h"
#include "src/widget/friendlistlayout.h"
#include "src/widget/friendwidget.h"
#include "src/widget/style.h"
#include "src/widget/translator.h"
#include "src/widget/widget.h"

#include <QBoxLayout>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QShortcut>
#include <QSplitter>

namespace {
const int minWidget = 220;
const int minHeight = 220;
const QSize minSize(minHeight, minWidget);
const QSize defaultSize(720, 400);
} // namespace

ContentDialog::ContentDialog(const Core& core, Settings& settings_, Style& style_,
                             IMessageBoxManager& messageBoxManager_, FriendList& friendList_,
                             ConferenceList& conferenceList_, Profile& profile_, QWidget* parent)
    : ActivateDialog(style_, parent, Qt::Window)
    , splitter{new QSplitter(this)}
    , friendLayout{new FriendListLayout(this)}
    , activeChatroomWidget(nullptr)
    , videoSurfaceSize(QSize())
    , videoCount(0)
    , settings{settings_}
    , style{style_}
    , messageBoxManager{messageBoxManager_}
    , friendList{friendList_}
    , conferenceList{conferenceList_}
    , profile{profile_}
{
    friendLayout->setContentsMargins(0, 0, 0, 0);
    friendLayout->setSpacing(0);

    layouts = {friendLayout->getLayoutOnline(), conferenceLayout.getLayout(),
               friendLayout->getLayoutOffline()};

    if (settings.getConferencePosition()) {
        layouts.swapItemsAt(0, 1);
    }

    auto* friendWidget = new QWidget();
    friendWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    friendWidget->setAutoFillBackground(true);
    friendWidget->setLayout(friendLayout);

    onConferencePositionChanged(settings.getConferencePosition());

    friendScroll = new QScrollArea(this);
    friendScroll->setMinimumWidth(minWidget);
    friendScroll->setFrameStyle(QFrame::NoFrame);
    friendScroll->setLayoutDirection(Qt::RightToLeft);
    friendScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    friendScroll->setWidgetResizable(true);
    friendScroll->setWidget(friendWidget);

    auto* contentWidget = new QWidget(this);
    contentWidget->setAutoFillBackground(true);

    contentLayout = new ContentLayout(settings, style, contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    splitter->addWidget(friendScroll);
    splitter->addWidget(contentWidget);
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(1, false);

    auto* boxLayout = new QVBoxLayout(this);
    boxLayout->setContentsMargins(0, 0, 0, 0);
    boxLayout->setSpacing(0);
    boxLayout->addWidget(splitter);

    setMinimumSize(minSize);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("detached");

    const QByteArray geometry = settings.getDialogGeometry();

    if (!geometry.isNull()) {
        restoreGeometry(geometry);
    } else {
        resize(defaultSize);
    }

    SplitterRestorer restorer(splitter);
    restorer.restore(settings.getDialogSplitterState(), size());

    username = core.getUsername();

    setAcceptDrops(true);

    reloadTheme();

    new QShortcut(Qt::CTRL | Qt::Key_Q, this, this, &ContentDialog::close);
    new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab, this, this, &ContentDialog::previousChat);
    new QShortcut(Qt::CTRL | Qt::Key_Tab, this, this, &ContentDialog::nextChat);
    new QShortcut(Qt::CTRL | Qt::Key_PageUp, this, this, &ContentDialog::previousChat);
    new QShortcut(Qt::CTRL | Qt::Key_PageDown, this, this, &ContentDialog::nextChat);

    connect(&settings, &Settings::conferencePositionChanged, this,
            &ContentDialog::onConferencePositionChanged);
    connect(splitter, &QSplitter::splitterMoved, this, &ContentDialog::saveSplitterState);

    Translator::registerHandler([this] { retranslateUi(); }, this);
}

ContentDialog::~ContentDialog()
{
    Translator::unregister(this);
}

void ContentDialog::closeEvent(QCloseEvent* event)
{
    emit willClose();
    event->accept();
}

FriendWidget* ContentDialog::addFriend(std::shared_ptr<FriendChatroom> chatroom, GenericChatForm* form)
{
    const auto compact = settings.getCompactLayout();
    auto* frnd = chatroom->getFriend();
    const auto& friendPk = frnd->getPublicKey();
    auto* friendWidget =
        new FriendWidget(chatroom, compact, settings, style, messageBoxManager, profile, this);
    emit connectFriendWidget(*friendWidget);
    chatWidgets[friendPk] = friendWidget;
    friendLayout->addFriendWidget(friendWidget, frnd->getStatus());
    chatForms[friendPk] = form;

    // TODO(sudden6): move this connection to the Friend::displayedNameChanged signal
    connect(frnd, &Friend::aliasChanged, this, &ContentDialog::updateFriendWidget);
    connect(frnd, &Friend::statusMessageChanged, this, &ContentDialog::setStatusMessage);
    connect(friendWidget, &FriendWidget::chatroomWidgetClicked, this, &ContentDialog::activate);

    // FIXME: emit should be removed
    emit friendWidget->chatroomWidgetClicked(friendWidget);

    return friendWidget;
}

ConferenceWidget* ContentDialog::addConference(std::shared_ptr<ConferenceRoom> chatroom,
                                               GenericChatForm* form)
{
    auto* const g = chatroom->getConference();
    const auto& conferenceId = g->getPersistentId();
    const auto compact = settings.getCompactLayout();
    auto* conferenceWidget = new ConferenceWidget(chatroom, compact, settings, style, this);
    chatWidgets[conferenceId] = conferenceWidget;
    conferenceLayout.addSortedWidget(conferenceWidget);
    chatForms[conferenceId] = form;

    connect(conferenceWidget, &ConferenceWidget::chatroomWidgetClicked, this, &ContentDialog::activate);

    // FIXME: emit should be removed
    emit conferenceWidget->chatroomWidgetClicked(conferenceWidget);

    return conferenceWidget;
}

void ContentDialog::removeFriend(const ToxPk& friendPk)
{
    auto* chatroomWidget = qobject_cast<FriendWidget*>(chatWidgets[friendPk]);
    disconnect(chatroomWidget->getFriend(), &Friend::aliasChanged, this,
               &ContentDialog::updateFriendWidget);

    // Need to find replacement to show here instead.
    if (activeChatroomWidget == chatroomWidget) {
        cycleChats(/* forward = */ true, /* inverse = */ false);
    }

    friendLayout->removeFriendWidget(chatroomWidget, Status::Status::Offline);
    friendLayout->removeFriendWidget(chatroomWidget, Status::Status::Online);

    chatroomWidget->deleteLater();

    if (chatroomCount() == 0) {
        contentLayout->clear();
        activeChatroomWidget = nullptr;
        deleteLater();
    } else {
        update();
    }

    chatWidgets.remove(friendPk);
    chatForms.remove(friendPk);
    closeIfEmpty();
}

void ContentDialog::removeConference(const ConferenceId& conferenceId)
{
    auto* chatroomWidget = qobject_cast<ConferenceWidget*>(chatWidgets[conferenceId]);
    // Need to find replacement to show here instead.
    if (activeChatroomWidget == chatroomWidget) {
        cycleChats(true, false);
    }

    conferenceLayout.removeSortedWidget(chatroomWidget);
    chatroomWidget->deleteLater();

    if (chatroomCount() == 0) {
        contentLayout->clear();
        activeChatroomWidget = nullptr;
        deleteLater();
    } else {
        update();
    }

    chatWidgets.remove(conferenceId);
    chatForms.remove(conferenceId);
    closeIfEmpty();
}

void ContentDialog::closeIfEmpty()
{
    if (chatWidgets.isEmpty()) {
        close();
    }
}

int ContentDialog::chatroomCount() const
{
    return friendLayout->friendTotalCount() + conferenceLayout.getLayout()->count();
}

void ContentDialog::ensureSplitterVisible()
{
    if (splitter->sizes().at(0) == 0) {
        splitter->setSizes({1, 1});
    }

    update();
}

/**
 * @brief Get current layout and index of current widget in it.
 * Current layout -- layout contains activated widget.
 *
 * @param[out] layout Current layout
 * @return Index of current widget in current layout.
 */
int ContentDialog::getCurrentLayout(QLayout*& layout)
{
    layout = friendLayout->getLayoutOnline();
    int index = friendLayout->indexOfFriendWidget(activeChatroomWidget, true);
    if (index != -1) {
        return index;
    }

    layout = friendLayout->getLayoutOffline();
    index = friendLayout->indexOfFriendWidget(activeChatroomWidget, false);
    if (index != -1) {
        return index;
    }

    layout = conferenceLayout.getLayout();
    index = conferenceLayout.indexOfSortedWidget(activeChatroomWidget);
    if (index != -1) {
        return index;
    }

    layout = nullptr;
    return -1;
}

/**
 * @brief Activate next/previous chat.
 * @param forward If true, activate next contact, previous otherwise.
 * @param inverse ??? TODO: Add docs.
 */
void ContentDialog::cycleChats(bool forward, bool inverse)
{
    QLayout* currentLayout;
    int index = getCurrentLayout(currentLayout);
    if ((currentLayout == nullptr) || index == -1) {
        return;
    }

    if (!inverse && index == currentLayout->count() - 1) {
        const bool conferencesOnTop = settings.getConferencePosition();
        const bool offlineEmpty = friendLayout->getLayoutOffline()->isEmpty();
        const bool onlineEmpty = friendLayout->getLayoutOnline()->isEmpty();
        const bool conferencesEmpty = conferenceLayout.getLayout()->isEmpty();
        const bool isCurOffline = currentLayout == friendLayout->getLayoutOffline();
        const bool isCurOnline = currentLayout == friendLayout->getLayoutOnline();
        const bool isCurConference = currentLayout == conferenceLayout.getLayout();
        const bool nextIsEmpty =
            (isCurOnline && offlineEmpty && (conferencesEmpty || conferencesOnTop))
            || (isCurConference && offlineEmpty && (onlineEmpty || !conferencesOnTop))
            || (isCurOffline);

        if (nextIsEmpty) {
            forward = !forward;
        }
    }

    index += forward ? 1 : -1;
    // If goes out of the layout, then go to the next and skip empty. This loop goes more
    // then 1 time, because for empty layout index will be out of interval (0 < 0 || 0 >= 0)
    while (index < 0 || index >= currentLayout->count()) {
        const int oldCount = currentLayout->count();
        currentLayout = nextLayout(currentLayout, forward);
        if (currentLayout == nullptr) {
            qFatal("Layouts changed during iteration");
        }
        const int newCount = currentLayout->count();
        if (index < 0) {
            index = newCount - 1;
        } else if (index >= oldCount) {
            index = 0;
        }
    }

    QWidget* widget = currentLayout->itemAt(index)->widget();
    auto* chatWidget = qobject_cast<GenericChatroomWidget*>(widget);
    if ((chatWidget != nullptr) && chatWidget != activeChatroomWidget) {
        // FIXME: emit should be removed
        emit chatWidget->chatroomWidgetClicked(chatWidget);
    }
}

void ContentDialog::onVideoShow(QSize size)
{
    ++videoCount;
    if (videoCount > 1) {
        return;
    }

    videoSurfaceSize = size;
    const QSize minSize_ = minimumSize();
    setMinimumSize(minSize_ + videoSurfaceSize);
}

void ContentDialog::onVideoHide()
{
    videoCount--;
    if (videoCount > 0) {
        return;
    }

    const QSize minSize_ = minimumSize();
    setMinimumSize(minSize_ - videoSurfaceSize);
    videoSurfaceSize = QSize();
}


/**
 * @brief Update window title and icon.
 */
void ContentDialog::updateTitleAndStatusIcon()
{
    if (activeChatroomWidget == nullptr) {
        setWindowTitle(username);
        return;
    }

    setWindowTitle(username + QStringLiteral(" - ") + activeChatroomWidget->getTitle());

    const bool isConference = activeChatroomWidget->getConference() != nullptr;
    if (isConference) {
        setWindowIcon(QIcon(":/img/conference.svg"));
        return;
    }

    const Status::Status currentStatus = activeChatroomWidget->getFriend()->getStatus();
    setWindowIcon(QIcon{Status::getIconPath(currentStatus)});
}

/**
 * @brief Update layouts order according to settings.
 * @param conferenceOnTop If true move conference layout on the top. Move under online otherwise.
 */
void ContentDialog::reorderLayouts(bool newConferenceOnTop)
{
    const bool oldConferenceOnTop = layouts.first() == conferenceLayout.getLayout();
    if (newConferenceOnTop != oldConferenceOnTop) {
        layouts.swapItemsAt(0, 1);
    }
}

void ContentDialog::previousChat()
{
    cycleChats(false);
}

/**
 * @brief Enable next chat.
 */
void ContentDialog::nextChat()
{
    cycleChats(true);
}

/**
 * @brief Update username to show in the title.
 * @param newName New name to display.
 */
void ContentDialog::setUsername(const QString& newName)
{
    username = newName;
    updateTitleAndStatusIcon();
}

void ContentDialog::reloadTheme()
{
    setStyleSheet(style.getStylesheet("contentDialog/contentDialog.qss", settings));
    friendScroll->setStyleSheet(style.getStylesheet("friendList/friendList.qss", settings));
}

bool ContentDialog::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::WindowActivate:
        if (activeChatroomWidget != nullptr) {
            activeChatroomWidget->resetEventFlags();
            activeChatroomWidget->updateStatusLight();

            updateTitleAndStatusIcon();

            const Friend* frnd = activeChatroomWidget->getFriend();
            Conference* conference = activeChatroomWidget->getConference();

            if (frnd != nullptr) {
                emit friendDialogShown(frnd);
            } else if (conference != nullptr) {
                emit conferenceDialogShown(conference);
            }
        }

        emit activated();
    default:
        break;
    }

    return ActivateDialog::event(event);
}

void ContentDialog::dragEnterEvent(QDragEnterEvent* event)
{
    QObject* o = event->source();
    auto* frnd = qobject_cast<FriendWidget*>(o);
    auto* conference = qobject_cast<ConferenceWidget*>(o);
    if (frnd != nullptr) {
        assert(event->mimeData()->hasFormat("toxPk"));
        const ToxPk toxPk{event->mimeData()->data("toxPk")};
        Friend* contact = friendList.findFriend(toxPk);
        if (contact == nullptr) {
            return;
        }

        const ToxPk friendId = contact->getPublicKey();

        // If friend is already in a dialog then you can't drop friend where it already is.
        if (!hasChat(friendId)) {
            event->acceptProposedAction();
        }
    } else if (conference != nullptr) {
        assert(event->mimeData()->hasFormat("conferenceId"));
        const ConferenceId conferenceId = ConferenceId{event->mimeData()->data("conferenceId")};
        Conference* contact = conferenceList.findConference(conferenceId);
        if (contact == nullptr) {
            return;
        }

        if (!hasChat(conferenceId)) {
            event->acceptProposedAction();
        }
    }
}

void ContentDialog::dropEvent(QDropEvent* event)
{
    QObject* o = event->source();
    auto* frnd = qobject_cast<FriendWidget*>(o);
    auto* conference = qobject_cast<ConferenceWidget*>(o);
    if (frnd != nullptr) {
        assert(event->mimeData()->hasFormat("toxPk"));
        const ToxPk toxId(event->mimeData()->data("toxPk"));
        Friend* contact = friendList.findFriend(toxId);
        if (contact == nullptr) {
            return;
        }

        emit addFriendDialog(contact, this);
        ensureSplitterVisible();
    } else if (conference != nullptr) {
        assert(event->mimeData()->hasFormat("conferenceId"));
        const ConferenceId conferenceId(event->mimeData()->data("conferenceId"));
        Conference* contact = conferenceList.findConference(conferenceId);
        if (contact == nullptr) {
            return;
        }

        emit addConferenceDialog(contact, this);
        ensureSplitterVisible();
    }
}

void ContentDialog::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange) {
        if (isActiveWindow()) {
            emit activated();
        }
    }
}

void ContentDialog::resizeEvent(QResizeEvent* event)
{
    saveDialogGeometry();
    QDialog::resizeEvent(event);
}

void ContentDialog::moveEvent(QMoveEvent* event)
{
    saveDialogGeometry();
    QDialog::moveEvent(event);
}

void ContentDialog::keyPressEvent(QKeyEvent* event)
{
    // Ignore escape keyboard shortcut.
    if (event->key() != Qt::Key_Escape) {
        QDialog::keyPressEvent(event);
    }
}

void ContentDialog::focusChat(const ChatId& chatId)
{
    focusCommon(chatId, chatWidgets);
}

void ContentDialog::focusCommon(const ChatId& id,
                                QHash<std::reference_wrapper<const ChatId>, GenericChatroomWidget*> list)
{
    auto it = list.find(id);
    if (it == list.end()) {
        return;
    }

    activate(*it);
}

/**
 * @brief Show ContentDialog, activate chatroom widget.
 * @param widget Widget which should be activated.
 */
void ContentDialog::activate(GenericChatroomWidget* widget)
{
    // If we clicked on the currently active widget, don't reload and relayout everything
    if (activeChatroomWidget == widget) {
        return;
    }

    contentLayout->clear();

    if (activeChatroomWidget != nullptr) {
        activeChatroomWidget->setAsInactiveChatroom();
    }

    activeChatroomWidget = widget;
    const Chat* chat = widget->getChat();
    chatForms[chat->getPersistentId()]->show(contentLayout);

    widget->setAsActiveChatroom();
    widget->resetEventFlags();
    widget->updateStatusLight();
    updateTitleAndStatusIcon();
}

void ContentDialog::updateFriendStatus(const ToxPk& friendPk, Status::Status status)
{
    auto* widget = qobject_cast<FriendWidget*>(chatWidgets.value(friendPk));
    addFriendWidget(widget, status);
}

void ContentDialog::updateChatStatusLight(const ChatId& chatId)
{
    auto* widget = chatWidgets.value(chatId);
    if (widget != nullptr) {
        widget->updateStatusLight();
    }
}

bool ContentDialog::isChatActive(const ChatId& chatId) const
{
    auto* widget = chatWidgets.value(chatId);
    if (widget == nullptr) {
        return false;
    }

    return widget->isActive();
}

// TODO: Connect to widget directly
void ContentDialog::setStatusMessage(const ToxPk& friendPk, const QString& message)
{
    auto* widget = chatWidgets.value(friendPk);
    if (widget != nullptr) {
        widget->setStatusMsg(message);
    }
}

/**
 * @brief Update friend widget name and position.
 * @param friendId Friend Id.
 * @param alias Alias to display on widget.
 */
void ContentDialog::updateFriendWidget(const ToxPk& friendPk, QString alias)
{
    std::ignore = alias;
    Friend* f = friendList.findFriend(friendPk);
    auto* friendWidget = qobject_cast<FriendWidget*>(chatWidgets[friendPk]);

    const Status::Status status = f->getStatus();
    friendLayout->addFriendWidget(friendWidget, status);
}

/**
 * @brief Handler of `conferencePositionChanged` action.
 * Move conference layout on the top or on the bottom.
 *
 * @param top If true, move conference layout on the top, false otherwise.
 */
void ContentDialog::onConferencePositionChanged(bool top)
{
    friendLayout->removeItem(conferenceLayout.getLayout());
    friendLayout->insertLayout(top ? 0 : 1, conferenceLayout.getLayout());
}

/**
 * @brief Retranslate all elements in the form.
 */
void ContentDialog::retranslateUi()
{
    updateTitleAndStatusIcon();
}

/**
 * @brief Save size of dialog window.
 */
void ContentDialog::saveDialogGeometry()
{
    settings.setDialogGeometry(saveGeometry());
}

/**
 * @brief Save state of splitter between dialog and dialog list.
 */
void ContentDialog::saveSplitterState()
{
    settings.setDialogSplitterState(splitter->saveState());
}

bool ContentDialog::hasChat(const ChatId& chatId) const
{
    return chatWidgets.contains(chatId);
}

/**
 * @brief Find the next or previous layout in layout list.
 * @param layout Current layout.
 * @param forward If true, move forward, backward otherwise.
 * @return Next/previous layout.
 */
QLayout* ContentDialog::nextLayout(QLayout* layout, bool forward) const
{
    const int index = layouts.indexOf(layout);
    if (index == -1) {
        return nullptr;
    }

    int next = forward ? index + 1 : index - 1;
    const size_t size = layouts.size();
    next = (next + size) % size;

    return layouts[next];
}

void ContentDialog::addFriendWidget(FriendWidget* widget, Status::Status status)
{
    friendLayout->addFriendWidget(widget, status);
}

bool ContentDialog::isActiveWidget(GenericChatroomWidget* widget)
{
    return activeChatroomWidget == widget;
}
