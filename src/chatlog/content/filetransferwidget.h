/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#pragma once

#include "src/core/toxfile.h"

#include <QTime>
#include <QWidget>

class CoreFile;

namespace Ui {
class FileTransferWidget;
}

class QVariantAnimation;
class QPushButton;
class Settings;
class Style;
class IMessageBoxManager;

class FileTransferWidget : public QWidget
{
    Q_OBJECT

public:
    FileTransferWidget(QWidget* parent, CoreFile& _coreFile, ToxFile file, Settings& settings,
                       Style& style, IMessageBoxManager& messageBoxManager);
    ~FileTransferWidget() override;
    bool isActive() const;
    void onFileTransferUpdate(ToxFile file);

protected:
    void updateWidgetColor(const ToxFile& file);
    void updateWidgetText(const ToxFile& file);
    void updateFileProgress(const ToxFile& file);
    void updateSignals(const ToxFile& file);
    void updatePreview(const ToxFile& file);
    void setupButtons(const ToxFile& file);
    void handleButton(QPushButton* btn);
    void showPreview(const QString& filename);
    void acceptTransfer(const QString& filepath);
    void setBackgroundColor(const QColor& c, bool whiteFont);
    void setButtonColor(const QColor& c);

    bool drawButtonAreaNeeded() const;

    void paintEvent(QPaintEvent* event) final;

public slots:
    void reloadTheme();

private slots:
    void onLeftButtonClicked();
    void onRightButtonClicked();
    void onPreviewButtonClicked();

private:
    static bool tryRemoveFile(const QString& filepath);

    void updateWidget(const ToxFile& file);
    void updateBackgroundColor(ToxFile::FileStatus status);

private:
    CoreFile& coreFile;
    Ui::FileTransferWidget* ui;
    ToxFile fileInfo;
    QVariantAnimation* backgroundColorAnimation = nullptr;
    QVariantAnimation* buttonColorAnimation = nullptr;
    QColor backgroundColor;
    QColor buttonColor;
    QColor buttonBackgroundColor;

    bool active;
    QTime lastTransmissionUpdate;
    ToxFile::FileStatus lastStatus = ToxFile::INITIALIZING;
    Settings& settings;
    Style& style;
    IMessageBoxManager& messageBoxManager;
};
