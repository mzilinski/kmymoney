/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDate>
#include <QElapsedTimer>
#include <QLocale>
#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <vector>

#include "journalmodel.h"
#include "ledgersearchfilter.h"
#include "mymoneyenums.h"
#include "mymoneyfile.h"
#include "mymoneymoney.h"
#include "mymoneypayee.h"
#include "mymoneysplit.h"
#include "testutilities.h"

using namespace test;

class LedgerSearchFilterTest : public QObject
{
    Q_OBJECT

    MyMoneyFile* file = nullptr;
    QString m_acChecking;
    QString m_acSavings;
    QString m_acGroceries;

    static QString withdrawal()
    {
        return MyMoneySplit::actionName(eMyMoney::Split::Action::Withdrawal);
    }

    static QString transfer()
    {
        return MyMoneySplit::actionName(eMyMoney::Split::Action::Transfer);
    }

    std::unique_ptr<LedgerSearchFilter> makeFilter()
    {
        auto filter = std::make_unique<LedgerSearchFilter>(nullptr);
        filter->setSourceModel(file->journalModel());
        return filter;
    }

private Q_SLOTS:
    void initTestCase()
    {
        // keep the formatted-amount expectations independent of the
        // environment when the binary is run outside of ctest
        QLocale::setDefault(QLocale::c());
    }

    void init()
    {
        file = MyMoneyFile::instance();
        file->unload();
        makeBaseCurrency();

        MyMoneyFileTransaction ft;
        // TransactionHelper resolves payees by name and silently creates
        // payee-less splits for unknown names, so add the payee up front
        MyMoneyPayee payee;
        payee.setName("Test Payee");
        file->addPayee(payee);
        ft.commit();

        m_acChecking = makeAccount(QString("Checking Account"), eMyMoney::Account::Type::Checkings, moCheckingOpen, QDate(2025, 1, 1), file->asset().id());
        m_acSavings = makeAccount(QString("Savings Account"), eMyMoney::Account::Type::Savings, moCheckingOpen, QDate(2025, 1, 1), file->asset().id());
        m_acGroceries = makeAccount(QString("Groceries"), eMyMoney::Account::Type::Expense, MyMoneyMoney(), QDate(2025, 1, 1), file->expense().id());
    }

    void cleanup()
    {
        file->unload();
    }

    void testEmptyFilterShowsNothing()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();

