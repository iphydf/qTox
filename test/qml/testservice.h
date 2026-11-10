/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/qml/backend.h"
#include "src/qml/qmlapp.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRemoteObjectNode>
#include <QTemporaryDir>
#include <QUrl>

#include <memory>

class INotifier;

/**
 * @brief A Tox service with its own settings, and the user interface's view of it.
 *
 * Its profiles bootstrap from one node only, so tests stay off the Internet.
 */
class TestService
{
public:
    TestService(INotifier& notifier_, const QString& nodeKey, quint16 nodePort)
        : notifier{notifier_}
    {
        useBootstrapNode(nodeKey, nodePort);
        // Received files go to the home folder.
        qputenv("HOME", dir.path().toUtf8());
        start();
    }

    /** For the profiles loaded next. A TCP port makes it a TCP relay too. */
    void useBootstrapNode(const QString& key, quint16 udpPort, quint16 tcpPort = 0)
    {
        // The format of qTox's bootstrapNodes.json.
        const QJsonObject node{
            {"public_key", key},
            {"ipv4", "127.0.0.1"},
            {"ipv6", "-"},
            {"port", udpPort},
            {"status_udp", true},
            {"status_tcp", tcpPort != 0},
            {"tcp_ports", tcpPort != 0 ? QJsonArray{tcpPort} : QJsonArray{}},
            {"maintainer", "test"},
        };
        QFile nodes{dir.filePath("bootstrapNodes.json")};
        if (!nodes.open(QIODevice::WriteOnly)) {
            qFatal("Can't write %s", qPrintable(nodes.fileName()));
        }
        nodes.write(QJsonDocument{QJsonObject{{"nodes", QJsonArray{node}}}}.toJson());
    }

    /** As when Android restarts the service and the user interface. */
    void restart()
    {
        qtox.reset();
        client.reset();
        backend.reset();
        start();
    }

    QUrl url() const
    {
        return QUrl{QStringLiteral("local:%1/qtox").arg(dir.path())};
    }

    QTemporaryDir dir;
    std::unique_ptr<Backend> backend;
    std::unique_ptr<QRemoteObjectNode> client;
    std::unique_ptr<QmlApp> qtox;

private:
    void start()
    {
        backend = std::make_unique<Backend>(url(), notifier, dir.path());
        // As main() does it: before the user interface asks for what the service shares.
        client = std::make_unique<QRemoteObjectNode>();
        client->connectToNode(url());
        qtox = std::make_unique<QmlApp>(*client);
    }

    INotifier& notifier;
};
