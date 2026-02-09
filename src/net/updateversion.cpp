/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/net/updateversion.h"

#include <QRegularExpression>

namespace {
const QRegularExpression versionRegex{QStringLiteral(R"(^v([0-9]+)\.([0-9]+)\.([0-9]+)$)")};
} // namespace

QDebug& operator<<(QDebug& stream, const Version& version)
{
    stream.noquote()
        << QStringLiteral("v%1.%2.%3").arg(version.major).arg(version.minor).arg(version.patch);
    return stream.quote();
}

std::optional<Version> tagToVersion(const QString& tagName)
{
    const auto& matches = versionRegex.match(tagName);
    if (matches.lastCapturedIndex() != 3) {
        return std::nullopt;
    }

    bool ok;
    const auto major = matches.captured(1).toInt(&ok);
    if (!ok) {
        return std::nullopt;
    }

    const auto minor = matches.captured(2).toInt(&ok);
    if (!ok) {
        return std::nullopt;
    }

    const auto patch = matches.captured(3).toInt(&ok);
    if (!ok) {
        return std::nullopt;
    }

    return Version{major, minor, patch};
}

bool isUpdateAvailable(const Version& current, const Version& available)
{
    if (current.major < available.major) {
        return true;
    }
    if (current.major > available.major) {
        return false;
    }

    if (current.minor < available.minor) {
        return true;
    }
    if (current.minor > available.minor) {
        return false;
    }

    if (current.patch < available.patch) {
        return true;
    }
    if (current.patch > available.patch) {
        return false;
    }

    return false;
}

bool isVersionStable(const QString& gitDescribeExact)
{
    return versionRegex.match(gitDescribeExact).hasMatch();
}
