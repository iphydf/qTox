/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2024-2026 The TokTok team.
 */

#include "src/widget/form/settings/generalform.h"

#include <QDirIterator>
#include <QTest>

class TestGeneralForm : public QObject
{
    Q_OBJECT
private slots:
    void testLocales();
};

void TestGeneralForm::testLocales()
{
    QDirIterator it(":/translations", QDirIterator::Subdirectories);
    QStringList translations;
    while (it.hasNext()) {
        it.next();
        translations << it.fileName();
    }
    QVERIFY(!translations.isEmpty());

    const QStringList locales = GeneralForm::getLocales();
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

QTEST_GUILESS_MAIN(TestGeneralForm)
#include "generalform_test.moc"
