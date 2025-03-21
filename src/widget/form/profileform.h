/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#pragma once

#include "src/widget/qrwidget.h"

#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QVBoxLayout>

class ContentLayout;
class CroppingLabel;
class IProfileInfo;
class MaskablePixmapWidget;
class Settings;
class Style;
class IMessageBoxManager;

namespace Ui {
class IdentitySettings;
}
class ToxId;
class ClickableTE : public QLabel
{
    Q_OBJECT
public:
signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent* event) final
    {
        std::ignore = event;
        emit clicked();
    }
};

class ProfileForm : public QWidget
{
    Q_OBJECT
public:
    ProfileForm(IProfileInfo* profileInfo_, Settings& settings, Style& style,
                IMessageBoxManager& messageBoxManager, QWidget* parent = nullptr);
    ~ProfileForm() override;
    void show(ContentLayout* contentLayout);
    bool isShown() const;

public slots:
    void onSelfAvatarLoaded(const QPixmap& pic);
    void onLogoutClicked();

private slots:
    void setPasswordButtonsText();
    void setToxId(const ToxId& id);
    void copyIdClicked();
    void onUserNameEdited();
    void onStatusMessageEdited();
    void onRenameClicked();
    void onExportClicked();
    void onDeleteClicked();
    void onCopyQrClicked();
    void onSaveQrClicked();
    void onDeletePassClicked();
    void onChangePassClicked();
    void onAvatarClicked();
    void showProfilePictureContextMenu(const QPoint& point);

private:
    void retranslateUi();
    void prFileLabelUpdate();
    bool eventFilter(QObject* object, QEvent* event) override;
    void refreshProfiles();
    static QString getSupportedImageFilter();

private:
    Ui::IdentitySettings* bodyUI;
    MaskablePixmapWidget* profilePicture;
    QTimer timer;
    bool hasCheck = false;
    QRWidget* qr;
    ClickableTE* toxId;
    IProfileInfo* profileInfo;
    Settings& settings;
    IMessageBoxManager& messageBoxManager;
};
