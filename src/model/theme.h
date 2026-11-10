/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QColor>
#include <QFont>
#include <QList>
#include <QObject>
#include <QString>

/**
 * @brief The built-in themes' colours and fonts, for every user interface.
 */
namespace Theme {
Q_NAMESPACE

enum class MainTheme
{
    Light,
    Dark,
};
Q_ENUM_NS(MainTheme)

enum class Font
{
    ExtraBig,
    Big,
    BigBold,
    Medium,
    MediumBold,
    Small,
    SmallLight,
};

struct Color
{
    MainTheme type;
    QString name;
    QColor color;
};

/** The theme colours a user can choose; settings store the index. */
const QList<Color>& colors();
int defaultColor(MainTheme theme);
MainTheme system();
QFont font(Font font);
} // namespace Theme
