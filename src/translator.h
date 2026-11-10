/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */


#pragma once

#include <QMutex>
#include <QPair>
#include <QStringList>
#include <QVector>

#include <functional>

class QObject;
class QTranslator;

class Translator
{
public:
    static const QStringList& getLocales();
    static QString getLanguageName(const QString& localeName);
    static void translate(const QString& localeName);
    static void registerHandler(const std::function<void()>& f, QObject* owner);
    static void unregister(QObject* owner);

private:
    using Callback = QPair<QObject*, std::function<void()>>;
    static QVector<Callback> callbacks;
    static QMutex lock;
    static QTranslator* core_translator;
    static QTranslator* app_translator;
};
