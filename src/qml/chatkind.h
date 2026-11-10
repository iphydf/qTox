/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QObject>

#include <QtQml/qqmlregistration.h>
#include <optional>

/**
 * @brief The kinds of chat: with one friend, in a conference, or in a group.
 */
namespace ChatKind {
Q_NAMESPACE
QML_ELEMENT

enum class Kind
{
    Friend,
    Conference,
    Group,
};
Q_ENUM_NS(Kind)

/** The kind the Tox service's interface passes as an int, if it is one. */
inline std::optional<Kind> fromInt(int kind)
{
    switch (static_cast<Kind>(kind)) {
    case Kind::Friend:
    case Kind::Conference:
    case Kind::Group:
        return static_cast<Kind>(kind);
    }
    return std::nullopt;
}
} // namespace ChatKind
