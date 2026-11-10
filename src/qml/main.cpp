/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/qml/avatarprovider.h"
#include "src/qml/backend.h"
#include "src/qml/callvideoprovider.h"
#include "src/qml/previewprovider.h"
#include "src/qml/qmlapp.h"
#include "src/qml/qrprovider.h"
#include "src/qml/systeminsets.h"
#include "src/qml/systemnotifier.h"
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
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qcoreapplication_platform.h>
#include <cstring>
#include <iterator>
#include <span>
#include <tuple>
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

/**
 * @brief Opens the pages that tapped notifications ask for, and what other apps share.
 */
class Intents : public QtAndroidPrivate::NewIntentListener
{
public:
    explicit Intents(QmlApp& qtox_)
        : qtox{qtox_}
    {
        // A tap or a share that started the user interface.
        open(QJniObject{QNativeInterface::QAndroidApplication::context()}
                 .callObjectMethod("getIntent", "()Landroid/content/Intent;"));
        QtAndroidPrivate::registerNewIntentListener(this);
    }

    ~Intents() override
    {
        QtAndroidPrivate::unregisterNewIntentListener(this);
    }

    Intents(const Intents&) = delete;
    Intents& operator=(const Intents&) = delete;

    // On Android's main thread.
    bool handleNewIntent(JNIEnv* env, jobject intent) override
    {
        std::ignore = env;
        return open(QJniObject{intent});
    }

private:
    bool open(const QJniObject& intent)
    {
        const QString destination = ask("destination", intent);
        const QString file = ask("sharedFile", intent);
        const QString text = ask("sharedText", intent);
        if (!destination.isEmpty()) {
            QMetaObject::invokeMethod(
                &qtox, [&qtox = qtox, destination] { qtox.open(destination); }, Qt::QueuedConnection);
            return true;
        }
        if (!file.isEmpty() || !text.isEmpty()) {
            QMetaObject::invokeMethod(
                &qtox, [&qtox = qtox, file, text] { qtox.receiveShare(QUrl{file}, text); },
                Qt::QueuedConnection);
            return true;
        }
        return false;
    }

    static QString ask(const char* method, const QJniObject& intent)
    {
        return QJniObject::callStaticMethod<jstring>("io/github/qtox/qtox/QtoxActivity", method,
                                                     "(Landroid/content/Intent;)Ljava/lang/String;",
                                                     intent.object())
            .toString();
    }

    QmlApp& qtox;
};

// The service that notifications' actions go to, once there is one.
ToxService* notifiedService = nullptr;

// On Android's main thread: what the user wrote in a message's notification.
void nativeReply(JNIEnv* env, jclass cls, jstring tag, jstring text)
{
    std::ignore = env;
    std::ignore = cls;
    ToxService* service = notifiedService;
    if (service == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        service,
        [service, tag = QJniObject{tag}.toString(), text = QJniObject{text}.toString()] {
            service->reply(tag, text);
        },
        Qt::QueuedConnection);
}

// On Android's main thread: the user declined a call from its notification.
void nativeDecline(JNIEnv* env, jclass cls)
{
    std::ignore = env;
    std::ignore = cls;
    if (ToxService* service = notifiedService) {
        QMetaObject::invokeMethod(service, [service] { service->endCall(); }, Qt::QueuedConnection);
    }
}

void receiveNotificationActions(Backend& backend)
{
    notifiedService = &backend.getService();
    const JNINativeMethod methods[] = {
        {"nativeReply", "(Ljava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void*>(nativeReply)},
        {"nativeDecline", "()V", reinterpret_cast<void*>(nativeDecline)},
    };
    QJniEnvironment{}.registerNativeMethods("io/github/qtox/qtox/QtoxService", methods,
                                            std::size(methods));
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
        SystemNotifier notifier;
        Backend backend{serviceUrl(QStringLiteral("qtox")), notifier};
        receiveNotificationActions(backend);
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
    SystemNotifier notifier;
    const Backend backend{url, notifier};
    if (!backend.isListening()) {
        return EXIT_FAILURE;
    }
#endif

    QRemoteObjectNode node;
    node.connectToNode(url);
    QmlApp qtox(node);
    QmlAppForeign::instance = &qtox;
#ifdef Q_OS_ANDROID
    const Intents intents{qtox};
#endif

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
