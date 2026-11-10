/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "chatform.h"

#include "src/chatlog/chatmessage.h"
#include "src/chatlog/chatwidget.h"
#include "src/chatlog/content/filetransferwidget.h"
#include "src/chatlog/content/text.h"
#include "src/core/core.h"
#include "src/core/coreav.h"
#include "src/core/corefile.h"
#include "src/model/callduration.h"
#include "src/model/callmanager.h"
#include "src/model/chatroom/friendchatroom.h"
#include "src/model/filemanager.h"
#include "src/model/friend.h"
#include "src/model/status.h"
#include "src/nexus.h"
#include "src/persistence/history.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"
#include "src/platform/screenshot.h"
#include "src/translator.h"
#include "src/widget/chatformheader.h"
#include "src/widget/contentdialogmanager.h"
#include "src/widget/form/loadhistorydialog.h"
#include "src/widget/imagepreviewwidget.h"
#include "src/widget/searchform.h"
#include "src/widget/style.h"
#include "src/widget/tool/abstractscreenshotgrabber.h"
#include "src/widget/tool/callconfirmwidget.h"
#include "src/widget/tool/chattextedit.h"
#include "src/widget/tool/croppinglabel.h"
#include "src/widget/tool/screenshotgrabber.h"
#include "src/widget/video/netcamview.h"
#include "src/widget/widget.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStringBuilder>
#include <QTemporaryDir>

#include <cassert>
#include <memory>

/**
 * @brief ChatForm::incomingNotification Notify that we are called by someone.
 * @param friendId Friend that is calling us.
 */
namespace {
constexpr int CHAT_WIDGET_MIN_HEIGHT = 50;
constexpr int SCREENSHOT_GRABBER_OPENING_DELAY = 500;
} // namespace

