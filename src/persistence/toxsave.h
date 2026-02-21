/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

class QString;
class QByteArray;
class Settings;
class IPC;
class QString;
class QWidget;

class ToxSave
{
public:
    const static QString eventHandlerKey;
    ToxSave(Settings& settings, IPC& ipc, QWidget* parent);
    ~ToxSave();
    bool handleToxSave(const QString& path);
    static bool toxSaveEventHandler(const QByteArray& eventData, void* userData);

private:
    Settings& settings;
    IPC& ipc;
    QWidget* parent;
};
