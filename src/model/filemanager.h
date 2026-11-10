/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/core/toxfile.h"
#include "src/core/toxpk.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <chrono>
#include <optional>

class ChatManager;
class Core;
class FriendList;
class Settings;

/**
 * @brief The files friends send and receive: what the chat shows of a transfer, and which offered
 * files we accept at once. A user interface shows the transfers and tells the user's choices.
 */
class FileManager : public QObject
{
    Q_OBJECT

public:
    FileManager(FriendList& friendList, ChatManager& chatManager, Settings& settings,
                QObject* parent = nullptr);

    void connectToCore(Core& core);

    /** Toxcore says a transfer's progress for every chunk: with this, we pass it on less often. */
    void setProgressInterval(std::chrono::milliseconds interval);
    /** Where the files we accept at once go, instead of the folders the settings name. */
    void setAutoAcceptDir(const QString& dir);

    /** A file that a friend offers and nobody has accepted yet. */
    std::optional<ToxFile> offeredFile(const ToxPk& friendPk, uint32_t fileNum) const;
    /**
     * Receives @p file into @p dir, under a name that no file there has.
     * @return false if we can't write there.
     */
    bool accept(const ToxFile& file, const QString& dir);

    enum class SendResult
    {
        Sent,
        CannotOpen,
        /** A pipe or the like: it has no size and can't be read again. */
        Sequential,
    };
    /** Offers the file at @p path to a friend. */
    SendResult send(const ToxPk& friendPk, const QString& path);

    /** A path in @p dir for a new file named @p fileName, numbered as in "name (1).txt" if taken. */
    static QString uniquePath(const QString& dir, const QString& fileName);

signals:
    /** After the chat has it. */
    void updated(const ToxFile& file);
    /** A friend offers a file. If we accept it at once, we have done so. */
    void offered(const ToxFile& file, const ToxPk& friendPk);

private:
    void onFileUpdated(const ToxFile& file);
    void onFileProgress(const ToxFile& file);
    void onFileSendFailed(uint32_t friendId, const QString& fileName);
    QString autoAcceptDirFor(const ToxPk& friendPk, uint64_t fileSize) const;

    FriendList& friendList;
    ChatManager& chatManager;
    Settings& settings;
    Core* core = nullptr;
    std::chrono::milliseconds progressInterval{0};
    QString autoAcceptDir;
    QHash<quint64, ToxFile> offeredFiles;
    // When each transfer's progress was last passed on.
    QHash<quint64, std::chrono::steady_clock::time_point> progressShown;
};
