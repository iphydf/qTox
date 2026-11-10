/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

class QImage;
class QString;

/**
 * @brief Shows and removes the Tox service's notifications.
 */
class INotifier
{
public:
    INotifier() = default;
    virtual ~INotifier();
    INotifier(const INotifier&) = default;
    INotifier& operator=(const INotifier&) = default;
    INotifier(INotifier&&) = default;
    INotifier& operator=(INotifier&&) = default;

    /** What the user can do from a notification, besides opening it. Java has the same numbers. */
    enum class Actions
    {
        None = 0,
        // ToxService::reply() gets what they wrote.
        Reply = 1,
        // A call's: answering opens qTox, declining ends it.
        AnswerOrDecline = 2,
    };

    /** Replaces the notification with the same @p tag, if there is one. */
    virtual void show(const QString& tag, const QString& title, const QString& text,
                      const QImage& icon, Actions actions) = 0;
    virtual void cancel(const QString& tag) = 0;
    /** What the ongoing notification, which keeps the service running, says. */
    virtual void showStatus(const QString& status) = 0;
    /** Whether the service records sound and video: Android lets it only while it says so. */
    virtual void setRecording(bool microphone, bool camera) = 0;
};