ChatForm::ChatForm(Profile& profile_, Friend* chatFriend, IChatLog& chatLog_,
                   IMessageDispatcher& messageDispatcher_, DocumentCache& documentCache_,
                   SmileyPack& smileyPack_, CameraSource& cameraSource_, Settings& settings_,
                   Style& style_, IMessageBoxManager& messageBoxManager,
                   ContentDialogManager& contentDialogManager_, FriendList& friendList_,
                   ConferenceList& conferenceList_, GroupList& groupList_, CallManager& callManager_,
                   FileManager& fileManager_, FriendChatroom& chatroom_, QWidget* parent_)
    : GenericChatForm(profile_.getCore(), chatFriend, chatLog_, messageDispatcher_, documentCache_,
                      smileyPack_, settings_, style_, messageBoxManager, friendList_,
                      conferenceList_, groupList_, parent_)
    , core{profile_.getCore()}
    , f(chatFriend)
    , cameraSource{cameraSource_}
    , settings{settings_}
    , style{style_}
    , contentDialogManager{contentDialogManager_}
    , profile{profile_}
    , callManager{callManager_}
    , fileManager{fileManager_}
    , chatroom{chatroom_}
{
    setName(f->getDisplayedName());

    headWidget->setAvatar(QPixmap(":/img/contact_dark.svg"));

    statusMessageLabel = new CroppingLabel();
    statusMessageLabel->setObjectName("statusLabel");
    statusMessageLabel->setFont(Style::getFont(Style::Font::Medium));
    statusMessageLabel->setMinimumHeight(Style::getFont(Style::Font::Medium).pixelSize());
    statusMessageLabel->setTextFormat(Qt::PlainText);
    statusMessageLabel->setContextMenuPolicy(Qt::CustomContextMenu);

    callDurationTimer = nullptr;

    chatWidget->setMinimumHeight(CHAT_WIDGET_MIN_HEIGHT);

    callDuration = new QLabel();
    headWidget->addWidget(statusMessageLabel);
    headWidget->addStretch();
    headWidget->addWidget(callDuration, 1, Qt::AlignCenter);
    callDuration->hide();

    imagePreview = new ImagePreviewButton(this);
    imagePreview->setFixedSize(100, 100);
    imagePreview->setFlat(true);
    imagePreview->setStyleSheet("QPushButton { border: 0px }");
    imagePreview->hide();

    auto cancelIcon = QIcon(style.getImagePath("rejectCall/rejectCall.svg", settings));
    auto* cancelButton = new QPushButton(imagePreview);
    cancelButton->setFixedSize(20, 20);
    cancelButton->move(QPoint(80, 0));
    cancelButton->setIcon(cancelIcon);
    cancelButton->setFlat(true);

    connect(cancelButton, &QPushButton::pressed, this, &ChatForm::cancelImagePreview);

    contentLayout->insertWidget(3, imagePreview);

    copyStatusAction = statusMessageMenu.addAction(QString(), this, &ChatForm::onCopyStatusMessage);

    const CoreFile* coreFile = core.getCoreFile();
    connect(&profile, &Profile::friendAvatarChanged, this, &ChatForm::onAvatarChanged);
    connect(coreFile, &CoreFile::fileReceiveRequested, this, &ChatForm::updateFriendActivityForFile);
    connect(coreFile, &CoreFile::fileSendStarted, this, &ChatForm::updateFriendActivityForFile);
    connect(&core, &Core::friendTypingChanged, this, &ChatForm::onFriendTypingChanged);
    connect(coreFile, &CoreFile::fileNameChanged, this, &ChatForm::onFileNameChanged);

    connect(chatFriend, &Friend::statusChanged, this, &ChatForm::onFriendStatusChanged);

    connect(&callManager, &CallManager::stateChanged, this, &ChatForm::onCallStateChanged);

    connect(headWidget, &ChatFormHeader::callTriggered, this, &ChatForm::onCallTriggered);
    connect(headWidget, &ChatFormHeader::videoCallTriggered, this, &ChatForm::onVideoCallTriggered);
    connect(headWidget, &ChatFormHeader::micMuteToggle, this, &ChatForm::onMicMuteToggle);
    connect(headWidget, &ChatFormHeader::volMuteToggle, this, &ChatForm::onVolMuteToggle);
    connect(sendButton, &QPushButton::pressed, this, &ChatForm::callUpdateFriendActivity);
    connect(sendButton, &QPushButton::pressed, this, &ChatForm::sendImageFromPreview);
    connect(msgEdit, &ChatTextEdit::enterPressed, this, &ChatForm::callUpdateFriendActivity);
    connect(msgEdit, &ChatTextEdit::textChanged, this, &ChatForm::onTextEditChanged);
    connect(msgEdit, &ChatTextEdit::pasteImage, this, &ChatForm::previewImage);
    connect(msgEdit, &ChatTextEdit::enterPressed, this, &ChatForm::sendImageFromPreview);
    connect(msgEdit, &ChatTextEdit::escapePressed, this, &ChatForm::cancelImagePreview);
    connect(statusMessageLabel, &CroppingLabel::customContextMenuRequested, this,
            [&](const QPoint& pos) {
                if (!statusMessageLabel->text().isEmpty()) {
                    auto* sender_ = static_cast<QWidget*>(sender());
                    statusMessageMenu.exec(sender_->mapToGlobal(pos));
                }
            });

    // reflect name changes in the header
    connect(headWidget, &ChatFormHeader::nameChanged, this,
            [this](const QString& newName) { f->setAlias(newName); });
    connect(headWidget, &ChatFormHeader::callAccepted, this,
            [this] { onAnswerCallTriggered(callManager.hasVideo(f->getPublicKey())); });
    connect(headWidget, &ChatFormHeader::callRejected, this, &ChatForm::onRejectCallTriggered);

    connect(bodySplitter, &QSplitter::splitterMoved, this, &ChatForm::onSplitterMoved);

    updateCallButtons();

    setAcceptDrops(true);
    retranslateUi();
    Translator::registerHandler([this] { retranslateUi(); }, this);
}

ChatForm::~ChatForm()
{
    Translator::unregister(this);
}

void ChatForm::setStatusMessage(const QString& newMessage)
{
    statusMessageLabel->setText(newMessage);
    // for long messages
    statusMessageLabel->setToolTip(Qt::convertFromPlainText(newMessage, Qt::WhiteSpaceNormal));
}

void ChatForm::callUpdateFriendActivity()
{
    emit updateFriendActivity(*f);
}

void ChatForm::updateFriendActivityForFile(const ToxFile& file)
{
    if (file.friendId != f->getId()) {
        return;
    }
    emit updateFriendActivity(*f);
}

void ChatForm::onFileNameChanged(const ToxPk& friendPk)
{
    if (friendPk != f->getPublicKey()) {
        return;
    }

    QMessageBox::warning(this, tr("Filename contained illegal characters"),
                         tr("Illegal characters have been changed to _\n"
                            "so you can save the file on Windows."));
}

void ChatForm::onTextEditChanged()
{
    chatroom.setDraftHasText(!msgEdit->toPlainText().isEmpty());
}

