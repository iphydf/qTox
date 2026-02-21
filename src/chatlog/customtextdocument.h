/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QList>
#include <QTextDocument>

#include <memory>

class QIcon;
class SmileyPack;
class Settings;

class CustomTextDocument : public QTextDocument
{
    Q_OBJECT
public:
    CustomTextDocument(SmileyPack& smileyPack, Settings& settings, QObject* parent = nullptr);

protected:
    QVariant loadResource(int type, const QUrl& name) override;

private:
    QList<std::shared_ptr<QIcon>> emoticonIcons;
    SmileyPack& smileyPack;
    Settings& settings;
};
