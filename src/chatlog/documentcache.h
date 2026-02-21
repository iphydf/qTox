/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QStack>

class QTextDocument;
class SmileyPack;
class Settings;

class DocumentCache
{
public:
    DocumentCache(SmileyPack& smileyPack, Settings& settings);
    ~DocumentCache();
    DocumentCache(DocumentCache&) = delete;
    DocumentCache& operator=(const DocumentCache&) = delete;

    QTextDocument* pop();
    void push(QTextDocument* doc);

private:
    QStack<QTextDocument*> documents;
    SmileyPack& smileyPack;
    Settings& settings;
};
