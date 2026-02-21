/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QDialog>

#include <memory>

namespace Ui {
class RemoveChatDialog;
}

class Chat;

class RemoveChatDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit RemoveChatDialog(QWidget* parent, const Chat& contact);
    ~RemoveChatDialog() override;

    bool removeHistory() const;

    bool accepted() const
    {
        return _accepted;
    }

public slots:
    void onAccepted();

protected:
    std::unique_ptr<Ui::RemoveChatDialog> ui;
    bool _accepted = false;
};