        QVERIFY(file->journalModel()->rowCount() > 0);
        QCOMPARE(filter->rowCount(), 0);
    }

    void testDedupOneRowPerTransaction()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();

        filter->setFilterFixedString(QStringLiteral("Groceries"));
        QVERIFY(file->journalModel()->rowCount() >= 2);
        QCOMPARE(filter->rowCount(), 1);
        // the representative row is the asset side
        QCOMPARE(filter->index(0, 0).data(eMyMoney::Model::SplitAccountIdRole).toString(), m_acChecking);
    }

    void testMatchesPayee()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();

        filter->setFilterFixedString(QStringLiteral("Test Payee"));
        QCOMPARE(filter->rowCount(), 1);
    }

    void testMatchesMemo()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        auto split = t.splits().at(0);
        split.setMemo(QStringLiteral("weekly shopping"));
        t.modifySplit(split);
        t.update();
        const auto filter = makeFilter();

        filter->setFilterFixedString(QStringLiteral("weekly shop"));
        QCOMPARE(filter->rowCount(), 1);
    }

    void testMatchesCategoryName()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();

        filter->setFilterFixedString(QStringLiteral("Grocer"));
        QCOMPARE(filter->rowCount(), 1);
    }

    void testMatchesAmount()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(23412, 100), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();

        // C locale enforced in initTestCase, so the formatted value is stable
        filter->setFilterFixedString(QStringLiteral("234.12"));
        QCOMPARE(filter->rowCount(), 1);
    }

    void testTransferAppearsOnce()
    {
        TransactionHelper t(QDate(2025, 2, 1), transfer(), MyMoneyMoney(100, 1), m_acChecking, m_acSavings);
        const auto filter = makeFilter();

        filter->setFilterFixedString(QStringLiteral("Test Payee"));
        QVERIFY(file->journalModel()->rowCount() >= 2);
        QCOMPARE(filter->rowCount(), 1);
    }

    void testNoMatchThenClear()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();
        QSignalSpy spy(filter.get(), &LedgerSearchFilter::filterApplied);

        filter->setFilterFixedString(QStringLiteral("xyzzy42"));
        QCOMPARE(filter->rowCount(), 0);
        QCOMPARE(spy.count(), 1);

        filter->setFilterFixedString(QString());
        QCOMPARE(filter->rowCount(), 0);
        QCOMPARE(spy.count(), 2);
    }

    void testLiveUpdate()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();

        filter->setFilterFixedString(QStringLiteral("Test Payee"));
        QCOMPARE(filter->rowCount(), 1);

        {
            TransactionHelper t2(QDate(2025, 3, 1), withdrawal(), MyMoneyMoney(12, 1), m_acChecking, m_acGroceries);
            QCOMPARE(filter->rowCount(), 2);
        }
        // the helper's destructor removed the second transaction again
        QCOMPARE(filter->rowCount(), 1);
    }

    void testSuspendResume()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();
        QSignalSpy spy(filter.get(), &LedgerSearchFilter::filterApplied);

        filter->setFilterFixedString(QStringLiteral("Test Payee"));
        QCOMPARE(filter->rowCount(), 1);

        filter->setSuspended(true);
        QCOMPARE(filter->rowCount(), 0);

        filter->setSuspended(false);
        QCOMPARE(filter->rowCount(), 1);
        QCOMPARE(spy.count(), 2); // the expression and the resume
    }

    void testFilterWhileSuspended()
    {
        // reachable in production: the debounce timer outlives the
        // dialog's Hide event
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();
        QSignalSpy spy(filter.get(), &LedgerSearchFilter::filterApplied);

        filter->setSuspended(true);
        filter->setFilterFixedString(QStringLiteral("Test Payee"));
        QCOMPARE(filter->rowCount(), 0);
        QCOMPARE(spy.count(), 1);

        filter->setSuspended(false);
        QCOMPARE(filter->rowCount(), 1);
        QCOMPARE(spy.count(), 2);
    }

    void testLiveModify()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();

        filter->setFilterFixedString(QStringLiteral("weekly shop"));
        QCOMPARE(filter->rowCount(), 0);

        // modifying under an active filter exercises the journal model's
        // remove+insert modification path against the cache hooks
        auto split = t.splits().at(0);
        split.setMemo(QStringLiteral("weekly shopping"));
        t.modifySplit(split);
        t.update();
        QCOMPARE(filter->rowCount(), 1);

        split.setMemo(QStringLiteral("something else"));
        t.modifySplit(split);
        t.update();
        QCOMPARE(filter->rowCount(), 0);
    }

    void testModelReset()
    {
        TransactionHelper t(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        const auto filter = makeFilter();

        filter->setFilterFixedString(QStringLiteral("Test Payee"));
        QCOMPARE(filter->rowCount(), 1);

        // unloading the file resets the journal model under the live filter
        file->unload();
        QCOMPARE(filter->rowCount(), 0);

        // rebuild the fixture: the still-set expression matches again
        init();
        TransactionHelper t2(QDate(2025, 2, 1), withdrawal(), MyMoneyMoney(42, 1), m_acChecking, m_acGroceries);
        QCOMPARE(filter->rowCount(), 1);
    }

    void testScanTiming()
    {
        // worst case scan: every transaction takes the deep evaluation
        // path because nothing matches
        constexpr int transactionCount = 2000;
        std::vector<std::unique_ptr<TransactionHelper>> transactions;
        transactions.reserve(transactionCount);
        for (int i = 0; i < transactionCount; ++i) {
            transactions.emplace_back(
                std::make_unique<TransactionHelper>(QDate(2025, 1, 1).addDays(i % 365), withdrawal(), MyMoneyMoney(i + 1, 1), m_acChecking, m_acGroceries));
        }
        const auto filter = makeFilter();
        const auto rows = file->journalModel()->rowCount();
        QVERIFY(rows >= 2 * transactionCount);

        // the proxy maps lazily, so the row count query is part of the scan
        QElapsedTimer timer;
        timer.start();
        filter->setFilterFixedString(QStringLiteral("xyzzy42"));
        const auto noMatchCount = filter->rowCount();
        const auto noMatchNs = timer.nsecsElapsed();
        QCOMPARE(noMatchCount, 0);

        // positive control: proves the timed pass really evaluates the rows
        timer.restart();
        filter->setFilterFixedString(QStringLiteral("Groceries"));
        const auto matchCount = filter->rowCount();
        const auto matchNs = timer.nsecsElapsed();
        QCOMPARE(matchCount, transactionCount);

        // the fixture's i18n warning flood hits QtTest's warning limit before
        // this line - run with -maxwarnings 0 to see it
        qDebug() << "scan of" << rows << "journal rows:" << (noMatchNs / 1000000.0) << "ms without and" << (matchNs / 1000000.0) << "ms with matches;"
                 << "extrapolated to 50k rows:" << (noMatchNs * (50000.0 / rows) / 1000000.0) << "/" << (matchNs * (50000.0 / rows) / 1000000.0) << "ms";
        // generous pathology guard (~50x native, also leaves headroom for
        // instrumented runs); the real evaluation is the logged number
        QVERIFY(noMatchNs < 2000000000);
    }
};

QTEST_GUILESS_MAIN(LedgerSearchFilterTest)

#include "test-ledgersearchfilter.moc"
