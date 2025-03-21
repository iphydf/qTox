/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "style.h"

#include "src/persistence/settings.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontInfo>
#include <QGuiApplication>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStringBuilder>
#include <QStyle>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QWidget>

/**
 * @enum Style::Font
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
 *
 * @var BuiltinThemePath
 * @brief Path to the theme built into the application binary
 */

namespace {
const QLatin1String ThemeSubFolder{"themes/"};
const QLatin1String BuiltinThemeDefaultPath{":themes/default/"};
const QLatin1String BuiltinThemeDarkPath{":themes/dark/"};

// helper functions
QFont appFont(int pixelSize, QFont::Weight weight)
{
    QFont font;
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

QString qssifyFont(QFont font)
{
    return QString("%1 %2px \"%3\"").arg(font.weight()).arg(font.pixelSize()).arg(font.family());
}

using MainTheme = Style::MainTheme;
struct ThemeNameColor
{
    MainTheme type;
    QString name;
    QColor color;
};
const QList<ThemeNameColor> themeNameColors = {
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

using ColorPalette = Style::ColorPalette;
const QMap<ColorPalette, QString> aliasColors = {
    {ColorPalette::TransferGood, "transferGood"},
    {ColorPalette::TransferWait, "transferWait"},
    {ColorPalette::TransferBad, "transferBad"},
    {ColorPalette::TransferMiddle, "transferMiddle"},
    {ColorPalette::MainText, "mainText"},
    {ColorPalette::NameActive, "nameActive"},
    {ColorPalette::StatusActive, "statusActive"},
    {ColorPalette::GroundExtra, "groundExtra"},
    {ColorPalette::GroundBase, "groundBase"},
    {ColorPalette::Orange, "orange"},
    {ColorPalette::Yellow, "yellow"},
    {ColorPalette::ThemeDark, "themeDark"},
    {ColorPalette::ThemeMediumDark, "themeMediumDark"},
    {ColorPalette::ThemeMedium, "themeMedium"},
    {ColorPalette::ThemeLight, "themeLight"},
    {ColorPalette::Action, "action"},
    {ColorPalette::Link, "link"},
    {ColorPalette::SearchHighlighted, "searchHighlighted"},
    {ColorPalette::SelectText, "selectText"},
};
} // namespace

int Style::defaultThemeColor(MainTheme theme)
{
    for (int i = 0; i < themeNameColors.size(); ++i) {
        if (themeNameColors[i].type == theme) {
            return i;
        }
    }
    return 0;
}

QStringList Style::getThemeColorNames()
{
    QStringList l;

    for (auto t : themeNameColors) {
        l << t.name;
    }

    return l;
}

QString Style::getThemeName()
{
    // TODO: return name of the current theme
    const QString themeName = "default";
    return QStringLiteral("default");
}

QString Style::getThemeFolder(int themeColor)
{
    const QString themeName = getThemeName();
    const QString themeFolder = ThemeSubFolder % themeName;
    const QString fullPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, themeFolder,
                                                    QStandardPaths::LocateDirectory);

    // No themes available, fallback to builtin
    if (fullPath.isEmpty()) {
        return getThemePath(themeColor);
    }

    return fullPath % QDir::separator();
}

QString Style::getStylesheet(const QString& filename, const Settings& settings, const QFont& baseFont)
{
    return getStylesheet(filename, settings.getThemeColor(), baseFont);
}

QString Style::getStylesheet(const QString& filename, int themeColor, const QFont& baseFont)
{
    const QString fullPath = getThemeFolder(themeColor) + filename;
    const std::pair<const QString, const QFont> cacheKey(fullPath, baseFont);
    auto it = stylesheetsCache.find(cacheKey);
    if (it != stylesheetsCache.end()) {
        // cache hit
        return it->second;
    }
    // cache miss, new styleSheet, read it from file and add to cache
    const QString newStylesheet = resolve(filename, themeColor, baseFont);
    stylesheetsCache.insert(std::make_pair(cacheKey, newStylesheet));
    return newStylesheet;
}

QString Style::getImagePath(const QString& filename, const Settings& settings)
{
    return getImagePath(filename, settings.getThemeColor());
}

QString Style::getImagePath(const QString& filename, int themeColor)
{
    QString fullPath = getThemeFolder(themeColor) + filename;

    // search for image in cache
    if (existingImagesCache.contains(fullPath)) {
        return fullPath;
    }

    // if not in cache
    if (QFileInfo::exists(fullPath)) {
        existingImagesCache << fullPath;
        return fullPath;
    }
    qWarning() << "Failed to open file (using defaults):" << fullPath;

    fullPath = getThemePath(themeColor) % filename;

    if (QFileInfo::exists(fullPath)) {
        return fullPath;
    }
    qWarning() << "Failed to open default file:" << fullPath;
    return {};
}

QColor Style::getColor(ColorPalette entry)
{
    return palette[entry];
}

QFont Style::getFont(Font font)
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

QString Style::resolve(const QString& filename, int themeColor, const QFont& baseFont)
{
    const QString themePath = getThemeFolder(themeColor);
    QString fullPath = themePath + filename;
    QString qss;

    QFile file{fullPath};
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        qss = QString::fromUtf8(file.readAll());
    } else {
        qWarning() << "Failed to open file:" << fullPath;

        fullPath = getThemePath(themeColor);
        QFile defaultFile{fullPath};

        if (defaultFile.open(QFile::ReadOnly | QFile::Text)) {
            qss = QString::fromUtf8(defaultFile.readAll());
        } else {
            qWarning() << "Failed to open default file:" << fullPath;
            return {};
        }
    }

