/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QAction>
#include <QLineEdit>

#include <memory>

class PasswordEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit PasswordEdit(QWidget* parent);
    ~PasswordEdit() override;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    class EventHandler : QObject
    {
    public:
        QVector<QAction*> actions;

        EventHandler();
        ~EventHandler() override;
        void updateActions();
        bool eventFilter(QObject* obj, QEvent* event) override;
    };

    void registerHandler();
    void unregisterHandler();

private:
    QAction* action;

    static std::unique_ptr<EventHandler> eventHandler;
};
