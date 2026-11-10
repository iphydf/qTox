/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "qmlmessageboxmanager.h"

#include <QDebug>

#include <tuple>

void QmlMessageBoxManager::showInfo(const QString& title, const QString& msg)
{
    emit message(title, msg);
}

void QmlMessageBoxManager::showWarning(const QString& title, const QString& msg)
{
    emit message(title, msg);
}

void QmlMessageBoxManager::showError(const QString& title, const QString& msg)
{
    emit message(title, msg);
}

bool QmlMessageBoxManager::askQuestion(const QString& title, const QString& msg, bool defaultAns,
                                       bool warning, bool yesno)
{
    std::ignore = warning;
    std::ignore = yesno;
    qWarning() << "Answering" << title << "with" << defaultAns << "for:" << msg;
    return defaultAns;
}

bool QmlMessageBoxManager::askQuestion(const QString& title, const QString& msg, const QString& button1,
                                       const QString& button2, bool defaultAns, bool warning)
{
    std::ignore = warning;
    qWarning() << "Answering" << title << "with" << (defaultAns ? button1 : button2) << "for:" << msg;
    return defaultAns;
}

void QmlMessageBoxManager::confirmExecutableOpen(const QFileInfo& file)
{
    std::ignore = file;
}
