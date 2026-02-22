#include <QtTest>
#include "YaycUtilities.h"

class TestYayc : public QObject
{
    Q_OBJECT

private slots:
    void compareSemver_data();
    void compareSemver();
};

void TestYayc::compareSemver_data()
{
    QTest::addColumn<QString>("v1");
    QTest::addColumn<QString>("v2");
    QTest::addColumn<int>("expected");

    QTest::newRow("equal")          << "1.0.0"  << "1.0.0"  <<  0;
    QTest::newRow("major less")     << "1.0.0"  << "2.0.0"  << -1;
    QTest::newRow("major greater")  << "3.0.0"  << "2.0.0"  <<  1;
    QTest::newRow("minor less")     << "1.2.0"  << "1.3.0"  << -1;
    QTest::newRow("minor greater")  << "1.4.0"  << "1.3.0"  <<  1;
    QTest::newRow("patch less")     << "1.0.1"  << "1.0.2"  << -1;
    QTest::newRow("patch greater")  << "1.0.3"  << "1.0.2"  <<  1;
    QTest::newRow("two segments")   << "1.4"    << "1.3"    <<  1;
    QTest::newRow("empty vs ver")   << ""       << "1.0.0"  << -1;
    QTest::newRow("ver vs empty")   << "1.0.0"  << ""       <<  1;
    QTest::newRow("both empty")     << ""       << ""       <<  0;
}

void TestYayc::compareSemver()
{
    QFETCH(QString, v1);
    QFETCH(QString, v2);
    QFETCH(int, expected);

    YaycUtilities utils;
    QCOMPARE(utils.compareSemver(v1, v2), expected);
}

QTEST_MAIN(TestYayc)
#include "tst_yayc.moc"
