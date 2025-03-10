/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "src/core/toxfile.h"

#include <QFile>
#include <QRegularExpression>

#include <tox/tox.h>
#include <utility>

#define TOX_HEX_ID_LENGTH 2 * TOX_ADDRESS_SIZE

/**
 * @file corestructs.h
 * @brief Some headers use Core structs but don't need to include all of core.h
 *
 * They should include this file directly instead to reduce compilation times
 *
 * @var uint8_t ToxFile::fileKind
 * @brief Data file (default) or avatar
 */

ToxFile::ToxFile()
    : fileKind(0)
    , fileNum(0)
    , friendId(0)
    , status(INITIALIZING)
    , direction(SENDING)
    , progress(0)
{
}

/**
 * @brief ToxFile constructor
 */
ToxFile::ToxFile(uint32_t fileNum_, uint32_t friendId_, QString fileName_, QString filePath_,
                 uint64_t filesize, FileDirection direction_)
    : fileKind{TOX_FILE_KIND_DATA}
    , fileNum(fileNum_)
    , friendId(friendId_)
    , fileName{std::move(fileName_)}
    , filePath{filePath_}
    , file{new QFile(filePath_)}
    , status{INITIALIZING}
    , direction{direction_}
    , progress(filesize)
{
}

bool ToxFile::operator==(const ToxFile& other) const
{
    return (fileNum == other.fileNum) && (friendId == other.friendId)
           && (direction == other.direction);
}

bool ToxFile::operator!=(const ToxFile& other) const
{
    return !(*this == other);
}

void ToxFile::setFilePath(QString path)
{
    filePath = path;
    file->setFileName(path);
}

bool ToxFile::open(bool write)
{
    return write ? file->open(QIODevice::ReadWrite) : file->open(QIODevice::ReadOnly);
}
