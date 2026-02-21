/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "genericsettings.h"

#include <QUrl>

class QTimer;
class QString;
class UpdateCheck;
class QLayoutItem;
class Style;

namespace Ui {
class AboutSettings;
}

class AboutForm : public GenericForm
{
    Q_OBJECT
public:
    AboutForm(UpdateCheck& updateCheck_, QString contactInfo_, Style& style_);
    ~AboutForm() override;
    QString getFormName() final
    {
        return tr("About");
    }

public slots:
    void onUpdateAvailable(QString latestVersion, QUrl link);
    void onUpToDate();
    void onUpdateCheckFailed();
    void reloadTheme() override;
    void onUnstableVersion();

private:
    void retranslateUi();
    void replaceVersions();
    QString createLink(QString path, QString text) const;

private:
    Ui::AboutSettings* bodyUI;
    QTimer* progressTimer;
    UpdateCheck& updateCheck;
    QString contactInfo;
    QMetaObject::Connection linkConnection;
    Style& style;
};
