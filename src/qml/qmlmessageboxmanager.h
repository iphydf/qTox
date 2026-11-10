/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/imessageboxmanager.h"

#include <QObject>

/**
 * @brief Passes messages for the user on to QML, which shows them.
 *
 * QML can't block for an answer, so questions get their default answer.
 */
class QmlMessageBoxManager : public QObject, public IMessageBoxManager
{
    Q_OBJECT

public:
    void showInfo(const QString& title, const QString& msg) override;
    void showWarning(const QString& title, const QString& msg) override;
    void showError(const QString& title, const QString& msg) override;
    bool askQuestion(const QString& title, const QString& msg, bool defaultAns = false,
                     bool warning = true, bool yesno = true) override;
    bool askQuestion(const QString& title, const QString& msg, const QString& button1,
                     const QString& button2, bool defaultAns = false, bool warning = true) override;
    void confirmExecutableOpen(const QFileInfo& file) override;

signals:
    void message(const QString& title, const QString& text);
};
