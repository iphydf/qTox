/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "src/widget/form/settings/genericsettings.h"

#include <memory>

class DebugLogModel;
class Paths;
class QTimer;
class Style;

namespace Ui {
class DebugLog;
}

class DebugLogForm final : public GenericForm
{
    Q_OBJECT
public:
    DebugLogForm(Paths& paths, Style& style, QWidget* parent);
    ~DebugLogForm() override;
    QString getFormName() final
    {
        return tr("Debug Log");
    }

protected:
    void showEvent(QShowEvent* event) final;

private:
    void retranslateUi();

private:
    Paths& paths_;
    std::unique_ptr<Ui::DebugLog> ui_;
    std::unique_ptr<DebugLogModel> debugLogModel_;
    std::unique_ptr<QTimer> reloadTimer_;
};
