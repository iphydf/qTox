/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#pragma once

#include "src/core/chatid.h"

#include <QByteArray>

#include <cstdint>
#include <memory>
#include <optional>

class ToxPk : public ChatId
{
    explicit ToxPk(QByteArray rawId);

public:
    static constexpr int size = 32;
    static constexpr int numHexChars = size * 2;
    ToxPk();
    static std::optional<ToxPk> parse(QByteArray rawId);
    static std::optional<ToxPk> parse(const uint8_t* rawId);
    static std::optional<ToxPk> parse(const QString& pk);
    int getSize() const override;
    std::unique_ptr<ChatId> clone() const override;
};
