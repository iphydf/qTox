/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "files.h"

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QThread>
#include <QUrl>

#include <memory>
#include <utility>

namespace {
template <typename Result>
void inBackground(QObject* context, std::function<Result()> work, std::function<void(Result)> done)
{
    auto result = std::make_shared<Result>();
    QThread* thread = QThread::create([result, work = std::move(work)] { *result = work(); });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, context,
                     [result, done = std::move(done)] { done(std::move(*result)); });
    thread->start();
}
} // namespace

namespace Files {
QString uniquePath(const QString& dir, const QString& fileName)
{
    // As the Widgets UI names the files it receives.
    const QFileInfo name{fileName};
    const QString base = name.baseName();
    const QString suffix = name.completeSuffix().isEmpty() ? QString{} : "." + name.completeSuffix();
    QString path = QStringLiteral("%1/%2%3").arg(dir, base, suffix);
    for (int number = 1; QFileInfo::exists(path); ++number) {
        path = QStringLiteral("%1/%2 (%3)%4").arg(dir, base).arg(number).arg(suffix);
    }
    return path;
}

QString path(const QUrl& file)
{
    return file.isLocalFile() ? file.toLocalFile() : file.toString();
}

QString name(const QUrl& file)
{
    // Only fileName() asks a content: URL's provider: the rest reads the URL.
    return QFileInfo{path(file)}.fileName();
}

void read(const QString& path, QObject* context, std::function<void(std::optional<QByteArray>)> done)
{
    inBackground<std::optional<QByteArray>>(
        context,
        [path]() -> std::optional<QByteArray> {
            QFile file{path};
            if (!file.open(QIODevice::ReadOnly)) {
                return std::nullopt;
            }
            return file.readAll();
        },
        std::move(done));
}

void write(const QString& path, const QByteArray& data, QObject* context, std::function<void(bool)> done)
{
    inBackground<bool>(
        context,
        [path, data] {
            QFile file{path};
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                   && file.write(data) == data.size() && file.flush();
        },
        std::move(done));
}
void copy(const QString& source, const QString& target, QObject* context, std::function<void(bool)> done)
{
    inBackground<bool>(
        context,
        [source, target] {
            constexpr qint64 pieceSize = 1 << 20;
            QFile from{source};
            QFile to{target};
            if (!from.open(QIODevice::ReadOnly)
                || !to.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                return false;
            }
            while (!from.atEnd()) {
                const QByteArray piece = from.read(pieceSize);
                if (piece.isEmpty() || to.write(piece) != piece.size()) {
                    return false;
                }
            }
            return to.flush();
        },
        std::move(done));
}
} // namespace Files