    if (palette.isEmpty()) {
        initPalette(themeColor);
    }

    if (dictColor.isEmpty()) {
        initDictColor();
    }

    if (dictFont.isEmpty()) {
        dictFont = {
            {"@baseFont",
             QString::fromUtf8("'%1' %2px").arg(baseFont.family()).arg(QFontInfo(baseFont).pixelSize())},
            {"@extraBig", qssifyFont(Style::getFont(Font::ExtraBig))},
            {"@big", qssifyFont(Style::getFont(Font::Big))},
            {"@bigBold", qssifyFont(Style::getFont(Font::BigBold))},
            {"@medium", qssifyFont(Style::getFont(Font::Medium))},
            {"@mediumBold", qssifyFont(Style::getFont(Font::MediumBold))},
            {"@small", qssifyFont(Style::getFont(Font::Small))},
            {"@smallLight", qssifyFont(Style::getFont(Font::SmallLight))},
        };
    }

    for (const QString& key : dictColor.keys()) {
        qss.replace(QRegularExpression(key % QLatin1String{"\\b"}), dictColor[key]);
    }

    for (const QString& key : dictFont.keys()) {
        qss.replace(QRegularExpression(key % QLatin1String{"\\b"}), dictFont[key]);
    }

    for (const QString& key : dictTheme.keys()) {
        qss.replace(QRegularExpression(key % QLatin1String{"\\b"}), dictTheme[key]);
    }

    // "@getImagePath()" function
    const QRegularExpression re{QStringLiteral(R"(@getImagePath\([^)\s]*\))")};
    QRegularExpressionMatchIterator i = re.globalMatch(qss);

    while (i.hasNext()) {
        const QRegularExpressionMatch match = i.next();
        QString path = match.captured(0);
        const QString phrase = path;

        path.remove(QStringLiteral("@getImagePath("));
        path.chop(1);

        QString fullImagePath = getThemeFolder(themeColor) + path;
        // image not in cache
        if (!existingImagesCache.contains(fullPath)) {
            if (QFileInfo::exists(fullImagePath)) {
                existingImagesCache << fullImagePath;
            } else {
                qWarning() << "Failed to open file (using defaults):" << fullImagePath;
                fullImagePath = getThemePath(themeColor) % path;
            }
        }

        qss.replace(phrase, fullImagePath);
    }

    return qss;
}

void Style::repolish(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);

    for (QObject* o : w->children()) {
        QWidget* c = qobject_cast<QWidget*>(o);
        if (c != nullptr) {
            c->style()->unpolish(c);
            c->style()->polish(c);
        }
    }
}

void Style::setThemeColor(int themeColor, int color)
{
    stylesheetsCache.clear(); // clear stylesheet cache which includes color info
    palette.clear();
    dictColor.clear();
    initPalette(themeColor);
    initDictColor();
    if (color < 0 || color >= themeNameColors.size()) {
        setThemeColor(QColor());
    } else {
        const auto& themeName = themeNameColors[color];

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        switch (themeName.type) {
        case MainTheme::Light:
            QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
            break;
        case MainTheme::Dark:
            QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
            break;
        }
#endif

        setThemeColor(themeName.color);
    }
}