void ChatForm::onAttachClicked()
{
    const QStringList paths = QFileDialog::getOpenFileNames(Q_NULLPTR, tr("Send a file"),
                                                            QDir::homePath(), QString(), nullptr);

    if (paths.isEmpty()) {
        return;
    }

    for (const QString& path : paths) {
#ifdef Q_OS_WASM
        // Send a copy, because only the main thread can read browser files.
        const QString fileName = QFileInfo(path).fileName();
        QTemporaryDir dir{QDir::temp().filePath(QStringLiteral("XXXXXX"))};
        const QString copy = dir.filePath(fileName);
        if (!QFile::copy(path, copy)) {
            QMessageBox::warning(this, tr("Unable to open"),
                                 tr("qTox wasn't able to open %1").arg(fileName));
            continue;
        }
        // Keep the copy, because the chat shows a preview of it.
        dir.setAutoRemove(false);
        sendFile(copy);
#else
        sendFile(path);
#endif
    }
}

void ChatForm::sendFile(const QString& path)
{
    switch (fileManager.send(f->getPublicKey(), path)) {
    case FileManager::SendResult::Sent:
        break;
    case FileManager::SendResult::CannotOpen:
        QMessageBox::warning(this, tr("Unable to open"),
                             tr("qTox wasn't able to open %1").arg(QFileInfo(path).fileName()));
        break;
    case FileManager::SendResult::Sequential:
        QMessageBox::critical(this, tr("Bad idea"),
                              tr("You're trying to send a sequential file, "
                                 "which is not going to work!"));
        break;
    }
}

void ChatForm::onCallStateChanged(const ToxPk& friendPk)
{
    if (friendPk != f->getPublicKey()) {
        return;
    }

    const uint32_t friendId = f->getId();
    switch (callManager.state(friendPk)) {
    case CallManager::State::Incoming:
        headWidget->createCallConfirm(callManager.hasVideo(friendPk));
        headWidget->showCallConfirm();
        emit incomingNotification(friendId);
        break;
    case CallManager::State::Outgoing:
        headWidget->showOutgoingCall(callManager.hasVideo(friendPk));
        emit updateFriendActivity(*f);
        break;
    case CallManager::State::Active:
        if (callManager.hasVideo(friendPk)) {
            showNetcam();
        } else {
            hideNetcam();
        }
        emit startCallNotification(friendId);
        updateCallButtons();
        startCounter();
        break;
    case CallManager::State::None:
        headWidget->removeCallConfirm();
        // Fixes an macOS bug with ending a call while in full screen
        if (netcam && netcam->isFullScreen()) {
            netcam->showNormal();
        }
        emit endCallNotification(friendId);
        updateCallButtons();
        deleteCounter();
        hideNetcam();
        break;
    }
}

void ChatForm::onAnswerCallTriggered(bool video)
{
    headWidget->removeCallConfirm();
    callManager.answer(f->getPublicKey(), video);
}

void ChatForm::onRejectCallTriggered()
{
    headWidget->removeCallConfirm();
    callManager.end(f->getPublicKey());
}

void ChatForm::onCallTriggered()
{
    const ToxPk& friendPk = f->getPublicKey();
    if (callManager.state(friendPk) != CallManager::State::None) {
        callManager.end(friendPk);
    } else {
        callManager.start(friendPk, false);
    }
}

void ChatForm::onVideoCallTriggered()
{
    const ToxPk& friendPk = f->getPublicKey();
    if (callManager.state(friendPk) != CallManager::State::None) {
        // TODO: We want to activate video on the active call.
        if (core.getAv()->isCallVideoEnabled(f)) {
            callManager.end(friendPk);
        }
    } else {
        callManager.start(friendPk, true);
    }
}

void ChatForm::updateCallButtons()
{
    CoreAV* av = core.getAv();
    const bool audio = av->isCallActive(f);
    const bool video = av->isCallVideoEnabled(f);
    const bool online = Status::isOnline(f->getStatus());
    headWidget->updateCallButtons(online, audio, video);
    updateMuteMicButton();
    updateMuteVolButton();
}

void ChatForm::onMicMuteToggle()
{
    CoreAV* av = core.getAv();
    av->toggleMuteCallInput(f);
    updateMuteMicButton();
}

void ChatForm::onVolMuteToggle()
{
    CoreAV* av = core.getAv();
    av->toggleMuteCallOutput(f);
    updateMuteVolButton();
}

