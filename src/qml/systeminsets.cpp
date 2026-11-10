/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "systeminsets.h"

#include <QJSEngine>
#include <QQmlEngine>

#ifdef Q_OS_ANDROID
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif

#include <tuple>

namespace {
SystemInsets* instance = nullptr;

#ifdef Q_OS_ANDROID
// From Android's main thread, in pixels.
void insetsChanged(JNIEnv* env, jclass cls, jint left, jint top, jint right, jint bottom)
{
    std::ignore = env;
    std::ignore = cls;
    SystemInsets* receiver = instance;
    QMetaObject::invokeMethod(
        receiver,
        [receiver, left, top, right, bottom] {
            receiver->set(QMarginsF(left, top, right, bottom) / qGuiApp->devicePixelRatio());
        },
        Qt::QueuedConnection);
}
#endif
} // namespace

SystemInsets::SystemInsets(QObject* parent)
    : QObject(parent)
{
    instance = this;
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    env.registerNativeMethods("io/github/qtox/qtox/QtoxActivity",
                              {{"insetsChanged", "(IIII)V", reinterpret_cast<void*>(insetsChanged)}});
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxActivity", "sendInsets", "(Z)V",
                                       jboolean{true});
#endif
}

SystemInsets::~SystemInsets()
{
#ifdef Q_OS_ANDROID
    // No calls after it returns.
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxActivity", "sendInsets", "(Z)V",
                                       jboolean{false});
#endif
    if (instance == this) {
        instance = nullptr;
    }
}

SystemInsets* SystemInsetsForeign::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    std::ignore = qmlEngine;
    std::ignore = jsEngine;
    // The app owns it, not QML.
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

qreal SystemInsets::getLeft() const
{
    return margins.left();
}

qreal SystemInsets::getTop() const
{
    return margins.top();
}

qreal SystemInsets::getRight() const
{
    return margins.right();
}

qreal SystemInsets::getBottom() const
{
    return margins.bottom();
}

void SystemInsets::set(const QMarginsF& newMargins)
{
    if (margins != newMargins) {
        margins = newMargins;
        emit changed();
    }
}