/**
 * @brief Set theme color.
 * @param color Color to set.
 *
 * Pass an invalid QColor to reset to defaults.
 */
void Style::setThemeColor(const QColor& color)
{
    if (!color.isValid()) {
        // Reset to default
        palette[ColorPalette::ThemeDark] = getColor(ColorPalette::ThemeDark);
        palette[ColorPalette::ThemeMediumDark] = getColor(ColorPalette::ThemeMediumDark);
        palette[ColorPalette::ThemeMedium] = getColor(ColorPalette::ThemeMedium);
        palette[ColorPalette::ThemeLight] = getColor(ColorPalette::ThemeLight);
    } else {
        palette[ColorPalette::ThemeDark] = color.darker(155);
        palette[ColorPalette::ThemeMediumDark] = color.darker(135);
        palette[ColorPalette::ThemeMedium] = color.darker(120);
        palette[ColorPalette::ThemeLight] = color.lighter(110);
    }

    dictTheme["@themeDark"] = getColor(ColorPalette::ThemeDark).name();
    dictTheme["@themeMediumDark"] = getColor(ColorPalette::ThemeMediumDark).name();
    dictTheme["@themeMedium"] = getColor(ColorPalette::ThemeMedium).name();
    dictTheme["@themeLight"] = getColor(ColorPalette::ThemeLight).name();
}

/**
 * @brief Reloads the application theme and redraw the window.
 *
 * For reload theme need connect signal themeReload() to function for reload
 * For example: connect(&style, &Style::themeReload, this, &SomeClass::reloadTheme);
 */
void Style::applyTheme()
{
    emit themeReload();
}

QPixmap Style::scaleSvgImage(const QString& path, uint32_t width, uint32_t height)
{
    QSvgRenderer render(path);
    QPixmap pixmap(width, height);
    pixmap.fill(QColor(0, 0, 0, 0));
    QPainter painter(&pixmap);
    render.render(&painter, pixmap.rect());
    return pixmap;
}

void Style::initPalette(int themeColor)
{
    QSettings colourSettings(getThemePath(themeColor) % "palette.ini", QSettings::IniFormat);

    auto keys = aliasColors.keys();

    colourSettings.beginGroup("colors");
    QMap<ColorPalette, QString> c;
    for (auto k : keys) {
        c[k] = colourSettings.value(aliasColors[k], "#000").toString();
        palette[k] = QColor(colourSettings.value(aliasColors[k], "#000").toString());
    }
    auto p = palette;
    colourSettings.endGroup();
}

void Style::initDictColor()
{
    dictColor = {{"@transferGood", Style::getColor(ColorPalette::TransferGood).name()},
                 {"@transferWait", Style::getColor(ColorPalette::TransferWait).name()},
                 {"@transferBad", Style::getColor(ColorPalette::TransferBad).name()},
                 {"@transferMiddle", Style::getColor(ColorPalette::TransferMiddle).name()},
                 {"@mainText", Style::getColor(ColorPalette::MainText).name()},
                 {"@nameActive", Style::getColor(ColorPalette::NameActive).name()},
                 {"@statusActive", Style::getColor(ColorPalette::StatusActive).name()},
                 {"@groundExtra", Style::getColor(ColorPalette::GroundExtra).name()},
                 {"@groundBase", Style::getColor(ColorPalette::GroundBase).name()},
                 {"@orange", Style::getColor(ColorPalette::Orange).name()},
                 {"@yellow", Style::getColor(ColorPalette::Yellow).name()},
                 {"@action", Style::getColor(ColorPalette::Action).name()},
                 {"@link", Style::getColor(ColorPalette::Link).name()},
                 {"@searchHighlighted", Style::getColor(ColorPalette::SearchHighlighted).name()},
                 {"@selectText", Style::getColor(ColorPalette::SelectText).name()}};
}

QString Style::getThemePath(int themeColor)
{
    if (themeNameColors[themeColor].type == MainTheme::Dark) {
        return BuiltinThemeDarkPath;
    }

    return BuiltinThemeDefaultPath;
}
