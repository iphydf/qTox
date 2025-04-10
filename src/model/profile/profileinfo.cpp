/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2017-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "profileinfo.h"

#include "src/core/core.h"
#include "src/model/toxclientstandards.h"
#include "src/nexus.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QFile>
#include <QImageReader>

namespace {
/**
 * @brief Convert QImage to png image.
 * @param pic Picture to convert.
 * @return Byte array with png image.
 */
QByteArray picToPng(const QImage& pic)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pic.save(&buffer, "PNG");
    buffer.close();
    return bytes;
}
/**
 * @brief Remove characters not supported for profile name from string.
 * @param src Source string.
 * @return Sanitized string.
 */
QString sanitize(const QString& src)
{
    QString name = src;
    // these are pretty much Windows banned filename characters
    const QList<QChar> banned{'/', '\\', ':', '<', '>', '"', '|', '?', '*'};
    for (const QChar c : banned) {
        name.replace(c, '_');
    }

    // also remove leading and trailing periods
    if (name[0] == '.') {
        name[0] = '_';
    }

    if (name.endsWith('.')) {
        name[name.length() - 1] = '_';
    }

    return name;
}

// TODO: Find out what is dangerous?
/**
 * @brief Dangerous way to find out if a path is writable.
 * @param filepath Path to file which should be deleted.
 * @return True, if file writeable, false otherwise.
 */
bool tryRemoveFile(const QString& filepath)
{
    QFile tmp(filepath);
    const bool writable = tmp.open(QIODevice::WriteOnly);
    tmp.remove();
    return writable;
}
} // namespace

/**
 * @class ProfileInfo
 * @brief Implement interface, that provides information about self profile.
 * Also, provide methods to work with profile file.
 * @note Should be used only when QApplication constructed.
 */

/**
 * @brief ProfileInfo constructor.
 * @param core Pointer to Tox Core.
 * @param profile Pointer to Profile.
 * @note All pointers parameters shouldn't be null.
 */
ProfileInfo::ProfileInfo(Core* core_, Profile* profile_, Settings& settings_, Nexus& nexus_)
    : profile{profile_}
    , core{core_}
    , settings{settings_}
    , nexus{nexus_}
{
    connect(core_, &Core::idSet, this, &ProfileInfo::idChanged);
    connect(core_, &Core::usernameSet, this, &ProfileInfo::usernameChanged);
    connect(core_, &Core::statusMessageSet, this, &ProfileInfo::statusMessageChanged);
}

/**
 * @brief Set a user password for profile.
 * @param password New password.
 * @return True on success, false otherwise.
 */
bool ProfileInfo::setPassword(const QString& password)
{
    const QString errorMsg = profile->setPassword(password);
    return errorMsg.isEmpty();
}

/**
 * @brief Delete a user password for profile.
 * @return True on success, false otherwise.
 */
bool ProfileInfo::deletePassword()
{
    const QString errorMsg = profile->setPassword("");
    return errorMsg.isEmpty();
}

/**
 * @brief Check if current profile is encrypted.
 * @return True if encrypted, false otherwise.
 */
bool ProfileInfo::isEncrypted() const
{
    return profile->isEncrypted();
}

/**
 * @brief Copy self ToxId to clipboard.
 */
void ProfileInfo::copyId() const
{
    const ToxId selfId = core->getSelfId();
    const QString txt = selfId.toString();
    QClipboard* clip = QApplication::clipboard();
    clip->setText(txt, QClipboard::Clipboard);
    if (clip->supportsSelection()) {
        clip->setText(txt, QClipboard::Selection);
    }
}

/**
 * @brief Set self user name.
 * @param name New name.
 */
void ProfileInfo::setUsername(const QString& name)
{
    core->setUsername(name);
}

/**
 * @brief Set self status message.
 * @param status New status message.
 */
void ProfileInfo::setStatusMessage(const QString& status)
{
    core->setStatusMessage(status);
}

/**
 * @brief Get name of tox profile file.
 * @return Profile name.
 */
QString ProfileInfo::getProfileName() const
{
    return profile->getName();
}

/**
 * @brief Rename profile file.
 * @param name New profile name.
 * @return Result code of rename operation.
 */
IProfileInfo::RenameResult ProfileInfo::renameProfile(const QString& name)
{
    const QString cur = profile->getName();
    if (name.isEmpty()) {
        return RenameResult::EmptyName;
    }

    const QString newName = sanitize(name);

    if (Profile::exists(newName, settings.getPaths())) {
        return RenameResult::ProfileAlreadyExists;
    }

    if (!profile->rename(name)) {
        return RenameResult::Error;
    }

    return RenameResult::OK;
}

/**
 * @brief Save profile in custom place.
 * @param path Path to save profile.
 * @return Result code of save operation.
 */
