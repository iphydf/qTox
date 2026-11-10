/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QObject>

#include <optional>

class Chat;

/**
 * @brief The kinds of chat: with one friend, in a conference, or in a group.
 */
namespace ChatKind {
Q_NAMESPACE

enum class Kind
{
    Friend,
    Conference,
    Group,
};
Q_ENUM_NS(Kind)

/** The kind an int stands for, if it stands for one: the Tox service's interface passes ints. */
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

/** The kind of @p chat. */
Kind of(const Chat& chat);
} // namespace ChatKind
