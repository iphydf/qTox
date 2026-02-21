/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2020 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QPixmap>
#include <QPushButton>
#include <QString>

class ImagePreviewButton : public QPushButton
{
public:
    explicit ImagePreviewButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
    }
    ~ImagePreviewButton() override;
    ImagePreviewButton(const ImagePreviewButton&) = delete;
    ImagePreviewButton& operator=(const ImagePreviewButton&) = delete;
    ImagePreviewButton(ImagePreviewButton&&) = delete;
    ImagePreviewButton& operator=(ImagePreviewButton&&) = delete;

    void setIconFromFile(const QString& filename);
    void setIconFromPixmap(const QPixmap& pixmap);

private:
    void initialize(const QPixmap& image);
};
