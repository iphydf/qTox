/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2017-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "iprofileinfo.h"

#include "util/interface.h"

#include <QObject>

class Core;
class QPoint;
class Profile;
class Settings;

class ProfileInfo : public QObject, public IProfileInfo
{
    Q_OBJECT
public:
    ProfileInfo(Core* core_, Profile* profile_, Settings& settings);

    bool setPassword(const QString& password) override;
    bool deletePassword() override;
    bool isEncrypted() const override;

    void copyId() const override;

    void setUsername(const QString& name) override;
    void setStatusMessage(const QString& status) override;

    QString getProfileName() const override;
    RenameResult renameProfile(const QString& name) override;
    SaveResult exportProfile(const QString& path) const override;
    QStringList removeProfile() override;
    void logout() override;

    void copyQr(const QImage& image) const override;
    SaveResult saveQr(const QImage& image, const QString& path) const override;

    SetAvatarResult setAvatar(const QString& path) override;
    void removeAvatar() override;

    static IProfileInfo::SetAvatarResult createAvatar(const QByteArray& image, QByteArray& avatar);

    SIGNAL_IMPL(ProfileInfo, idChanged, const ToxId& id)
    SIGNAL_IMPL(ProfileInfo, usernameChanged, const QString& name)
    SIGNAL_IMPL(ProfileInfo, statusMessageChanged, const QString& message)

signals:
    /** The profile was left; the UI shows the login screen with this profile chosen. */
    void loginRequested(const QString& profileName);

private:
    static IProfileInfo::SetAvatarResult byteArrayToPng(QByteArray inData, QByteArray& outPng);
    static IProfileInfo::SetAvatarResult scalePngToAvatar(QByteArray& avatar);
    Profile* const profile;
    Core* const core;
    Settings& settings;
};