void ChatForm::onFriendStatusChanged(const ToxPk& friendPk, Status::Status status)
{
    // Disable call buttons if friend is offline
    assert(friendPk == f->getPublicKey());
    std::ignore = friendPk;

    if (!Status::isOnline(f->getStatus())) {
        // Hide the "is typing" message when a friend goes offline
        setFriendTyping(false);
    }

    updateCallButtons();

    std::ignore = status;
}

void ChatForm::onFriendTypingChanged(quint32 friendId, bool isTyping_)
{
    if (friendId == f->getId()) {
        setFriendTyping(isTyping_);
    }
}

void ChatForm::onFriendNameChanged(const QString& name)
{
    if (sender() == f) {
        setName(name);
    }
}

void ChatForm::onStatusMessage(const QString& message)
{
    if (sender() == f) {
        setStatusMessage(message);
    }
}

void ChatForm::onAvatarChanged(const ToxPk& friendPk, const QImage& pic)
{
    if (friendPk != f->getPublicKey()) {
        return;
    }

    headWidget->setAvatar(QPixmap::fromImage(pic));
}

std::unique_ptr<NetCamView> ChatForm::createNetcam()
{
    qDebug() << "creating netcam";
    const uint32_t friendId = f->getId();
    std::unique_ptr<NetCamView> view =
        std::make_unique<NetCamView>(f->getPublicKey(), cameraSource, settings, style, profile, this);
    CoreAV* av = core.getAv();
    callVideo = av->getVideoSourceFromCall(friendId);
    view->show(callVideo.get(), f->getDisplayedName());
    connect(view.get(), &NetCamView::videoCallEnd, this, &ChatForm::onVideoCallTriggered);
    connect(view.get(), &NetCamView::volMuteToggle, this, &ChatForm::onVolMuteToggle);
    connect(view.get(), &NetCamView::micMuteToggle, this, &ChatForm::onMicMuteToggle);
    return view;
}

void ChatForm::dragEnterEvent(QDragEnterEvent* ev)
{
    if (ev->mimeData()->hasUrls()) {
        ev->acceptProposedAction();
    }
}

void ChatForm::dropEvent(QDropEvent* ev)
{
    if (!ev->mimeData()->hasUrls()) {
        return;
    }

    for (const QUrl& url : ev->mimeData()->urls()) {
        QFileInfo info(url.path());

        const QString urlString = url.toString();
        if (url.isValid() && !url.isLocalFile()
            && urlString.length() < static_cast<int>(tox_max_message_length())) {
            messageDispatcher.sendMessage(false, urlString);

            continue;
        }

        // The URL's path as it is, else as a local file's.
        if (!info.exists()) {
            info.setFile(url.toLocalFile());
        }
        sendFile(info.absoluteFilePath());
    }
}

void ChatForm::onScreenshotClicked()
{
    doScreenshot();
    // Give the window manager a moment to open the full-screen grabber window
    QTimer::singleShot(SCREENSHOT_GRABBER_OPENING_DELAY, this, &ChatForm::hideFileMenu);
}

void ChatForm::doScreenshot()
{
    // Note: grabber is self-managed and will destroy itself when done.
    AbstractScreenshotGrabber* grabber = Platform::createScreenshotGrabber(this);
    if (grabber == nullptr) {
        grabber = new ScreenshotGrabber(this);
    }
    connect(grabber, &AbstractScreenshotGrabber::screenshotTaken, this, &ChatForm::previewImage);
    grabber->showGrabber();
}

void ChatForm::previewImage(const QPixmap& pixmap)
{
    imagePreviewSource = pixmap;
    imagePreview->setIconFromPixmap(pixmap);
    imagePreview->show();
}

void ChatForm::cancelImagePreview()
{
    imagePreviewSource = QPixmap();
    imagePreview->hide();
}

void ChatForm::sendImageFromPreview()
{
    if (!imagePreview->isVisible()) {
        return;
    }

    QDir(settings.getPaths().getAppDataDirPath()).mkpath("images");

    // use ~ISO 8601 for screenshot timestamp, considering FS limitations
    // https://en.wikipedia.org/wiki/ISO_8601
    // Windows has to be supported, thus filename can't have `:` in it :/
    // Format should be: `qTox_Screenshot_yyyy-MM-dd HH-mm-ss.zzz.png`
    const QString filepath =
        QString("%1images%2qTox_Image_%3.png")
            .arg(settings.getPaths().getAppDataDirPath())
            .arg(QDir::separator())
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss.zzz"));
    QFile file(filepath);

    if (file.open(QFile::ReadWrite)) {
        imagePreviewSource.save(&file, "PNG");
        imagePreview->hide();
        imagePreviewSource = QPixmap();
        file.close();
        sendFile(QFileInfo{file}.filePath());
    } else {
        QMessageBox::warning(this,
                             tr("Failed to open temporary file", "Temporary file for screenshot"),
                             tr("qTox wasn't able to save the screenshot"));
    }
}

