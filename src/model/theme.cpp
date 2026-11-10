/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "theme.h"

#include <QFontInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

#include <map>

/**
 * @enum Theme::Font
 *
 * @var ExtraBig
 * @brief [SystemDefault + 2]px, bold
 *
 * @var Big
 * @brief [SystemDefault]px
 *
 * @var BigBold
 * @brief [SystemDefault]px, bold
 *
 * @var Medium
 * @brief [SystemDefault - 1]px
 *
 * @var MediumBold
 * @brief [SystemDefault - 1]px, bold
 *
 * @var Small
 * @brief [SystemDefault - 2]px
 *
 * @var SmallLight
 * @brief [SystemDefault - 2]px, light
 */

namespace {
// helper functions
QFont appFont(int pixelSize, QFont::Weight weight)
{
    QFont font;
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

using Theme::MainTheme;
const QList<Theme::Color> themeNameColors = {
    {MainTheme::Light, QObject::tr("Default"), QColor()},
    {MainTheme::Light, QObject::tr("Blue"), QColor("#004aa4")},
    {MainTheme::Light, QObject::tr("Olive"), QColor("#97ba00")},
    {MainTheme::Light, QObject::tr("Red"), QColor("#c23716")},
    {MainTheme::Light, QObject::tr("Violet"), QColor("#4617b5")},
    {MainTheme::Dark, QObject::tr("Dark"), QColor()},
    {MainTheme::Dark, QObject::tr("Dark blue"), QColor("#00336d")},
    {MainTheme::Dark, QObject::tr("Dark olive"), QColor("#4d5f00")},
    {MainTheme::Dark, QObject::tr("Dark red"), QColor("#7a210d")},
    {MainTheme::Dark, QObject::tr("Dark violet"), QColor("#280d6c")},
};
} // namespace

const QList<Theme::Color>& Theme::colors()
{
    return themeNameColors;
}

int Theme::defaultColor(MainTheme theme)
{
    for (int i = 0; i < themeNameColors.size(); ++i) {
        if (themeNameColors[i].type == theme) {
            return i;
        }
    }
    return 0;
}

Theme::MainTheme Theme::system()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
#else
    return QGuiApplication::palette().color(QPalette::Window).value() < 128
#endif
               ? MainTheme::Dark
               : MainTheme::Light;
}

QFont Theme::font(Font font)
{
    // fonts as defined in
    // https://github.com/ItsDuke/Tox-UI/blob/master/UI%20GUIDELINES.md

    static const int defSize = QFontInfo(QFont()).pixelSize();

    static const std::map<Font, QFont> fonts = {
        {Font::ExtraBig, appFont(defSize + 3, QFont::Bold)},
        {Font::Big, appFont(defSize + 1, QFont::Normal)},
        {Font::BigBold, appFont(defSize + 1, QFont::Bold)},
        {Font::Medium, appFont(defSize, QFont::Normal)},
        {Font::MediumBold, appFont(defSize, QFont::Bold)},
        {Font::Small, appFont(defSize - 1, QFont::Normal)},
        {Font::SmallLight, appFont(defSize - 1, QFont::Light)},
    };

    return fonts.at(font);
}
