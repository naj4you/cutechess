#include <QtTest/QTest>
#include <mersenne.h>

class tst_Mersenne: public QObject
{
	Q_OBJECT

	private slots:
		void numbers_data();
		void numbers();
		void reseed();
};

void tst_Mersenne::numbers_data()
{
	QTest::addColumn<quint32>("seed");
	QTest::addColumn<quint32>("random1");
	QTest::addColumn<quint32>("random2");

	QTest::newRow("0")
		<< quint32(0U)
		<< quint32(2357136044U)
		<< quint32(1745961492U);

	QTest::newRow("0xFFFFFFFF")
		<< quint32(0xFFFFFFFFU)
		<< quint32(2720109276U)
		<< quint32(3792022478U);
}

void tst_Mersenne::numbers()
{
	QFETCH(quint32, seed);
	QFETCH(quint32, random1);
	QFETCH(quint32, random2);

	Mersenne::initialize(seed);
	QCOMPARE(Mersenne::random(), random1);
	QCOMPARE(Mersenne::random(), random2);
}

void tst_Mersenne::reseed()
{
	// The same seed must always give the same sequence,
	// regardless of how many numbers were drawn before
	Mersenne::initialize(12345);
	QList<quint32> first;
	for (int i = 0; i < 1000; i++)
		first << Mersenne::random();

	Mersenne::initialize(12345);
	for (int i = 0; i < 1000; i++)
		QCOMPARE(Mersenne::random(), first.at(i));
}


QTEST_MAIN(tst_Mersenne)
#include "tst_mersenne.moc"
