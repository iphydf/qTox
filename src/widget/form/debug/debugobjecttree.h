/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2024-2026 The TokTok team.
 */

#ifndef DEBUGOBJECTTREE_H
#define DEBUGOBJECTTREE_H

#include "src/widget/form/settings/genericsettings.h"

#include <memory>

class DebugObjectTreeModel;
class Style;

namespace Ui {
class DebugObjectTree;
}

class DebugObjectTree : public GenericForm
{
    Q_OBJECT

public:
    explicit DebugObjectTree(Style& style, QWidget* parent = nullptr);
    ~DebugObjectTree() override;

    QString getFormName() final
    {
        // Not translated (for now). Debugging only.
        return QStringLiteral("Object Tree");
    }

private:
    std::unique_ptr<Ui::DebugObjectTree> ui_;
    DebugObjectTreeModel* model_;
};

#endif // DEBUGOBJECTTREE_H
