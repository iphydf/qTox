/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "chatmodel.h"

#include "src/model/ichatlog.h"
#include "src/model/textformatter.h"
#include "src/persistence/history.h"

#include <utility>

namespace {
QString fileStatus(ToxFile::FileStatus status)
{
    switch (status) {
    case ToxFile::INITIALIZING:
        return QStringLiteral("waiting");
    case ToxFile::PAUSED:
        return QStringLiteral("paused");
    case ToxFile::TRANSMITTING:
        return QStringLiteral("transmitting");
    case ToxFile::BROKEN:
        return QStringLiteral("broken");
    case ToxFile::CANCELED:
        return QStringLiteral("canceled");
    case ToxFile::FINISHED:
        return QStringLiteral("finished");
    }
    return {};
}
} // namespace

ChatModel::ChatModel(IChatLog& chatLog_, ToxPk selfPk_, ChatLogIdx first_, Style style_, QObject* parent)
    : QAbstractListModel(parent)
    , chatLog{chatLog_}
    , selfPk{std::move(selfPk_)}
    , style{style_}
    , first{first_.get()}
    , rows{chatLog.getNextIdx().get() - first}
{
    connect(&chatLog, &IChatLog::itemUpdated, this,
            [this](ChatLogIdx idx) { onItemUpdated(idx.get()); });
}

int ChatModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows);
}

QVariant ChatModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= rows) {
        return {};
    }

    const ChatLogItem& item = chatLog.at(ChatLogIdx(first + static_cast<size_t>(index.row())));
    const auto type = item.getContentType();
    const bool isMessage = type == ChatLogItem::ContentType::message;
    const bool isFile = type == ChatLogItem::ContentType::fileTransfer;

    switch (role) {
    case KindRole:
        switch (type) {
        case ChatLogItem::ContentType::message:
            return QStringLiteral("message");
        case ChatLogItem::ContentType::fileTransfer:
            return QStringLiteral("file");
        case ChatLogItem::ContentType::systemMessage:
            return QStringLiteral("system");
        }
        return {};
    case TextRole:
        return text(item);
    case SenderRole:
        return item.getDisplayName();
    case IsSelfRole:
        return item.getSender() == selfPk;
    case TimeRole:
        return item.getTimestamp();
    case DayRole:
        return item.getTimestamp().toLocalTime().date().toString(Qt::ISODate);
    case FileStatusRole:
        return isFile ? fileStatus(item.getContentAsFile().file.status) : QVariant{};
    case FileSizeRole:
        return isFile ? QVariant::fromValue(item.getContentAsFile().file.progress.getFileSize())
                      : QVariant{};
    case FileTransferredRole:
        return isFile ? QVariant::fromValue(item.getContentAsFile().file.progress.getBytesSent())
                      : QVariant{};
    case FileNumRole:
        return isFile ? QVariant::fromValue(item.getContentAsFile().file.fileNum) : QVariant{};
    case FilePathRole:
        return isFile ? item.getContentAsFile().file.filePath : QVariant{};
    case FilePausedByUserRole:
        return isFile ? item.getContentAsFile().file.pauseStatus.localPaused() : QVariant{};
    case HtmlRole:
        return isMessage ? html(item.getContentAsMessage().message.content) : QVariant{};
    case PendingRole:
        return isMessage && item.getContentAsMessage().state == MessageState::pending;
    case BrokenRole:
        return isMessage && item.getContentAsMessage().state == MessageState::broken;
    case IsActionRole:
        return isMessage && item.getContentAsMessage().message.isAction;
    case IsPrivateRole:
        return isMessage && !item.getContentAsMessage().message.recipient.isEmpty();
    case RecipientRole:
        return isMessage && item.getSender() == selfPk
                   ? item.getContentAsMessage().message.recipientName
                   : QString{};
    default:
        return {};
    }
}

QString ChatModel::text(const ChatLogItem& item)
{
    switch (item.getContentType()) {
    case ChatLogItem::ContentType::message:
        return item.getContentAsMessage().message.content;
    case ChatLogItem::ContentType::fileTransfer:
        return item.getContentAsFile().file.fileName;
    case ChatLogItem::ContentType::systemMessage:
        return item.getContentAsSystemMessage().toString();
    }
    return {};
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {TextRole, "text"},
        {SenderRole, "sender"},
        {IsSelfRole, "isSelf"},
        {TimeRole, "time"},
        {PendingRole, "pending"},
        {BrokenRole, "broken"},
        {IsActionRole, "isAction"},
        {DayRole, "day"},
        {FileStatusRole, "fileStatus"},
        {FileSizeRole, "fileSize"},
        {FileTransferredRole, "fileTransferred"},
        {FileNumRole, "fileNum"},
        {FilePathRole, "filePath"},
        {FilePausedByUserRole, "filePausedByUser"},
        {HtmlRole, "html"},
        {IsPrivateRole, "isPrivate"},
        {RecipientRole, "recipient"},
    };
}

QString ChatModel::html(const QString& message) const
{
    // As the Widgets UI's ChatMessage formats it, but for quotes and smileys.
    QString text = message.toHtmlEscaped();
    if (style.hidePostNullSuffix) {
        text = TextFormatter::processPostNullSuffix(text, true);
    }
    text = TextFormatter::highlightURI(text);
    if (style.markdown != Settings::StyleType::NONE) {
        text = TextFormatter::applyMarkdown(text, style.markdown == Settings::StyleType::WITH_CHARS);
    }
    // StyledText breaks lines only where it's told.
    return text.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
}

void ChatModel::startAt(ChatLogIdx first_)
{
    beginResetModel();
    first = first_.get();
    rows = chatLog.getNextIdx().get() - first;
    endResetModel();
}

void ChatModel::onItemUpdated(size_t idx)
{
    if (idx < first) {
        return;
    }
    const size_t row = idx - first;
    if (row < rows) {
        emit dataChanged(index(static_cast<int>(row)), index(static_cast<int>(row)));
        return;
    }

    beginInsertRows(QModelIndex(), static_cast<int>(rows), static_cast<int>(row));
    rows = row + 1;
    endInsertRows();
}
