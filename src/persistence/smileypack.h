/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#pragma once

#include <QIcon>
#include <QMap>
#include <QMutex>
#include <QRegularExpression>

#include <memory>

class QTimer;
class ISmileySettings;

class SmileyPack : public QObject
{
    Q_OBJECT

public:
    explicit SmileyPack(ISmileySettings& settings);
    SmileyPack(SmileyPack&) = delete;
    SmileyPack& operator=(const SmileyPack&) = delete;
    ~SmileyPack() override;

    static QList<QPair<QString, QString>> listSmileyPacks(const QStringList& paths);
    static QList<QPair<QString, QString>> listSmileyPacks();

    QString smileyfied(const QString& msg);
    QList<QStringList> getEmoticons() const;
    std::shared_ptr<QIcon> getAsIcon(const QString& emoticon) const;
    static QString getAsRichText(const QString& key);

private slots:
    void cleanupIconsCache();
    void onSmileyPackChanged();

private:
    void load(const QString& filename);
    void constructRegex();

    mutable std::map<QString, std::shared_ptr<QIcon>> cachedIcon;
    QHash<QString, QString> emoticonToPath;
    QList<QStringList> emoticons;
    QString path;
    QTimer* cleanupTimer;
    QRegularExpression smilify;
    mutable QMutex loadingMutex;
    ISmileySettings& settings;
};
