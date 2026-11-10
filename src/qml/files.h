/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

class QObject;
class QUrl;

namespace Files {
/** A path in @p dir for a new file named @p fileName, numbered as in "name (1).txt" if taken. */
QString uniquePath(const QString& dir, const QString& fileName);

/** What QFile opens for a file that a FileDialog chose: on Android, a content: URL. */
QString path(const QUrl& file);

/** The name the user sees of a file that a FileDialog chose. */
QString name(const QUrl& file);

/**
 * @brief Reads a file off the GUI thread: a content: URL's provider may be slow.
 *
 * @p done runs in @p context's thread, with nothing if the file can't be read.
 */
void read(const QString& path, QObject* context, std::function<void(std::optional<QByteArray>)> done);

/** Like read(): replaces the file's content with @p data, and says whether it did. */
void write(const QString& path, const QByteArray& data, QObject* context,
           std::function<void(bool)> done);

/** Like write(), from the file @p source, a piece at a time: files can be big. */
void copy(const QString& source, const QString& target, QObject* context,
          std::function<void(bool)> done);
} // namespace Files
