/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QProcess>
#include <QTest>

#include <algorithm>

class TestLint : public QObject
{
    Q_OBJECT

private slots:
    void untrustedText();
};

/** qmllint with tools/qmllint's plugin warns of the cases' lines that say so, and of no others. */
void TestLint::untrustedText()
{
    QList<int> unsafe;
    QFile cases{QStringLiteral(LINT_CASES)};
    QVERIFY(cases.open(QIODevice::ReadOnly));
    for (int line = 1; !cases.atEnd(); ++line) {
        if (cases.readLine().trimmed().endsWith("// HTML")) {
            unsafe += line;
        }
    }
    QVERIFY(!unsafe.isEmpty());

    // With the QML modules of QML_IMPORT_PATH, where Qt's aren't in one place.
    QProcess lint;
    lint.start(QStringLiteral(LINT_QMLLINT),
               {QStringLiteral("-E"), QStringLiteral("--plugin-paths"), QStringLiteral(LINT_PLUGINS),
                QStringLiteral("--json"), QStringLiteral("-"), cases.fileName()});
    QVERIFY(lint.waitForFinished());
    const QJsonDocument found = QJsonDocument::fromJson(lint.readAllStandardOutput());
    QList<int> warned;
    for (const QJsonValue warning : found["files"][0]["warnings"].toArray()) {
        if (warning["id"].toString() == QStringLiteral("Plugin.qTox.untrusted-text")) {
            warned += warning["line"].toInt();
        }
    }
    std::sort(warned.begin(), warned.end());
    QCOMPARE(warned, unsafe);
}

QTEST_GUILESS_MAIN(TestLint)
#include "lint_test.moc"
