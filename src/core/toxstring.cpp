/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2017-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "toxstring.h"

#include <QByteArray>
#include <QDebug>
#include <QSet>
#include <QString>

#include <cassert>
#include <climits>
#include <utility>

/**
 * @class ToxString
 * @brief Helper to convert safely between strings in the c-toxcore representation and QString.
 */

/**
 * @brief Creates a ToxString from a QString.
 * @param string Input text.
 */
ToxString::ToxString(const QString& text)
    : ToxString(text.toUtf8())
{
}

/**
 * @brief Creates a ToxString from bytes in a QByteArray.
 * @param text Input text.
 */
ToxString::ToxString(QByteArray text)
    : string(std::move(text))
{
}

/**
 * @brief Creates a ToxString from the representation used by c-toxcore.
 * @param text Pointer to the beginning of the text.
 * @param length Number of bytes to read from the beginning.
 */
ToxString::ToxString(const uint8_t* text, size_t length)
{
    assert(length <= INT_MAX);
    string = QByteArray(reinterpret_cast<const char*>(text), length);
}

/**
 * @brief Returns a pointer to the beginning of the string data.
 * @return Pointer to the beginning of the string data.
 */
const uint8_t* ToxString::data() const
{
    return reinterpret_cast<const uint8_t*>(string.constData());
}

/**
 * @brief Get the number of bytes in the string.
 * @return Number of bytes in the string.
 */
size_t ToxString::size() const
{
    return string.size();
}

/**
 * @brief Interpret the string as UTF-8 encoded QString.
 *
 * Replaces any non-printable characters in the string with escape sequences. This is a defense-in-depth
 * measure to prevent some potential security issues caused by bugs in client code or one of its dependencies.
 */
QString ToxString::getQString() const
{
    const auto tainted = QString::fromUtf8(string).toStdU32String();
    QSet<std::pair<QChar::Category, char32_t>> removed;
    std::u32string cleaned;
    for (const char32_t c : tainted) {
        const auto category = QChar::category(c);
        // Cf (Other_Format) is to allow skin-color modifiers for emojis.
        // We also allow newlines and tabs, which are Other_Control, and covered by isSpace.
        // We also allow \0, so we can later use it to filter out post-NUL garbage.
        if (c == 0 || QChar::isPrint(c) || QChar::isSpace(c)
            || category == QChar::Category::Other_Format) {
            cleaned.push_back(c);
            continue;
        }
        removed.insert({category, c});
        // Add a formatted escape sequence so non-printable characters are still
        // visible in chat logs and can be easily identified.
        cleaned += QStringLiteral("\\x%1")
                       .arg(static_cast<std::uint_least32_t>(c), 2, 16, QChar('0'))
                       .toStdU32String();
    }
    if (!removed.isEmpty()) {
        qWarning() << "Removed non-printable characters from a string:" << removed;
    }
    return QString::fromStdU32String(cleaned);
}

/**
 * @brief Returns the bytes verbatim as they were received from or will be sent to the network.
 *
 * No cleanup or interpretation is done here. The bytes are returned as they were received from the
 * network. Callers should be careful when processing these bytes. If UTF-8 messages are expected,
 * use getQString() instead.
 */
const QByteArray& ToxString::getBytes() const
{
    return string;
}
