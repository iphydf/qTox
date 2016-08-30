#include "src/core/core.h"
#include "src/core/toxstring.h"

#include <QtTest>

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void testSanitize() const;
};


void TestCore::testSanitize() const
{
    QCOMPARE(
        Core::sanitize("hello"),
        QString("hello"));
}


QTEST_MAIN(TestCore)
#include "core_test.moc"