IProfileInfo::SaveResult ProfileInfo::exportProfile(const QString& path) const
{
    const QString current = profile->getName() + Core::TOX_EXT;
    if (path.isEmpty()) {
        return SaveResult::EmptyPath;
    }

    if (!tryRemoveFile(path)) {
        return SaveResult::NoWritePermission;
    }

    if (!QFile::copy(settings.getPaths().getSettingsDirPath() + current, path)) {
        return SaveResult::Error;
    }

    return SaveResult::OK;
}

/**
 * @brief Remove profile.
 * @return List of files, which couldn't be removed automaticaly.
 */
QStringList ProfileInfo::removeProfile()
{
    QStringList manualDeleteFiles = profile->remove();
    QMetaObject::invokeMethod(&nexus, "showLogin");
    return manualDeleteFiles;
}

/**
 * @brief Log out from current profile.
 */
void ProfileInfo::logout()
{
    // TODO(kriby): Refactor all of these invokeMethod calls with connect() properly when possible
    settings.saveGlobal();
    QMetaObject::invokeMethod(&nexus, "showLogin", Q_ARG(QString, settings.getCurrentProfile()));
}

/**
 * @brief Copy image to clipboard.
 * @param image Image to copy.
 */
void ProfileInfo::copyQr(const QImage& image) const
{
    QApplication::clipboard()->setImage(image);
}

/**
 * @brief Save image to file.
 * @param image Image to save.
 * @param path Path to save.
 * @return Result code of save operation.
 */
IProfileInfo::SaveResult ProfileInfo::saveQr(const QImage& image, const QString& path) const
{
    const QString current = profile->getName() + ".png";
    if (path.isEmpty()) {
        return SaveResult::EmptyPath;
    }

    if (!tryRemoveFile(path)) {
        return SaveResult::NoWritePermission;
    }

    // nullptr - image format same as file extension,
    // 75-quality, png file is ~6.3kb
    if (!image.save(path, nullptr, 75)) {
        return SaveResult::Error;
    }

    return SaveResult::OK;
}

/**
 * @brief Set self avatar.
 * @param path Path to image, which should be the new avatar.
 * @return Code of set avatar operation.
 */
IProfileInfo::SetAvatarResult ProfileInfo::setAvatar(const QString& path)
{
    if (path.isEmpty()) {
        return SetAvatarResult::EmptyPath;
    }

    QFile file(path);
    file.open(QIODevice::ReadOnly);
    if (!file.isOpen()) {
        return SetAvatarResult::CanNotOpen;
    }
    QByteArray avatar;
    const auto err = createAvatarFromFile(file, avatar);
    if (err == SetAvatarResult::OK) {
        profile->setAvatar(avatar);
    }
    return err;
}

/**
 * @brief Create an avatar from an image file
 * @param file Image file, which should be the new avatar.
 * @param avatar Output avatar of correct file type and size.
 * @return SetAvatarResult
 */
IProfileInfo::SetAvatarResult ProfileInfo::createAvatarFromFile(QFile& file, QByteArray& avatar)
{
    const QByteArray fileContents{file.readAll()};
    auto err = byteArrayToPng(fileContents, avatar);
    if (err != SetAvatarResult::OK) {
        return err;
    }

    err = scalePngToAvatar(avatar);
    return err;
}

/**
 * @brief Create a png from image data
 * @param inData byte array from an image file.
 * @param outPng byte array which the png will be written to.
 * @return SetAvatarResult
 */
IProfileInfo::SetAvatarResult ProfileInfo::byteArrayToPng(QByteArray inData, QByteArray& outPng)
{
    QBuffer inBuffer{&inData};
    QImageReader reader{&inBuffer};
    QImage image;
    const auto format = reader.format();
    // read whole image even if we're not going to use the QImage, to make sure the image is valid
    if (!reader.read(&image)) {
        return SetAvatarResult::CanNotRead;
    }

    if (format == "png") {
        // FIXME: re-encode the png even though inData is already valid. This strips the metadata
        // since we don't have a good png metadata stripping method currently.
        outPng = picToPng(image);
    } else {
        outPng = picToPng(image);
    }
    return SetAvatarResult::OK;
}

/*
 * @brief Scale a png to an acceptable file size.
 * @param avatar byte array containing the avatar.
 * @return SetAvatarResult
 */
IProfileInfo::SetAvatarResult ProfileInfo::scalePngToAvatar(QByteArray& avatar)
{
    // We do a first rescale to 256x256 in case the image was huge, then keep trying from here
    constexpr int scaleSizes[] = {256, 128, 64, 32};

    for (auto scaleSize : scaleSizes) {
        if (ToxClientStandards::IsValidAvatarSize(avatar.size()))
            break;
        QImage image;
        image.loadFromData(avatar);
        image = image.scaled(scaleSize, scaleSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        avatar = picToPng(image);
    }

    // If this happens, you're really doing it on purpose.
    if (!ToxClientStandards::IsValidAvatarSize(avatar.size())) {
        return SetAvatarResult::TooLarge;
    }
    return SetAvatarResult::OK;
}

/**
 * @brief Remove self avatar.
 */
void ProfileInfo::removeAvatar()
{
    profile->removeSelfAvatar();
}
