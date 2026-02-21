/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "src/core/toxfile.h"
#include "src/widget/form/filetransferlist.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QLabel>
#include <QListWidgetItem>
#include <QString>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

class ContentLayout;
class CoreFile;
class FriendList;
class IMessageBoxManager;
class QFileInfo;
class QTableView;
class Settings;
class Style;

class FilesForm : public QObject
{
    Q_OBJECT

public:
    FilesForm(CoreFile& coreFile, Settings& settings, Style& style,
              IMessageBoxManager& messageBoxManager, FriendList& friendList);
    ~FilesForm() override;

    bool isShown() const;
    void show(ContentLayout* contentLayout);

public slots:
    void onFileUpdated(const ToxFile& file);

private slots:
    void onSentFileActivated(const QModelIndex& index);
    void onReceivedFileActivated(const QModelIndex& index);

private:
    struct FileInfo
    {
        QListWidgetItem* item = nullptr;
        ToxFile file;
    };

    void retranslateUi();

    QWidget* head;
    QLabel headLabel;
    QVBoxLayout headLayout;
    QTabWidget main;
    QTableView *sent, *recvd;
    FileTransferList::Model *sentModel, *recvdModel;
    IMessageBoxManager& messageBoxManager;
};
