/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/model/inotifier.h"

/**
 * @brief Android's notifications, which NotificationReceiver.java answers. Does nothing
 * elsewhere.
 *
 * Make one at a time: the user's answers go to the newest.
 */
class AndroidNotifier : public INotifier
{
public:
    AndroidNotifier();
    ~AndroidNotifier() override;
    AndroidNotifier(const AndroidNotifier&) = delete;
    AndroidNotifier& operator=(const AndroidNotifier&) = delete;
    AndroidNotifier(AndroidNotifier&&) = delete;
    AndroidNotifier& operator=(AndroidNotifier&&) = delete;

    void setListener(IListener* listener_) override;
    void show(const QString& tag, const NotificationData& data, Actions actions) override;
    void cancel(const QString& tag) override;
    void showStatus(const QString& status) override;
    void setRecording(bool microphone, bool camera) override;

    /** Who gets the user's answers now: called on Qt's thread. */
    static IListener* currentListener();

private:
    IListener* listener = nullptr;
};
