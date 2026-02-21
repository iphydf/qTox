/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "textcompose.h"

#include "src/widget/style.h"

#include <QString>
#include <QUrl>

QString TextCompose::urlEncode(const QString& str)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(str));
}

QString TextCompose::createLink(Style& style, QString path, QString text)
{
    return QStringLiteral("<a href=\"%1\" style=\"text-decoration: underline; color:%2;\">%3</a>")
        .arg(path, style.getColor(Style::ColorPalette::Link).name(), text);
}
