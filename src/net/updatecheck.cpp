/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */
#include "src/net/updatecheck.h"

#include "src/net/updateversion.h"
#include "src/persistence/settings.h"
#include "src/version.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QRegularExpression>
#include <QTimer>

#include <cassert>
#include <optional>

namespace {
#ifdef UPDATE_CHECK_ENABLED
const QUrl versionUrl{QStringLiteral("https://api.github.com/repos/TokTok/qTox/releases/latest")};
#endif // UPDATE_CHECK_ENABLED
} // namespace

bool UpdateCheck::isCurrentVersionStable()
{
    return isVersionStable(VersionInfo::gitDescribeExact());
}

UpdateCheck::UpdateCheck(const Settings& settings_, QObject* parent)
    : QObject(parent)
    , settings(settings_)
{
    qInfo() << "qTox is running version:" << VersionInfo::gitDescribe();
#ifdef UPDATE_CHECK_ENABLED
    updateTimer.start(1000 * 60 * 60 * 24 /* 1 day */);
    connect(&updateTimer, &QTimer::timeout, this, &UpdateCheck::checkForUpdate);
    connect(&manager, &QNetworkAccessManager::finished, this, &UpdateCheck::handleResponse);
#endif
}

void UpdateCheck::checkForUpdate()
{
#ifdef UPDATE_CHECK_ENABLED
    if (!settings.getCheckUpdates()) {
        // still run the timer to check periodically incase setting changes
        return;
    }

    manager.setProxy(settings.getProxy());
    QNetworkRequest request{versionUrl};
    // If GITHUB_TOKEN is set, use it to increase rate limit.
    const QByteArray token = qgetenv("GITHUB_TOKEN");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", "token " + token);
    }
    manager.get(request);
#endif
}

void UpdateCheck::handleResponse(QNetworkReply* reply)
{
    assert(reply != nullptr);
    if (reply == nullptr) {
        qWarning() << "Update check returned null reply, ignoring";
        return;
    }

#ifdef UPDATE_CHECK_ENABLED
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to check for update:" << reply->error();
        emit updateCheckFailed();
        reply->deleteLater();
        return;
    }
    const QByteArray result = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(result);
    const QJsonObject jObject = doc.object();
    const QVariantMap mainMap = jObject.toVariantMap();
    const QString latestVersion = mainMap["tag_name"].toString();
    if (latestVersion.isEmpty()) {
        qWarning() << "No tag name found in response:";
        emit updateCheckFailed();
        reply->deleteLater();
        return;
    }

    const QUrl link{mainMap["html_url"].toString()};
    emit complete(VersionInfo::gitDescribe(), latestVersion, link);

    if (!isCurrentVersionStable()) {
        qWarning() << "Currently running an untested/unstable version of qTox";
        emit versionIsUnstable();
    }

    const auto currentVer = tagToVersion(VersionInfo::gitDescribe());
    const auto availableVer = tagToVersion(latestVersion);

    if (currentVer && availableVer && isUpdateAvailable(*currentVer, *availableVer)) {
        qInfo() << "Update available from version" << *currentVer << "to" << *availableVer;
        emit updateAvailable(latestVersion, link);
    } else {
        if (currentVer) {
            qInfo() << "qTox is up to date:" << *currentVer;
        }
        emit upToDate();
    }

    reply->deleteLater();
#endif
}
