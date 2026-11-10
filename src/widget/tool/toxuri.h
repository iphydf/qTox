/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */


#pragma once

#include <QDialog>

class Core;
// Internals
class QByteArray;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class IMessageBoxManager;
class ToxURIDialog : public QDialog
{
    Q_OBJECT
public:
    ToxURIDialog(QWidget* parent, Core& core, IMessageBoxManager& messageBoxManager);
    QString getRequestMessage();
    bool handleToxURI(const QString& toxURI);

private:
    void setUserId(const QString& userId);

private:
    QPlainTextEdit* messageEdit;
    QLabel* friendsLabel;
    QLineEdit* userIdEdit;
    Core& core;
    IMessageBoxManager& messageBoxManager;
};
