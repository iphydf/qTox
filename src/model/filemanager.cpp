/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "filemanager.h"

#include "src/core/core.h"
#include "src/core/corefile.h"
#include "src/friendlist.h"
#include "src/model/chathistory.h"
#include "src/model/chatmanager.h"
#include "src/model/friend.h"
#include "src/persistence/settings.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <tox/tox.h>

namespace {
quint64 fileKey(uint32_t friendId, uint32_t fileNum)
{
    return (static_cast<quint64>(friendId) << 32) | fileNum;
}

bool isDone(ToxFile::FileStatus status)
{
    return status == ToxFile::CANCELED || status == ToxFile::FINISHED || status == ToxFile::BROKEN;
}

bool isWritable(const QString& path)
{
    QFile file{path};
    const bool writable = file.open(QIODevice::WriteOnly);
    file.remove();
    return writable;
}
} // namespace

FileManager::FileManager(FriendList& friendList_, ChatManager& chatManager_, Settings& settings_,
                         QObject* parent)
    : QObject{parent}
    , friendList{friendList_}
    , chatManager{chatManager_}
    , settings{settings_}
{
}

void FileManager::connectToCore(Core& core_)
{
    core = &core_;
    const CoreFile* coreFile = core->getCoreFile();
    // All queued, to hear of a file in Core's order: what its thread says comes queued, and a pause
    // from this thread would overtake the progress before it.
    for (const auto signal : {&CoreFile::fileSendStarted, &CoreFile::fileReceiveRequested,
                              &CoreFile::fileTransferAccepted, &CoreFile::fileTransferCancelled,
                              &CoreFile::fileTransferFinished, &CoreFile::fileTransferPaused}) {
        connect(coreFile, signal, this, &FileManager::onFileUpdated, Qt::QueuedConnection);
    }
    connect(coreFile, &CoreFile::fileTransferInfo, this, &FileManager::onFileProgress,
            Qt::QueuedConnection);
    connect(coreFile, &CoreFile::fileTransferRemotePausedUnpaused, this,
            &FileManager::onFileUpdated, Qt::QueuedConnection);
    connect(coreFile, &CoreFile::fileTransferBrokenUnbroken, this, &FileManager::onFileUpdated,
            Qt::QueuedConnection);
    connect(coreFile, &CoreFile::fileSendFailed, this, &FileManager::onFileSendFailed,
            Qt::QueuedConnection);
}

void FileManager::setProgressInterval(std::chrono::milliseconds interval)
{
    progressInterval = interval;
}

void FileManager::setAutoAcceptDir(const QString& dir)
{
    autoAcceptDir = dir;
}

std::optional<ToxFile> FileManager::offeredFile(const ToxPk& friendPk, uint32_t fileNum) const
{
    const Friend* f = friendList.findFriend(friendPk);
    if (f == nullptr) {
        return std::nullopt;
    }
    const auto it = offeredFiles.constFind(fileKey(f->getId(), fileNum));
    return it == offeredFiles.cend() ? std::nullopt : std::optional<ToxFile>{*it};
}

bool FileManager::accept(const ToxFile& file, const QString& dir)
{
    if (core == nullptr || !QDir{}.mkpath(dir)) {
        return false;
    }
    const QString path = uniquePath(dir, file.fileName);
    if (!isWritable(path)) {
        return false;
    }
    core->getCoreFile()->acceptFileRecvRequest(file.friendId, file.fileNum, path);
    return true;
}

FileManager::SendResult FileManager::send(const ToxPk& friendPk, const QString& path)
{
    const Friend* f = friendList.findFriend(friendPk);
    QFile file{path};
    if (core == nullptr || f == nullptr || !file.exists() || !file.open(QIODevice::ReadOnly)) {
        return SendResult::CannotOpen;
    }
    file.close();
    if (file.isSequential()) {
        return SendResult::Sequential;
    }
    core->getCoreFile()->sendFile(f->getId(), QFileInfo{path}.fileName(), path, file.size());
    return SendResult::Sent;
}

QString FileManager::uniquePath(const QString& dir, const QString& fileName)
{
    const QFileInfo name{fileName};
    const QString base = name.baseName();
    const QString suffix = name.completeSuffix().isEmpty() ? QString{} : "." + name.completeSuffix();
    QString path = QStringLiteral("%1/%2%3").arg(dir, base, suffix);
    for (int number = 1; QFileInfo::exists(path); ++number) {
        path = QStringLiteral("%1/%2 (%3)%4").arg(dir, base).arg(number).arg(suffix);
    }
    return path;
}

void FileManager::onFileUpdated(const ToxFile& file)
{
    // Avatars aren't files the user sees.
    const Friend* f = friendList.findFriend(friendList.id2Key(file.friendId));
    if (f == nullptr || file.fileKind == TOX_FILE_KIND_AVATAR) {
        return;
    }
    const ToxPk& friendPk = f->getPublicKey();

    const quint64 key = fileKey(file.friendId, file.fileNum);
    const bool isOffer = file.direction == ToxFile::RECEIVING && file.status == ToxFile::INITIALIZING;
    if (isOffer) {
        offeredFiles.insert(key, file);
    } else {
        offeredFiles.remove(key);
    }
    if (isDone(file.status)) {
        progressShown.remove(key);
    }

    const ToxPk& sender = file.direction == ToxFile::SENDING ? core->getSelfPublicKey() : friendPk;
    if (ChatHistory* chatLog = chatManager.getFriendChatLog(friendPk)) {
        chatLog->onFileUpdated(sender, file);
    }
    emit updated(file);

    if (isOffer) {
        const QString dir = autoAcceptDirFor(friendPk, file.progress.getFileSize());
        // The user can still accept it.
        if (!dir.isEmpty() && !accept(file, dir)) {
            qWarning() << "Cannot write to" << dir;
        }
        emit offered(file, friendPk);
    }
}

void FileManager::onFileProgress(const ToxFile& file)
{
    const auto now = std::chrono::steady_clock::now();
    auto& shown = progressShown[fileKey(file.friendId, file.fileNum)];
    if (now - shown >= progressInterval) {
        shown = now;
        onFileUpdated(file);
    }
}

void FileManager::onFileSendFailed(uint32_t friendId, const QString& fileName)
{
    if (IChatLog* chatLog = chatManager.getFriendChatLog(friendList.id2Key(friendId))) {
        chatLog->addSystemMessage(
            {SystemMessageType::fileSendFailed, QDateTime::currentDateTime(), {fileName}});
    }
}

QString FileManager::autoAcceptDirFor(const ToxPk& friendPk, uint64_t fileSize) const
{
    // The friend's folder, else everyone's if that is on, for files up to the size the user set.
    QString dir = settings.getAutoAcceptDir(friendPk);
    if (dir.isEmpty() && settings.getAutoSaveEnabled()) {
        dir = settings.getGlobalAutoAcceptDir();
    }
    const size_t maxSize = settings.getMaxAutoAcceptSize();
    if (dir.isEmpty() || (maxSize != 0 && fileSize > maxSize)) {
        return {};
    }
    return autoAcceptDir.isEmpty() ? dir : autoAcceptDir;
}
