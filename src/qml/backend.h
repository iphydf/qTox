/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/persistence/settings.h"
#include "src/qml/qmlmessageboxmanager.h"
#include "src/video/camerasource.h"

#include <QRemoteObjectHost>
#include <QString>

#include <memory>

class INotifier;
class QUrl;
class ToxService;

/**
 * @brief Tox and everything it needs, shared with the user interfaces at a URL.
 */
class Backend
{
public:
    /**
     * @param settingsDir Where the settings and profiles are, instead of the usual place, as with
     *                    qTox's --portable.
     */
    Backend(const QUrl& url, INotifier& notifier, const QString& settingsDir = {});
    ~Backend();
    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;
    Backend(Backend&&) = delete;
    Backend& operator=(Backend&&) = delete;

    bool isListening() const;
    ToxService& getService();

private:
    QmlMessageBoxManager messageBoxManager;
    Settings settings;
    CameraSource cameraSource{settings};
    QRemoteObjectHost host;
    std::unique_ptr<ToxService> service;
};
