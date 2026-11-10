/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QObject>

#include <memory>

class QUrl;

/**
 * @brief What Android asks the user interface for: to open a notification's destination, or to
 * take what another app shared. Does nothing elsewhere.
 *
 * Starts with the intent that started the app, so connect to it right away.
 */
class AndroidIntents : public QObject
{
    Q_OBJECT

public:
    explicit AndroidIntents(QObject* parent = nullptr);
    ~AndroidIntents() override;
    AndroidIntents(const AndroidIntents&) = delete;
    AndroidIntents& operator=(const AndroidIntents&) = delete;
    AndroidIntents(AndroidIntents&&) = delete;
    AndroidIntents& operator=(AndroidIntents&&) = delete;

signals:
    /** Open @p destination: a destination.h name. */
    void openRequested(const QString& destination);
    /** Another app shared a file or text. */
    void shareReceived(const QUrl& file, const QString& text);

private:
    struct Listener;
    std::unique_ptr<Listener> listener;
};
