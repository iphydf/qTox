/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2024-2026 The TokTok team.
 */

#include "src/translator.h"

#include <QDirIterator>
#include <QTest>

class TestTranslator : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void testLocales();
    void testLanguageNames();
};

void TestTranslator::initTestCase()
{
    // As the app does: a static build only links the resource if asked to.
    Q_INIT_RESOURCE(translations);
}

void TestTranslator::testLocales()
{
    QDirIterator it(":/translations", QDirIterator::Subdirectories);
    QStringList translations;
    while (it.hasNext()) {
        it.next();
        translations << it.fileName();
    }
    QVERIFY(!translations.isEmpty());

    const QStringList locales = Translator::getLocales();
    QVERIFY(!locales.isEmpty());

    // check if all locales are present in the translations
    for (const QString& locale : locales) {
        QVERIFY2(translations.contains(locale + ".qm"), qPrintable(locale + ".qm not found"));
    }

    // check if all translations are present in the locales
    for (const QString& translation : translations) {
        QVERIFY2(locales.contains(translation.left(translation.size() - 3)),
                 qPrintable(translation.left(translation.size() - 3) + " not found"));
    }
}

void TestTranslator::testLanguageNames()
{
    QCOMPARE(Translator::getLanguageName(QStringLiteral("de")), QStringLiteral("Deutsch"));
    QCOMPARE(Translator::getLanguageName(QStringLiteral("jbo")), QStringLiteral("Lojban"));
    QCOMPARE(Translator::getLanguageName(QStringLiteral("pr")), QStringLiteral("Pirate"));

    for (const QString& locale : Translator::getLocales()) {
        QVERIFY2(!Translator::getLanguageName(locale).isEmpty(), qPrintable(locale));
    }
}

QTEST_GUILESS_MAIN(TestTranslator)
#include "translator_test.moc"
