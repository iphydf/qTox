/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

class QString;
struct NotificationData;

/**
 * @brief Shows and removes the system's notifications.
 */
class INotifier
{
public:
    /** Gets what the user does in the notifications. */
    class IListener
    {
    public:
        IListener() = default;
        virtual ~IListener();
        IListener(const IListener&) = default;
        IListener& operator=(const IListener&) = default;
        IListener(IListener&&) = default;
        IListener& operator=(IListener&&) = default;

        /** What the user wrote in a notification that let them reply. */
        virtual void replied(const QString& tag, const QString& text) = 0;
        /** The user declined the call that a notification is about. */
        virtual void declined(const QString& tag) = 0;
        /** The user opened a notification. */
        virtual void opened() = 0;
    };

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
        // The listener gets what they wrote.
        Reply = 1,
        // A call's: answering opens qTox, declining ends it.
        AnswerOrDecline = 2,
    };

    /** Who gets what the user does, or nullptr. */
    virtual void setListener(IListener* listener) = 0;
    /** Replaces the notification with the same @p tag, if there is one. */
    virtual void show(const QString& tag, const NotificationData& data, Actions actions) = 0;
    virtual void cancel(const QString& tag) = 0;
    /** What the ongoing notification, which keeps Tox running, says. */
    virtual void showStatus(const QString& status) = 0;
    /** Whether Tox records sound and video: Android lets it only while it says so. */
    virtual void setRecording(bool microphone, bool camera) = 0;
};
