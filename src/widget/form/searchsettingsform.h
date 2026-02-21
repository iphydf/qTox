/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "src/widget/searchtypes.h"

#include <QWidget>

namespace Ui {
class SearchSettingsForm;
}
class Settings;
class Style;

class SearchSettingsForm : public QWidget
{
    Q_OBJECT

public:
    SearchSettingsForm(Settings& settings, Style& style, QWidget* parent = nullptr);
    ~SearchSettingsForm() override;

    ParameterSearch getParameterSearch();
    void reloadTheme();

private:
    Ui::SearchSettingsForm* ui;
    QDate startDate;
    bool isUpdate{false};
    Settings& settings;
    Style& style;

    void updateStartDateLabel();
    void setUpdate(bool isUpdate_);

private slots:
    void onStartSearchSelected(int index);
    void onRegisterClicked(bool checked);
    void onWordsOnlyClicked(bool checked);
    void onRegularClicked(bool checked);
    void onChoiceDate();

signals:
    void updateSettings(bool isUpdate);
};
