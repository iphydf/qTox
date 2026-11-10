/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QMarginsF>
#include <QObject>

#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/**
 * @brief How far the system covers the window's edges beyond the safe area that Qt keeps clear of
 * its bars and the display's cutouts: the on-screen keyboard, which this follows as it opens and
 * closes.
 *
 * In device-independent pixels. Nothing covers a desktop window.
 */
class SystemInsets : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal left READ getLeft NOTIFY changed)
    Q_PROPERTY(qreal top READ getTop NOTIFY changed)
    Q_PROPERTY(qreal right READ getRight NOTIFY changed)
    Q_PROPERTY(qreal bottom READ getBottom NOTIFY changed)

public:
    /** QML's: there's one at a time. */
    explicit SystemInsets(QObject* parent = nullptr);
    ~SystemInsets() override;
    SystemInsets(const SystemInsets&) = delete;
    SystemInsets& operator=(const SystemInsets&) = delete;
    SystemInsets(SystemInsets&&) = delete;
    SystemInsets& operator=(SystemInsets&&) = delete;

    qreal getLeft() const;
    qreal getTop() const;
    qreal getRight() const;
    qreal getBottom() const;
    void set(const QMarginsF& newMargins);

signals:
    void changed();

private:
    QMarginsF margins;
};

/**
 * @brief Makes the one SystemInsets the QML singleton SystemInsets.
 */
struct SystemInsetsForeign
{
    Q_GADGET
    QML_FOREIGN(SystemInsets)
    QML_SINGLETON
    QML_NAMED_ELEMENT(SystemInsets)

public:
    static SystemInsets* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
};
