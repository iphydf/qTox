/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "backend.h"

#include "src/qml/toxservice.h"

#include <QDebug>
#include <QUrl>

Backend::Backend(const QUrl& url, INotifier& notifier, const QString& settingsDir)
    : settings{messageBoxManager,
               settingsDir.isEmpty() ? Paths::Portable::Auto : Paths::Portable::Portable, settingsDir}
    , host{url}
{
    if (!isListening()) {
        qCritical() << "The Tox service can't listen at" << url;
        return;
    }
    service = std::make_unique<ToxService>(settings, messageBoxManager, cameraSource, notifier, host);
    QObject::connect(&messageBoxManager, &QmlMessageBoxManager::message, service.get(),
                     &ToxService::error);
}

Backend::~Backend() = default;

ToxService& Backend::getService()
{
    return *service;
}

bool Backend::isListening() const
{
    return host.lastError() == QRemoteObjectNode::NoError;
}