void ChatForm::onCopyStatusMessage()
{
    // make sure to copy not truncated text directly from the friend
    const QString text = f->getStatusMessage();
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard != nullptr) {
        clipboard->setText(text, QClipboard::Clipboard);
    }
}

void ChatForm::updateMuteMicButton()
{
    const CoreAV* av = core.getAv();
    const bool active = av->isCallActive(f);
    const bool inputMuted = av->isCallInputMuted(f);
    headWidget->updateMuteMicButton(active, inputMuted);
    if (netcam) {
        netcam->updateMuteMicButton(inputMuted);
    }
}

void ChatForm::updateMuteVolButton()
{
    const CoreAV* av = core.getAv();
    const bool active = av->isCallActive(f);
    const bool outputMuted = av->isCallOutputMuted(f);
    headWidget->updateMuteVolButton(active, outputMuted);
    if (netcam) {
        netcam->updateMuteVolButton(outputMuted);
    }
}

void ChatForm::startCounter()
{
    if (callDurationTimer != nullptr) {
        return;
    }
    callDurationTimer = new QTimer();
    connect(callDurationTimer, &QTimer::timeout, this, &ChatForm::onUpdateTime);
    callDurationTimer->start(1000);
    callDuration->show();
}

void ChatForm::deleteCounter()
{
    if (callDurationTimer == nullptr) {
        return;
    }
    callDurationTimer->stop();
    callDuration->setText("");
    callDuration->hide();

    delete callDurationTimer;
    callDurationTimer = nullptr;
}

void ChatForm::onUpdateTime()
{
    callDuration->setText(secondsToDHMS(callManager.seconds(f->getPublicKey())));
}

void ChatForm::setFriendTyping(bool isTyping_)
{
    chatWidget->setTypingNotificationVisible(isTyping_);
    const QString name = f->getDisplayedName();
    chatWidget->setTypingNotificationName(name);
}

void ChatForm::show(ContentLayout* contentLayout_)
{
    GenericChatForm::show(contentLayout_);
}

void ChatForm::reloadTheme()
{
    GenericChatForm::reloadTheme();
}

void ChatForm::showEvent(QShowEvent* event)
{
    GenericChatForm::showEvent(event);
}

void ChatForm::hideEvent(QHideEvent* event)
{
    GenericChatForm::hideEvent(event);
}

void ChatForm::retranslateUi()
{
    copyStatusAction->setText(tr("Copy"));

    updateMuteMicButton();
    updateMuteVolButton();

    if (netcam) {
        netcam->setShowMessages(chatWidget->isVisible());
    }
}

void ChatForm::showNetcam()
{
    if (!netcam) {
        netcam = createNetcam();
    }

    connect(netcam.get(), &NetCamView::showMessageClicked, this, &ChatForm::onShowMessagesClicked);

    bodySplitter->insertWidget(0, netcam.get());
    bodySplitter->setCollapsible(0, false);

    const QSize minSize = netcam->getSurfaceMinSize();
    ContentDialog* current = contentDialogManager.current();
    if (current != nullptr) {
        current->onVideoShow(minSize);
    }
}

void ChatForm::hideNetcam()
{
    if (!netcam) {
        return;
    }

    ContentDialog* current = contentDialogManager.current();
    if (current != nullptr) {
        current->onVideoHide();
    }

    netcam->close();
    netcam->hide();
    netcam.reset();
    callVideo.reset();
}

void ChatForm::onSplitterMoved(int pos, int index)
{
    std::ignore = pos;
    std::ignore = index;
    if (netcam) {
        netcam->setShowMessages(bodySplitter->sizes()[1] == 0);
    }
}

void ChatForm::onShowMessagesClicked()
{
    if (netcam) {
        if (bodySplitter->sizes()[1] == 0) {
            bodySplitter->setSizes({1, 1});
        } else {
            bodySplitter->setSizes({1, 0});
        }

        onSplitterMoved(0, 0);
    }
}
