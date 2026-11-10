/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/platform/androidintents.h"
#include "src/platform/androidnotifier.h"
#include "src/qml/avatarprovider.h"
#include "src/qml/backend.h"
#include "src/qml/callvideoprovider.h"
#include "src/qml/previewprovider.h"
#include "src/qml/qmlapp.h"
#include "src/qml/qrprovider.h"
#include "src/qml/systeminsetsforeign.h"
#include "src/qml/toxservice.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QRemoteObjectNode>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QJniObject>

#include <QtCore/private/qandroidextras_p.h>
#include <QtCore/qcoreapplication_platform.h>
#include <cstring>
#include <span>
#endif

namespace {
/**
 * @brief Where the service listens: a socket only this user, or on Android only qTox, can reach.
 *
 * Android's abstract sockets would be open to every app.
 */
QUrl serviceUrl(const QString& name)
{
    return QUrl{QStringLiteral("local:%1/%2")
                    .arg(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation), name)};
}

#ifdef Q_OS_ANDROID

// The service's android.app.arguments in AndroidManifest.xml.
bool isService(int argc, char* argv[])
{
    const std::span args(argv, argc);
    return std::ranges::any_of(args.subspan(1),
                               [](const char* arg) { return std::strcmp(arg, "--service") == 0; });
}

void startService()
{
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxService", "start",
                                       "(Landroid/app/Activity;)V",
                                       QNativeInterface::QAndroidApplication::context().object());
}
#endif
} // namespace

// The QML module is a static library's.
Q_IMPORT_QML_PLUGIN(qToxPlugin)

int main(int argc, char* argv[])
{
#ifdef Q_OS_ANDROID
    if (isService(argc, argv)) {
        // Without the GUI: Qt's Android GUI needs an activity, and services have none. Qt's
        // QtService waits in onCreate() until a QAndroidService exists.
        QAndroidService app(argc, argv);
        QCoreApplication::setApplicationName("qTox");
        Q_INIT_RESOURCE(translations);
        AndroidNotifier notifier;
        Backend backend{serviceUrl(QStringLiteral("qtox")), notifier};
        return backend.isListening() ? QCoreApplication::exec() : EXIT_FAILURE;
    }
#endif

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("qTox");
    QGuiApplication::setDesktopFileName("io.github.qtox.qTox");
    QQuickStyle::setStyle("Material");

    // The translations live in a static library, which links the resource
    // only if asked to.
    Q_INIT_RESOURCE(translations);

#ifdef Q_OS_ANDROID
    // The service keeps running when the user interface closes.
    startService();
    const QUrl url = serviceUrl(QStringLiteral("qtox"));
#else
    const QUrl url = serviceUrl(QStringLiteral("qtox-%1").arg(QCoreApplication::applicationPid()));
    AndroidNotifier notifier;
    const Backend backend{url, notifier};
    if (!backend.isListening()) {
        return EXIT_FAILURE;
    }
#endif

    QRemoteObjectNode node;
    node.connectToNode(url);
    QmlApp qtox(node);
    QmlAppForeign::instance = &qtox;
    // The pages that tapped notifications ask for, and what other apps share.
    const AndroidIntents intents;
    QObject::connect(&intents, &AndroidIntents::openRequested, &qtox, &QmlApp::open);
    QObject::connect(&intents, &AndroidIntents::shareReceived, &qtox, &QmlApp::receiveShare);

    const SystemInsets systemInsets;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("avatar"), new AvatarProvider{qtox});
    engine.addImageProvider(QStringLiteral("qr"), new QrProvider);
    engine.addImageProvider(QStringLiteral("preview"), new PreviewProvider);
    engine.addImageProvider(QStringLiteral("callvideo"), new CallVideoProvider{qtox});
    QObject::connect(&qtox, &QmlApp::translationChanged, &engine, &QQmlEngine::retranslate);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("qTox", "Main");

    return QGuiApplication::exec();
}
