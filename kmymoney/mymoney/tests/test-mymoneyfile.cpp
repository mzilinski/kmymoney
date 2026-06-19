/*
    SPDX-FileCopyrightText: 2002-2019 Thomas Baumgart <tbaumgart@kde.org>
    SPDX-FileCopyrightText: 2004 Ace Jones <acejones@users.sourceforge.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test-mymoneyfile.h"
#include <iostream>

#include <QDataStream>
#include <QFile>
#include <QList>
#include <QTest>
#include <memory>

#include "accountsmodel.h"
#include "mymoneyaccount.h"
#include "mymoneyenums.h"
#include "mymoneyexception.h"
#include "mymoneyinstitution.h"
#include "mymoneymoney.h"
#include "mymoneypayee.h"
#include "mymoneyprice.h"
#include "mymoneysecurity.h"
#include "mymoneysplit.h"
#include "mymoneytestutils.h"
#include "mymoneytransaction.h"
#include "mymoneytransactionfilter.h"
#include "onlinejob.h"
#include "payeesmodel.h"

#include "payeeidentifier/ibanbic/ibanbic.h"
#include "payeeidentifiertyped.h"

QTEST_GUILESS_MAIN(MyMoneyFileTest)

MyMoneyFileTest::MyMoneyFileTest()
    : m(nullptr)
{
}

void MyMoneyFileTest::objectAdded(eMyMoney::File::Object type, const QString& id)
{
    Q_UNUSED(type);
    m_objectsAdded += id;
}

void MyMoneyFileTest::objectRemoved(eMyMoney::File::Object type, const QString& id)
{
    Q_UNUSED(type);
    m_objectsRemoved += id;
}

void MyMoneyFileTest::objectModified(eMyMoney::File::Object type, const QString& id)
{
    Q_UNUSED(type);
    m_objectsModified += id;
}

void MyMoneyFileTest::clearObjectLists()
{
    m_objectsAdded.clear();
    m_objectsModified.clear();
    m_objectsRemoved.clear();
    m_balanceChanged.clear();
    m_valueChanged.clear();
}

void MyMoneyFileTest::balanceChanged(const MyMoneyAccount& account)
{
    m_balanceChanged += account.id();
}

void MyMoneyFileTest::valueChanged(const MyMoneyAccount& account)
{
    m_valueChanged += account.id();
}

void MyMoneyFileTest::setupBaseCurrency()
{
    MyMoneySecurity base("EUR", "Euro", QChar(0x20ac));
    MyMoneyFileTransaction ft;
    try {
        m->currency(base.id());
    } catch (const MyMoneyException&) {
        m->addCurrency(base);
    }
    m->setBaseCurrency(base);
    ft.commit();
}

// this method will be called once at the beginning of the test
void MyMoneyFileTest::initTestCase()
{
    m = MyMoneyFile::instance();

    connect(m, &MyMoneyFile::objectAdded, this, &MyMoneyFileTest::objectAdded);
    connect(m, &MyMoneyFile::objectRemoved, this, &MyMoneyFileTest::objectRemoved);
    connect(m, &MyMoneyFile::objectModified, this, &MyMoneyFileTest::objectModified);
    connect(m, &MyMoneyFile::balanceChanged, this, &MyMoneyFileTest::balanceChanged);
    connect(m, &MyMoneyFile::valueChanged, this, &MyMoneyFileTest::valueChanged);
}

// this method will be called before each testfunction
void MyMoneyFileTest::init()
{
    clearObjectLists();
}

// this method will be called after each testfunction
void MyMoneyFileTest::cleanup()
{
    m->unload();
}

void MyMoneyFileTest::testEmptyConstructor()
{
    MyMoneyPayee user = m->user();

    QCOMPARE(user.name(), QString());
    QCOMPARE(user.address(), QString());
    QCOMPARE(user.city(), QString());
    QCOMPARE(user.state(), QString());
    QCOMPARE(user.postcode(), QString());
    QCOMPARE(user.telephone(), QString());
    QCOMPARE(user.email(), QString());

    QCOMPARE(m->institutionCount(), static_cast<unsigned>(0));
    QCOMPARE(m->dirty(), false);
    QCOMPARE(m->accountsModel()->itemList().count(), 0);
}

void MyMoneyFileTest::testAddOneInstitution()
{
    MyMoneyInstitution institution;

    institution.setName("institution1");
    institution.setTown("town");
    institution.setStreet("street");
    institution.setPostcode("postcode");
    institution.setTelephone("telephone");
    institution.setManager("manager");
    institution.setBankCode("sortcode");

    // MyMoneyInstitution institution_file("", institution);
    MyMoneyInstitution institution_id("I000002", institution);
    MyMoneyInstitution institution_noname(institution);
    institution_noname.setName(QString());

    QCOMPARE(m->institutionCount(), static_cast<unsigned>(0));

    m->setDirty(false);

    clearObjectLists();
    MyMoneyFileTransaction ft;
    try {
        m->addInstitution(institution);
        ft.commit();
        QCOMPARE(institution.id(), QLatin1String("I000001"));
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));
        QCOMPARE(m->dirty(), true);

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 1);
        QCOMPARE(m_objectsModified.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QCOMPARE(m_objectsAdded[0], QLatin1String("I000001"));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }

    clearObjectLists();
    ft.restart();
    try {
        m->addInstitution(institution_id);
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
        ft.commit();
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));
    }

    ft.restart();
    try {
        m->addInstitution(institution_noname);
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
        ft.commit();
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));
    }
    QCOMPARE(m_objectsRemoved.count(), 0);
    QCOMPARE(m_objectsAdded.count(), 0);
    QCOMPARE(m_objectsModified.count(), 0);
    QCOMPARE(m_balanceChanged.count(), 0);
    QCOMPARE(m_valueChanged.count(), 0);
}

void MyMoneyFileTest::testAddTwoInstitutions()
{
    testAddOneInstitution();
    MyMoneyInstitution institution;
    institution.setName("institution2");
    institution.setTown("town");
    institution.setStreet("street");
    institution.setPostcode("postcode");
    institution.setTelephone("telephone");
    institution.setManager("manager");
    institution.setBankCode("sortcode");

    m->setDirty(false);

    MyMoneyFileTransaction ft;
    try {
        m->addInstitution(institution);
        ft.commit();

        QCOMPARE(institution.id(), QLatin1String("I000002"));
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(2));
        QCOMPARE(m->dirty(), true);
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }

    m->setDirty(false);

    try {
        institution = m->institution("I000001");
        QCOMPARE(institution.id(), QLatin1String("I000001"));
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(2));
        QCOMPARE(m->dirty(), false);

        institution = m->institution("I000002");
        QCOMPARE(institution.id(), QLatin1String("I000002"));
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(2));
        QCOMPARE(m->dirty(), false);
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }
}

void MyMoneyFileTest::testRemoveInstitution()
{
    testAddTwoInstitutions();

    MyMoneyInstitution i;

    QCOMPARE(m->institutionCount(), static_cast<unsigned>(2));

    i = m->institution("I000001");
    QCOMPARE(i.id(), QLatin1String("I000001"));
    QCOMPARE(i.accountCount(), static_cast<unsigned>(0));

    clearObjectLists();

    m->setDirty(false);
    MyMoneyFileTransaction ft;
    try {
        m->removeInstitution(i);
        QCOMPARE(m_objectsRemoved.count(), 0);
        ft.commit();
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));
        QCOMPARE(m->dirty(), true);
        QCOMPARE(m_objectsRemoved.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_objectsModified.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QCOMPARE(m_objectsRemoved[0], QLatin1String("I000001"));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }

    m->setDirty(false);

    try {
        m->institution("I000001");
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));
        QCOMPARE(m->dirty(), false);
    }

    clearObjectLists();
    ft.restart();
    try {
        m->removeInstitution(i);
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
        ft.commit();
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));
        QCOMPARE(m->dirty(), false);
        QCOMPARE(m_objectsRemoved.count(), 0);
    }
}

void MyMoneyFileTest::testInstitutionRetrieval()
{
    testAddOneInstitution();

    m->setDirty(false);

    MyMoneyInstitution institution;

    QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));

    try {
        institution = m->institution("I000001");
        QCOMPARE(institution.id(), QLatin1String("I000001"));
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }

    try {
        institution = m->institution("I000002");
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(1));
    }

    QCOMPARE(m->dirty(), false);
}

void MyMoneyFileTest::testInstitutionListRetrieval()
{
    QList<MyMoneyInstitution> list;

    m->setDirty(false);
    list = m->institutionList();
    QCOMPARE(m->dirty(), false);
    QCOMPARE(list.count(), 0);

    testAddTwoInstitutions();

    m->setDirty(false);
    list = m->institutionList();
    QCOMPARE(m->dirty(), false);
    QCOMPARE(list.count(), 2);

    QList<MyMoneyInstitution>::const_iterator it;
    it = list.cbegin();

    QVERIFY((*it).name() == "institution1" || (*it).name() == "institution2");
    ++it;
    QVERIFY((*it).name() == "institution2" || (*it).name() == "institution1");
    ++it;
    QVERIFY(it == list.cend());
}

void MyMoneyFileTest::testInstitutionModify()
{
    testAddTwoInstitutions();
    MyMoneyInstitution institution;

    institution = m->institution("I000001");
    institution.setStreet("new street");
    institution.setTown("new town");
    institution.setPostcode("new postcode");
    institution.setTelephone("new telephone");
    institution.setManager("new manager");
    institution.setName("new name");
    institution.setBankCode("new sortcode");

    m->setDirty(false);

    clearObjectLists();
    MyMoneyFileTransaction ft;
    try {
        m->modifyInstitution(institution);
        ft.commit();
        QCOMPARE(institution.id(), QLatin1String("I000001"));
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(2));
        QCOMPARE(m->dirty(), true);

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QCOMPARE(m_objectsModified[0], QLatin1String("I000001"));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }

    MyMoneyInstitution newInstitution;
    newInstitution = m->institution("I000001");

    QCOMPARE(newInstitution.id(), QLatin1String("I000001"));
    QCOMPARE(newInstitution.street(), QLatin1String("new street"));
    QCOMPARE(newInstitution.town(), QLatin1String("new town"));
    QCOMPARE(newInstitution.postcode(), QLatin1String("new postcode"));
    QCOMPARE(newInstitution.telephone(), QLatin1String("new telephone"));
    QCOMPARE(newInstitution.manager(), QLatin1String("new manager"));
    QCOMPARE(newInstitution.name(), QLatin1String("new name"));
    QCOMPARE(newInstitution.bankcode(), QLatin1String("new sortcode"));

    m->setDirty(false);

    ft.restart();
    MyMoneyInstitution failInstitution2("I000003", newInstitution);
    try {
        m->modifyInstitution(failInstitution2);
        QFAIL("Exception expected");
    } catch (const MyMoneyException&) {
        ft.commit();
        QCOMPARE(failInstitution2.id(), QLatin1String("I000003"));
        QCOMPARE(m->institutionCount(), static_cast<unsigned>(2));
        QCOMPARE(m->dirty(), false);
    }
}

void MyMoneyFileTest::testSetFunctions()
{
    MyMoneyPayee user = m->user();

    QCOMPARE(user.name(), QString());
    QCOMPARE(user.address(), QString());
    QCOMPARE(user.city(), QString());
    QCOMPARE(user.state(), QString());
    QCOMPARE(user.postcode(), QString());
    QCOMPARE(user.telephone(), QString());
    QCOMPARE(user.email(), QString());

    MyMoneyFileTransaction ft;
    m->setDirty(false);
    user.setName("Name");
    m->setUser(user);
    QCOMPARE(m->dirty(), true);
    m->setDirty(false);
    user.setAddress("Street");
    m->setUser(user);
    QCOMPARE(m->dirty(), true);
    m->setDirty(false);
    user.setCity("Town");
    m->setUser(user);
    QCOMPARE(m->dirty(), true);
    m->setDirty(false);
    user.setState("County");
    m->setUser(user);
    QCOMPARE(m->dirty(), true);
    m->setDirty(false);
    user.setPostcode("Postcode");
    m->setUser(user);
    QCOMPARE(m->dirty(), true);
    m->setDirty(false);
    user.setTelephone("Telephone");
    m->setUser(user);
    QCOMPARE(m->dirty(), true);
    m->setDirty(false);
    user.setEmail("Email");
    m->setUser(user);
    QCOMPARE(m->dirty(), true);
    m->setDirty(false);

    ft.commit();
    user = m->user();
    QCOMPARE(user.name(), QLatin1String("Name"));
    QCOMPARE(user.address(), QLatin1String("Street"));
    QCOMPARE(user.city(), QLatin1String("Town"));
    QCOMPARE(user.state(), QLatin1String("County"));
    QCOMPARE(user.postcode(), QLatin1String("Postcode"));
    QCOMPARE(user.telephone(), QLatin1String("Telephone"));
    QCOMPARE(user.email(), QLatin1String("Email"));
}

void MyMoneyFileTest::testAddAccounts()
{
    testAddTwoInstitutions();
    setupBaseCurrency();
    MyMoneyAccount a, b, c;
    a.setAccountType(eMyMoney::Account::Type::Checkings);
    b.setAccountType(eMyMoney::Account::Type::Checkings);

    MyMoneyInstitution institution;

    m->setDirty(false);

    QCOMPARE(m->accountsModel()->itemList().count(), 0);

    institution = m->institution("I000001");
    QCOMPARE(institution.id(), QLatin1String("I000001"));

    a.setName("Account1");
    a.setInstitutionId(institution.id());
    a.setCurrencyId("EUR");

    clearObjectLists();
    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount parent = m->asset();
        m->addAccount(a, parent);
        ft.commit();
        QCOMPARE(m->accountsModel()->itemList().count(), 1);
        QCOMPARE(a.parentAccountId(), QLatin1String("AStd::Asset"));
        QCOMPARE(a.id(), QLatin1String("A000001"));
        QCOMPARE(a.institutionId(), QLatin1String("I000001"));
        QCOMPARE(a.currencyId(), QLatin1String("EUR"));
        QCOMPARE(m->dirty(), true);
        QCOMPARE(m->asset().accountList().count(), 1);
        QCOMPARE(m->asset().accountList().at(0), QLatin1String("A000001"));

        institution = m->institution("I000001");
        QCOMPARE(institution.accountCount(), static_cast<unsigned>(1));
        QCOMPARE(institution.accountList().at(0), QLatin1String("A000001"));

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 1);
        QCOMPARE(m_objectsModified.count(), 2);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsAdded.contains(QLatin1String("A000001")));
        QVERIFY(m_objectsModified.contains(QLatin1String("I000001")));
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Asset")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // try to add this account again, should not work
    ft.restart();
    try {
        MyMoneyAccount parent = m->asset();
        m->addAccount(a, parent);
        QFAIL("Expecting exception!");
    } catch (const MyMoneyException&) {
        ft.commit();
    }

    // check that we can modify the local object and
    // reload it from the file
    a.setName("AccountX");
    a = m->account("A000001");
    QCOMPARE(a.name(), QLatin1String("Account1"));

    m->setDirty(false);

    // check if we can get the same info to a different object
    c = m->account("A000001");
    QCOMPARE(c.accountType(), eMyMoney::Account::Type::Checkings);
    QCOMPARE(c.id(), QLatin1String("A000001"));
    QCOMPARE(c.name(), QLatin1String("Account1"));
    QCOMPARE(c.institutionId(), QLatin1String("I000001"));

    QCOMPARE(m->dirty(), false);

    // add a second account
    institution = m->institution("I000002");
    b.setName("Account2");
    b.setInstitutionId(institution.id());
    b.setCurrencyId("EUR");
    clearObjectLists();
    ft.restart();
    try {
        MyMoneyAccount parent = m->asset();
        m->addAccount(b, parent);
        ft.commit();
        QCOMPARE(m->dirty(), true);
        QCOMPARE(b.id(), QLatin1String("A000002"));
        QCOMPARE(b.currencyId(), QLatin1String("EUR"));
        QCOMPARE(b.parentAccountId(), QLatin1String("AStd::Asset"));
        QCOMPARE(m->accountsModel()->itemList().count(), 2);

        institution = m->institution("I000001");
        QCOMPARE(institution.accountCount(), static_cast<unsigned>(1));
        QCOMPARE(institution.accountList().at(0), QLatin1String("A000001"));

        institution = m->institution("I000002");
        QCOMPARE(institution.accountCount(), static_cast<unsigned>(1));
        QCOMPARE(institution.accountList().at(0), QLatin1String("A000002"));

        QCOMPARE(m->asset().accountList().count(), 2);
        QCOMPARE(m->asset().accountList().at(0), QLatin1String("A000001"));
        QCOMPARE(m->asset().accountList().at(1), QLatin1String("A000002"));

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 1);
        QCOMPARE(m_objectsModified.count(), 2);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsAdded.contains(QLatin1String("A000002")));
        QVERIFY(m_objectsModified.contains(QLatin1String("I000002")));
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Asset")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    MyMoneyAccount p;

    p = m->account("A000002");
    QCOMPARE(p.accountType(), eMyMoney::Account::Type::Checkings);
    QCOMPARE(p.id(), QLatin1String("A000002"));
    QCOMPARE(p.name(), QLatin1String("Account2"));
    QCOMPARE(p.institutionId(), QLatin1String("I000002"));
    QCOMPARE(p.currencyId(), QLatin1String("EUR"));
}

void MyMoneyFileTest::testAddCategories()
{
    setupBaseCurrency();

    MyMoneyAccount a, b, c;
    a.setAccountType(eMyMoney::Account::Type::Income);
    a.setOpeningDate(QDate::currentDate());
    b.setAccountType(eMyMoney::Account::Type::Expense);

    m->setDirty(false);

    QCOMPARE(m->accountsModel()->itemList().count(), 0);
    QCOMPARE(a.openingDate(), QDate::currentDate());
    QVERIFY(!b.openingDate().isValid());

    a.setName("Account1");
    a.setCurrencyId("EUR");

    clearObjectLists();
    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount parent = m->income();
        m->addAccount(a, parent);
        ft.commit();
        QCOMPARE(m->accountsModel()->itemList().count(), 1);
        QCOMPARE(a.parentAccountId(), QLatin1String("AStd::Income"));
        QCOMPARE(a.id(), QLatin1String("A000001"));
        QCOMPARE(a.institutionId(), QString());
        QCOMPARE(a.currencyId(), QLatin1String("EUR"));
        QCOMPARE(a.openingDate(), QDate(1900, 1, 1));
        QCOMPARE(m->dirty(), true);
        QCOMPARE(m->income().accountList().count(), 1);
        QCOMPARE(m->income().accountList()[0], QLatin1String("A000001"));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // add a second category, expense this time
    b.setName("Account2");
    b.setCurrencyId("EUR");
    clearObjectLists();
    ft.restart();
    try {
        MyMoneyAccount parent = m->expense();
        m->addAccount(b, parent);
        ft.commit();
        QCOMPARE(m->dirty(), true);
        QCOMPARE(b.id(), QLatin1String("A000002"));
        QCOMPARE(a.institutionId(), QString());
        QCOMPARE(b.currencyId(), QLatin1String("EUR"));
        QCOMPARE(b.openingDate(), QDate(1900, 1, 1));
        QCOMPARE(b.parentAccountId(), QLatin1String("AStd::Expense"));
        QCOMPARE(m->accountsModel()->itemList().count(), 2);

        QCOMPARE(m->income().accountList().count(), 1);
        QCOMPARE(m->expense().accountList().count(), 1);
        QCOMPARE(m->income().accountList()[0], QLatin1String("A000001"));
        QCOMPARE(m->expense().accountList()[0], QLatin1String("A000002"));

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 1);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsAdded.contains(QLatin1String("A000002")));
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Expense")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testModifyAccount()
{
    testAddAccounts();
    m->setDirty(false);

    MyMoneyAccount p = m->account("A000001");
    MyMoneyInstitution institution;

    QCOMPARE(p.accountType(), eMyMoney::Account::Type::Checkings);
    QCOMPARE(p.name(), QLatin1String("Account1"));

    p.setName("New account name");
    MyMoneyFileTransaction ft;
    clearObjectLists();
    try {
        m->modifyAccount(p);
        ft.commit();

        QCOMPARE(m->dirty(), true);
        QCOMPARE(m->accountsModel()->itemList().count(), 2);
        QCOMPARE(p.accountType(), eMyMoney::Account::Type::Checkings);
        QCOMPARE(p.name(), QLatin1String("New account name"));

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsModified.contains(QLatin1String("A000001")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
    m->setDirty(false);

    // try to move account to new institution
    p.setInstitutionId("I000002");
    ft.restart();
    clearObjectLists();
    try {
        m->modifyAccount(p);
        ft.commit();

        QCOMPARE(m->dirty(), true);
        QCOMPARE(m->accountsModel()->itemList().count(), 2);
        QCOMPARE(p.accountType(), eMyMoney::Account::Type::Checkings);
        QCOMPARE(p.name(), QLatin1String("New account name"));
        QCOMPARE(p.institutionId(), QLatin1String("I000002"));

        institution = m->institution("I000001");
        QCOMPARE(institution.accountCount(), static_cast<unsigned>(0));

        institution = m->institution("I000002");
        QCOMPARE(institution.accountCount(), static_cast<unsigned>(2));
        QCOMPARE(institution.accountList().at(0), QLatin1String("A000002"));
        QCOMPARE(institution.accountList().at(1), QLatin1String("A000001"));

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_objectsModified.count(), 3);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsModified.contains(QLatin1String("A000001")));
        QVERIFY(m_objectsModified.contains(QLatin1String("I000001")));
        QVERIFY(m_objectsModified.contains(QLatin1String("I000002")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
    m->setDirty(false);

    // try to change to an account type that is allowed
    p.setAccountType(eMyMoney::Account::Type::Savings);
    ft.restart();
    try {
        m->modifyAccount(p);
        ft.commit();

        QCOMPARE(m->dirty(), true);
        QCOMPARE(m->accountsModel()->itemList().count(), 2);
        QCOMPARE(p.accountType(), eMyMoney::Account::Type::Savings);
        QCOMPARE(p.name(), QLatin1String("New account name"));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
    m->setDirty(false);

    // try to change to an account type that is not allowed
    p.setAccountType(eMyMoney::Account::Type::CreditCard);
    ft.restart();
    try {
        m->modifyAccount(p);
        QFAIL("Expecting exception!");
    } catch (const MyMoneyException&) {
        ft.commit();
    }
    m->setDirty(false);

    // try to fool engine a bit
    p.setParentAccountId("A000001");
    ft.restart();
    try {
        m->modifyAccount(p);
        QFAIL("Expecting exception!");
    } catch (const MyMoneyException&) {
        ft.commit();
    }
}

void MyMoneyFileTest::testReparentAccount()
{
    testAddAccounts();
    m->setDirty(false);

    MyMoneyAccount p = m->account("A000001");
    MyMoneyAccount q = m->account("A000002");
    MyMoneyAccount o = m->account(p.parentAccountId());

    // make A000001 a child of A000002
    clearObjectLists();
    MyMoneyFileTransaction ft;
    try {
        QVERIFY(p.parentAccountId() != q.id());
        QCOMPARE(o.accountCount(), 2);
        QCOMPARE(q.accountCount(), 0);
        m->reparentAccount(p, q);
        ft.commit();
        QCOMPARE(m->dirty(), true);
        QCOMPARE(p.parentAccountId(), q.id());
        QCOMPARE(q.accountCount(), 1);
        QCOMPARE(q.id(), QLatin1String("A000002"));
        QCOMPARE(p.id(), QLatin1String("A000001"));
        QCOMPARE(q.accountList().at(0), p.id());

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_objectsModified.count(), 3);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsModified.contains(QLatin1String("A000001")));
        QVERIFY(m_objectsModified.contains(QLatin1String("A000002")));
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Asset")));

        o = m->account(o.id());
        QCOMPARE(o.accountCount(), 1);
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testRemoveStdAccount(const MyMoneyAccount& acc)
{
    QString txt("Exception expected while removing account ");
    txt += acc.id();
    MyMoneyFileTransaction ft;
    try {
        m->removeAccount(acc);
        QFAIL(qPrintable(txt));
    } catch (const MyMoneyException&) {
        ft.commit();
    }
}

void MyMoneyFileTest::testRemoveAccount()
{
    MyMoneyInstitution institution;

    testAddAccounts();
    QCOMPARE(m->accountsModel()->itemList().count(), 2);
    m->setDirty(false);

    MyMoneyAccount p = m->account("A000001");

    clearObjectLists();

    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount q("Ainvalid", p);
        m->removeAccount(q);
        QFAIL("Exception expected!");
    } catch (const MyMoneyException&) {
        ft.commit();
    }

    ft.restart();
    try {
        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_objectsModified.count(), 0);

        m->removeAccount(p);
        ft.commit();
        QCOMPARE(m->dirty(), true);
        QCOMPARE(m->accountsModel()->itemList().count(), 1);
        institution = m->institution("I000001");
        QCOMPARE(institution.accountCount(), static_cast<unsigned>(0));
        QCOMPARE(m->asset().accountList().count(), 1);

        QCOMPARE(m_objectsRemoved.count(), 1);
        QCOMPARE(m_objectsModified.count(), 2);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);

        QVERIFY(m_objectsRemoved.contains(QLatin1String("A000001")));
        QVERIFY(m_objectsModified.contains(QLatin1String("I000001")));
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Asset")));

        institution = m->institution("I000002");
        QCOMPARE(institution.accountCount(), static_cast<unsigned>(1));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // Check that the standard account-groups cannot be removed
    testRemoveStdAccount(m->liability());
    testRemoveStdAccount(m->asset());
    testRemoveStdAccount(m->expense());
    testRemoveStdAccount(m->income());
}

void MyMoneyFileTest::testRemoveAccountTree()
{
    testReparentAccount();
    MyMoneyAccount a = m->account("A000002");

    clearObjectLists();
    MyMoneyFileTransaction ft;
    // remove the account
    try {
        m->removeAccount(a);
        ft.commit();

        QCOMPARE(m_objectsRemoved.count(), 1);
        QCOMPARE(m_objectsModified.count(), 5);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);

        QVERIFY(m_objectsRemoved.contains(QLatin1String("A000002")));
        QVERIFY(m_objectsModified.contains(QLatin1String("A000001")));
        QVERIFY(m_objectsModified.contains(QLatin1String("I000002")));
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Asset")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
    QCOMPARE(m->accountsModel()->itemList().count(), 1);

    // make sure it's gone
    try {
        m->account("A000002");
        QFAIL("Exception expected!");
    } catch (const MyMoneyException&) {
    }

    // make sure that children are re-parented to parent account
    try {
        a = m->account("A000001");
        QCOMPARE(a.parentAccountId(), m->asset().id());
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testAccountListRetrieval()
{
    QList<MyMoneyAccount> list;

    m->setDirty(false);
    m->accountList(list);
    QCOMPARE(m->dirty(), false);
    QCOMPARE(list.count(), 0);

    testAddAccounts();

    m->setDirty(false);
    list.clear();
    m->accountList(list);
    QCOMPARE(m->dirty(), false);
    QCOMPARE(list.count(), 2);

    QCOMPARE(list[0].accountType(), eMyMoney::Account::Type::Checkings);
    QCOMPARE(list[1].accountType(), eMyMoney::Account::Type::Checkings);
}

void MyMoneyFileTest::testAddTransaction()
{
    testAddAccounts();
    setupBaseCurrency();

    MyMoneyTransaction t, p;

    MyMoneyAccount exp1;
    exp1.setAccountType(eMyMoney::Account::Type::Expense);
    exp1.setName("Expense1");
    MyMoneyAccount exp2;
    exp2.setAccountType(eMyMoney::Account::Type::Expense);
    exp2.setName("Expense2");

    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount parent = m->expense();
        m->addAccount(exp1, parent);
        m->addAccount(exp2, parent);
        ft.commit();
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // fake the last modified flag to check that the
    // date is updated when we add the transaction
    MyMoneyAccount a = m->account("A000001");
    a.setLastModified(QDate(1, 2, 3));
    ft.restart();
    try {
        m->modifyAccount(a);
        ft.commit();
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
    ft.restart();

    QCOMPARE(m->accountsModel()->itemList().count(), 4);
    a = m->account("A000001");
    QCOMPARE(a.lastModified(), QDate(1, 2, 3));

    // construct a transaction and add it to the pool
    t.setPostDate(QDate(2002, 2, 1));
    t.setMemo("Memotext");

    MyMoneySplit split1;
    MyMoneySplit split2;

    split1.setAccountId("A000001");
    split1.setShares(MyMoneyMoney(-1000, 100));
    split1.setValue(MyMoneyMoney(-1000, 100));
    split2.setAccountId("A000003");
    split2.setValue(MyMoneyMoney(1000, 100));
    split2.setShares(MyMoneyMoney(1000, 100));
    try {
        t.addSplit(split1);
        t.addSplit(split2);
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    /*
            // FIXME: we don't have a payee and a number field right now
            // guess we should have a number field per split, don't know
            // about the payee
            t.setMethod(MyMoneyCheckingTransaction::Withdrawal);
            t.setPayee("Thomas Baumgart");
            t.setNumber("1234");
            t.setState(MyMoneyCheckingTransaction::Cleared);
    */
    m->setDirty(false);

    ft.restart();
    clearObjectLists();
    try {
        m->addTransaction(t);
        ft.commit();
        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 1);
        QCOMPARE(m_balanceChanged.count(), 2);
        QCOMPARE(m_balanceChanged.count("A000001"), 1);
        QCOMPARE(m_balanceChanged.count("A000003"), 1);
        QCOMPARE(m_valueChanged.count(), 0);
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
    ft.restart();
    clearObjectLists();

    QCOMPARE(t.id(), QLatin1String("T000000000000000001"));
    QCOMPARE(t.postDate(), QDate(2002, 2, 1));
    QCOMPARE(t.entryDate(), QDate::currentDate());
    QCOMPARE(m->dirty(), true);

    // check the balance of the accounts
    a = m->account("A000001");
    QCOMPARE(a.lastModified(), QDate::currentDate());
    QCOMPARE(a.balance().toDouble(), MyMoneyMoney(-1000, 100).toDouble());

    MyMoneyAccount b = m->account("A000003");
    QCOMPARE(b.lastModified(), QDate::currentDate());
    QCOMPARE(b.balance(), MyMoneyMoney(1000, 100));

    m->setDirty(false);

    // locate transaction in MyMoneyFile via id

    try {
        p = m->transaction("T000000000000000001");
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QCOMPARE(p.splitCount(), static_cast<unsigned>(2));
        QCOMPARE(p.memo(), QLatin1String("Memotext"));
        QCOMPARE(p.splits()[0].accountId(), QLatin1String("A000001"));
        QCOMPARE(p.splits()[1].accountId(), QLatin1String("A000003"));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // check if it's in the account(s) as well

    try {
        p = m->transaction("A000001", 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QCOMPARE(p.id(), QLatin1String("T000000000000000001"));
        QCOMPARE(p.splitCount(), static_cast<unsigned>(2));
        QCOMPARE(p.memo(), QLatin1String("Memotext"));
        QCOMPARE(p.splits()[0].accountId(), QLatin1String("A000001"));
        QCOMPARE(p.splits()[1].accountId(), QLatin1String("A000003"));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    try {
        p = m->transaction("A000003", 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QCOMPARE(p.id(), QLatin1String("T000000000000000001"));
        QCOMPARE(p.splitCount(), static_cast<unsigned>(2));
        QCOMPARE(p.memo(), QLatin1String("Memotext"));
        QCOMPARE(p.splits()[0].accountId(), QLatin1String("A000001"));
        QCOMPARE(p.splits()[1].accountId(), QLatin1String("A000003"));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testIsStandardAccount()
{
    QCOMPARE(m->isStandardAccount(m->liability().id()), true);
    QCOMPARE(m->isStandardAccount(m->asset().id()), true);
    QCOMPARE(m->isStandardAccount(m->expense().id()), true);
    QCOMPARE(m->isStandardAccount(m->income().id()), true);
    QCOMPARE(m->isStandardAccount(m->equity().id()), true);
    QCOMPARE(m->isStandardAccount("A00001"), false);
}

void MyMoneyFileTest::testHasActiveSplits()
{
    testAddTransaction();

    QCOMPARE(m->hasActiveSplits("A000001"), true);
    QCOMPARE(m->hasActiveSplits("A000002"), false);
}

void MyMoneyFileTest::testModifyTransactionSimple()
{
    // this will test that we can modify the basic attributes
    // of a transaction
    testAddTransaction();

    MyMoneyTransaction t = m->transaction("T000000000000000001");
    t.setMemo("New Memotext");
    m->setDirty(false);

    MyMoneyFileTransaction ft;
    clearObjectLists();
    try {
        m->modifyTransaction(t);
        ft.commit();
        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 2);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000001")), 1);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000003")), 1);
        QCOMPARE(m_valueChanged.count(), 0);
        t = m->transaction("T000000000000000001");
        QCOMPARE(t.memo(), QLatin1String("New Memotext"));
        QCOMPARE(m->dirty(), true);

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testModifyTransactionNewPostDate()
{
    // this will test that we can modify the basic attributes
    // of a transaction
    testAddTransaction();

    MyMoneyTransaction t = m->transaction("T000000000000000001");
    t.setPostDate(QDate(2004, 2, 1));
    m->setDirty(false);

    MyMoneyFileTransaction ft;
    clearObjectLists();
    try {
        m->modifyTransaction(t);
        ft.commit();
        t = m->transaction("T000000000000000001");
        QCOMPARE(t.postDate(), QDate(2004, 2, 1));
        t = m->transaction("A000001", 0);
        QCOMPARE(t.id(), QLatin1String("T000000000000000001"));
        QCOMPARE(m->dirty(), true);

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 2);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000001")), 1);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000003")), 1);
        QCOMPARE(m_valueChanged.count(), 0);
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testModifyTransactionNewAccount()
{
    // this will test that we can modify the basic attributes
    // of a transaction
    testAddTransaction();

    MyMoneyTransaction t = m->transaction("T000000000000000001");
    MyMoneySplit s;
    s = t.splits()[0];
    s.setAccountId("A000002");
    t.modifySplit(s);

    m->setDirty(false);
    QList<MyMoneyTransaction> list;
    MyMoneyFileTransaction ft;
    clearObjectLists();
    try {
        MyMoneyTransactionFilter f1("A000001");
        MyMoneyTransactionFilter f2("A000002");
        MyMoneyTransactionFilter f3("A000003");
        m->transactionList(list, f1);
        QCOMPARE(list.count(), 1);
        m->transactionList(list, f2);
        QCOMPARE(list.count(), 0);
        m->transactionList(list, f3);
        QCOMPARE(list.count(), 1);

        m->modifyTransaction(t);
        ft.commit();
        t = m->transaction("T000000000000000001");
        QCOMPARE(t.postDate(), QDate(2002, 2, 1));
        t = m->transaction("A000002", 0);
        QCOMPARE(m->dirty(), true);
        m->transactionList(list, f1);
        QCOMPARE(list.count(), 0);
        m->transactionList(list, f2);
        QCOMPARE(list.count(), 1);
        m->transactionList(list, f3);
        QCOMPARE(list.count(), 1);

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 3);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000001")), 1);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000002")), 1);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000003")), 1);
        QCOMPARE(m_valueChanged.count(), 0);

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testRemoveTransaction()
{
    testModifyTransactionNewPostDate();

    MyMoneyTransaction t;
    t = m->transaction("T000000000000000001");

    m->setDirty(false);
    MyMoneyFileTransaction ft;
    QList<MyMoneyTransaction> list;
    clearObjectLists();
    try {
        m->removeTransaction(t);
        ft.commit();
        QCOMPARE(m->dirty(), true);
        QCOMPARE(m->transactionCount(), static_cast<unsigned>(0));
        MyMoneyTransactionFilter f1("A000001");
        MyMoneyTransactionFilter f2("A000002");
        MyMoneyTransactionFilter f3("A000003");
        m->transactionList(list, f1);
        QCOMPARE(list.count(), 0);
        m->transactionList(list, f2);
        QCOMPARE(list.count(), 0);
        m->transactionList(list, f3);
        QCOMPARE(list.count(), 0);

        QCOMPARE(m_objectsRemoved.count(), 1);
        QCOMPARE(m_objectsModified.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 2);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000001")), 1);
        QCOMPARE(m_balanceChanged.count(QLatin1String("A000003")), 1);
        QCOMPARE(m_valueChanged.count(), 0);

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

/*
 * This function is currently not implemented. It's kind of tricky
 * because it modifies a lot of objects in a single call. This might
 * be a problem for the undo/redo stuff. That's why I left it out in
 * the first run. We might add it, if we need it.
 * /
void testMoveSplits() {
        testModifyTransactionNewPostDate();

        QCOMPARE(m->account("A000001").transactionCount(), 1);
        QCOMPARE(m->account("A000002").transactionCount(), 0);
        QCOMPARE(m->account("A000003").transactionCount(), 1);

        try {
                m->moveSplits("A000001", "A000002");
                QCOMPARE(m->account("A000001").transactionCount(), 0);
                QCOMPARE(m->account("A000002").transactionCount(), 1);
                QCOMPARE(m->account("A000003").transactionCount(), 1);
        } catch (const MyMoneyException&) {
                QFAIL("Unexpected exception!");
        }
}
*/

void MyMoneyFileTest::testBalanceTotal()
{
    testAddTransaction();
    MyMoneyTransaction t;

    // construct a transaction and add it to the pool
    t.setPostDate(QDate(2002, 2, 1));
    t.setMemo("Memotext");

    MyMoneySplit split1;
    MyMoneySplit split2;

    MyMoneyFileTransaction ft;
    try {
        split1.setAccountId("A000002");
        split1.setShares(MyMoneyMoney(-1000, 100));
        split1.setValue(MyMoneyMoney(-1000, 100));
        split2.setAccountId("A000004");
        split2.setValue(MyMoneyMoney(1000, 100));
        split2.setShares(MyMoneyMoney(1000, 100));
        t.addSplit(split1);
        t.addSplit(split2);
        m->addTransaction(t);
        ft.commit();
        ft.restart();
        QCOMPARE(t.id(), QLatin1String("T000000000000000002"));
        QCOMPARE(m->totalBalance("A000001"), MyMoneyMoney(-1000, 100));
        QCOMPARE(m->totalBalance("A000002"), MyMoneyMoney(-1000, 100));

        MyMoneyAccount p = m->account("A000001");
        MyMoneyAccount q = m->account("A000002");
        m->reparentAccount(p, q);
        ft.commit();
        // check totalBalance() and balance() with combinations of parameters
        QCOMPARE(m->totalBalance("A000001"), MyMoneyMoney(-1000, 100));
        QCOMPARE(m->totalBalance("A000002"), MyMoneyMoney(-2000, 100));
        QVERIFY(m->totalBalance("A000002", QDate(2002, 1, 15)).isZero());

        QCOMPARE(m->balance("A000001"), MyMoneyMoney(-1000, 100));
        QCOMPARE(m->balance("A000002"), MyMoneyMoney(-1000, 100));
        // Date of a transaction
        QCOMPARE(m->balance("A000001", QDate(2002, 2, 1)), MyMoneyMoney(-1000, 100));
        QCOMPARE(m->balance("A000002", QDate(2002, 2, 1)), MyMoneyMoney(-1000, 100));
        // Date after last transaction
        QCOMPARE(m->balance("A000001", QDate(2002, 2, 1)), MyMoneyMoney(-1000, 100));
        QCOMPARE(m->balance("A000002", QDate(2002, 2, 1)), MyMoneyMoney(-1000, 100));
        // Date before first transaction
        QVERIFY(m->balance("A000001", QDate(2002, 1, 15)).isZero());
        QVERIFY(m->balance("A000002", QDate(2002, 1, 15)).isZero());

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // Now check for exceptions
    try {
        // Account not found for balance()
        QVERIFY(m->balance("A000005").isZero());
        QFAIL("Exception expected");
    } catch (const MyMoneyException&) {
    }

    try {
        // Account not found for totalBalance()
        QVERIFY(m->totalBalance("A000005").isZero());
        QFAIL("Exception expected");
    } catch (const MyMoneyException&) {
    }
}

void MyMoneyFileTest::testModifyStandardAccountNames()
{
    MyMoneyFileTransaction ft;
    clearObjectLists();
    try {
        auto account = m->account(MyMoneyAccount::stdAccName(eMyMoney::Account::Standard::Liability));
        QVERIFY(account.name() != QLatin1String("Verbindlichkeiten"));
        account.setName(QLatin1String("Verbindlichkeiten"));
        m->modifyAccount(account);
        ft.commit();
        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Liability")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }
    ft.restart();
    clearObjectLists();
    try {
        auto account = m->account(MyMoneyAccount::stdAccName(eMyMoney::Account::Standard::Asset));
        QVERIFY(account.name() != QString::fromUtf8("Vermögen"));
        account.setName(QString::fromUtf8("Vermögen"));
        m->modifyAccount(account);
        ft.commit();
        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Asset")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }
    ft.restart();
    clearObjectLists();
    try {
        auto account = m->account(MyMoneyAccount::stdAccName(eMyMoney::Account::Standard::Expense));
        QVERIFY(account.name() != QLatin1String("Ausgaben"));
        account.setName(QLatin1String("Ausgaben"));
        m->modifyAccount(account);
        ft.commit();
        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Expense")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }
    ft.restart();
    clearObjectLists();
    try {
        auto account = m->account(MyMoneyAccount::stdAccName(eMyMoney::Account::Standard::Income));
        QVERIFY(account.name() != QLatin1String("Einnahmen"));
        account.setName(QLatin1String("Einnahmen"));
        m->modifyAccount(account);
        ft.commit();
        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Income")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }
    ft.restart();

    QCOMPARE(m->liability().name(), QLatin1String("Verbindlichkeiten"));
    QCOMPARE(m->asset().name(), QString::fromUtf8("Vermögen"));
    QCOMPARE(m->expense().name(), QLatin1String("Ausgaben"));
    QCOMPARE(m->income().name(), QLatin1String("Einnahmen"));
}

void MyMoneyFileTest::testAddPayee()
{
    MyMoneyPayee p;

    p.setName("THB");
    QCOMPARE(m->dirty(), false);
    MyMoneyFileTransaction ft;
    try {
        m->addPayee(p);
        ft.commit();
        QCOMPARE(m->dirty(), true);
        QCOMPARE(p.id(), QLatin1String("P000001"));

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 1);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);

        QVERIFY(m_objectsAdded.contains(QLatin1String("P000001")));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }
}

void MyMoneyFileTest::testModifyPayee()
{
    MyMoneyPayee p;

    testAddPayee();
    clearObjectLists();

    p = m->payee("P000001");
    p.setName("New name");
    MyMoneyFileTransaction ft;
    try {
        m->modifyPayee(p);
        ft.commit();
        p = m->payee("P000001");
        QCOMPARE(p.name(), QLatin1String("New name"));

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);

        QVERIFY(m_objectsModified.contains(QLatin1String("P000001")));
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }
}

void MyMoneyFileTest::testRemovePayee()
{
    MyMoneyPayee p;

    testAddPayee();
    clearObjectLists();
    QCOMPARE(m->payeeList().count(), 1);

    p = m->payee("P000001");
    MyMoneyFileTransaction ft;
    try {
        m->removePayee(p);
        ft.commit();
        QCOMPARE(m->payeeList().count(), 0);

        QCOMPARE(m_objectsRemoved.count(), 1);
        QCOMPARE(m_objectsModified.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 0);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);

        QVERIFY(m_objectsRemoved.contains(QLatin1String("P000001")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception");
    }
}

void MyMoneyFileTest::testPayeeWithIdentifier()
{
    MyMoneyPayee p;
    try {
        MyMoneyFileTransaction ft;
        m->addPayee(p);
        ft.commit();

        p = m->payee(p.id());

        payeeIdentifier ident = payeeIdentifier(new payeeIdentifiers::ibanBic());
        payeeIdentifierTyped<payeeIdentifiers::ibanBic> iban(ident);
        iban->setIban(QLatin1String("DE82 2007 0024 0066 6446 00"));

        ft.restart();
        p.addPayeeIdentifier(iban);
        m->modifyPayee(p);
        ft.commit();

        p = m->payee(p.id());
        QCOMPARE(p.payeeIdentifiers().count(), 1);

        ident = p.payeeIdentifiers().at(0);
        try {
            iban = payeeIdentifierTyped<payeeIdentifiers::ibanBic>(ident);
        } catch (...) {
            QFAIL("Unexpected exception");
        }
        QCOMPARE(iban->electronicIban(), QLatin1String("DE82200700240066644600"));
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testAddTransactionStd()
{
    testAddAccounts();
    MyMoneyTransaction t, p;
    MyMoneyAccount a;

    a = m->account("A000001");

    // construct a transaction and add it to the pool
    t.setPostDate(QDate(2002, 2, 1));
    t.setMemo("Memotext");

    MyMoneySplit split1;
    MyMoneySplit split2;

    split1.setAccountId("A000001");
    split1.setShares(MyMoneyMoney(-1000, 100));
    split1.setValue(MyMoneyMoney(-1000, 100));
    split2.setAccountId(MyMoneyAccount::stdAccName(eMyMoney::Account::Standard::Expense));
    split2.setValue(MyMoneyMoney(1000, 100));
    split2.setShares(MyMoneyMoney(1000, 100));
    try {
        t.addSplit(split1);
        t.addSplit(split2);
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    /*
            // FIXME: we don't have a payee and a number field right now
            // guess we should have a number field per split, don't know
            // about the payee
            t.setMethod(MyMoneyCheckingTransaction::Withdrawal);
            t.setPayee("Thomas Baumgart");
            t.setNumber("1234");
            t.setState(MyMoneyCheckingTransaction::Cleared);
    */
    m->setDirty(false);

    MyMoneyFileTransaction ft;
    try {
        m->addTransaction(t);
        ft.commit();
        QFAIL("Missing expected exception!");
    } catch (const MyMoneyException&) {
    }

    QCOMPARE(m->dirty(), false);
}

void MyMoneyFileTest::testAccount2Category()
{
    testReparentAccount();
    QCOMPARE(m->accountToCategory("A000001"), QLatin1String("Account2:Account1"));
    QCOMPARE(m->accountToCategory("A000002"), QLatin1String("Account2"));
}

void MyMoneyFileTest::testCategory2Account()
{
    testAddTransaction();
    MyMoneyAccount a = m->account("A000003");
    MyMoneyAccount b = m->account("A000004");

    MyMoneyFileTransaction ft;
    try {
        m->reparentAccount(b, a);
        ft.commit();
        QCOMPARE(m->categoryToAccount("Expense1"), QLatin1String("A000003"));
        QCOMPARE(m->categoryToAccount("Expense1:Expense2"), QLatin1String("A000004"));
        QVERIFY(m->categoryToAccount("Acc2").isEmpty());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testHasAccount()
{
    testAddAccounts();

    MyMoneyAccount a, b;
    a.setAccountType(eMyMoney::Account::Type::Checkings);
    a.setName("Account3");
    b = m->account("A000001");
    MyMoneyFileTransaction ft;
    try {
        m->addAccount(a, b);
        ft.commit();
        QCOMPARE(m->accountsModel()->itemList().count(), 3);
        QCOMPARE(a.parentAccountId(), QLatin1String("A000001"));
        QCOMPARE(m->hasAccount("A000001", "Account3"), true);
        QCOMPARE(m->hasAccount("A000001", "Account2"), false);
        QCOMPARE(m->hasAccount("A000002", "Account3"), false);
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testAddEquityAccount()
{
    MyMoneyAccount i;
    i.setName("Investment");
    i.setAccountType(eMyMoney::Account::Type::Investment);

    setupBaseCurrency();

    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount parent = m->asset();
        m->addAccount(i, parent);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    // keep a copy for later use
    m_inv = i;

    // make sure, that only equity accounts can be children to it
    MyMoneyAccount a;
    a.setName("Testaccount");
    QList<eMyMoney::Account::Type> list;
    list << eMyMoney::Account::Type::Checkings;
    list << eMyMoney::Account::Type::Savings;
    list << eMyMoney::Account::Type::Cash;
    list << eMyMoney::Account::Type::CreditCard;
    list << eMyMoney::Account::Type::Loan;
    list << eMyMoney::Account::Type::CertificateDep;
    list << eMyMoney::Account::Type::Investment;
    list << eMyMoney::Account::Type::MoneyMarket;
    list << eMyMoney::Account::Type::Asset;
    list << eMyMoney::Account::Type::Liability;
    list << eMyMoney::Account::Type::Currency;
    list << eMyMoney::Account::Type::Income;
    list << eMyMoney::Account::Type::Expense;
    list << eMyMoney::Account::Type::AssetLoan;

    QList<eMyMoney::Account::Type>::Iterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        a.setAccountType(*it);
        ft.restart();
        try {
            m->addAccount(a, i);
            const auto msg = QStringLiteral("Can add non-equity type %1 to investment").arg(static_cast<int>(*it));
            QFAIL(msg.toLatin1());
        } catch (const MyMoneyException&) {
            ft.commit();
        }
    }
    ft.restart();
    try {
        a.setName("Teststock");
        a.setAccountType(eMyMoney::Account::Type::Stock);
        m->addAccount(a, i);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testReparentEquity()
{
    testAddEquityAccount();
    testAddEquityAccount();
    MyMoneyAccount parent;

    // check the bad cases
    QList<eMyMoney::Account::Type> list;
    list << eMyMoney::Account::Type::Checkings;
    list << eMyMoney::Account::Type::Savings;
    list << eMyMoney::Account::Type::Cash;
    list << eMyMoney::Account::Type::CertificateDep;
    list << eMyMoney::Account::Type::MoneyMarket;
    list << eMyMoney::Account::Type::Asset;
    list << eMyMoney::Account::Type::AssetLoan;
    list << eMyMoney::Account::Type::Currency;
    parent = m->asset();
    testReparentEquity(list, parent);

    list.clear();
    list << eMyMoney::Account::Type::CreditCard;
    list << eMyMoney::Account::Type::Loan;
    list << eMyMoney::Account::Type::Liability;
    parent = m->liability();
    testReparentEquity(list, parent);

    list.clear();
    list << eMyMoney::Account::Type::Income;
    parent = m->income();
    testReparentEquity(list, parent);

    list.clear();
    list << eMyMoney::Account::Type::Expense;
    parent = m->expense();
    testReparentEquity(list, parent);

    // now check the good case
    MyMoneyAccount stock = m->account("A000002");
    MyMoneyAccount inv = m->account(m_inv.id());
    MyMoneyFileTransaction ft;
    try {
        m->reparentAccount(stock, inv);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testReparentEquity(QList<eMyMoney::Account::Type>& list, MyMoneyAccount& parent)
{
    MyMoneyAccount a;
    MyMoneyAccount stock = m->account("A000002");

    QList<eMyMoney::Account::Type>::Iterator it;
    MyMoneyFileTransaction ft;
    for (it = list.begin(); it != list.end(); ++it) {
        a.setName(QString("Testaccount %1").arg((int)*it));
        a.setAccountType(*it);
        try {
            m->addAccount(a, parent);
            m->reparentAccount(stock, a);
            const auto msg = QStringLiteral("Can reparent stock to non-investment type %1 to account").arg(static_cast<int>(*it));
            QFAIL(msg.toLatin1());
        } catch (const MyMoneyException&) {
            ft.commit();
        }
        ft.restart();
    }
}

void MyMoneyFileTest::testBaseCurrency()
{
    MyMoneySecurity base("EUR", "Euro", QChar(0x20ac));
    MyMoneySecurity ref;

    // make sure, no base currency is set
    try {
        ref = m->baseCurrency();
        QVERIFY(ref.id().isEmpty());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // make sure, we cannot assign an unknown currency
    try {
        m->setBaseCurrency(base);
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
    }

    MyMoneyFileTransaction ft;
    // add the currency and try again
    try {
        m->addCurrency(base);
        m->setBaseCurrency(base);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    ft.restart();

    // make sure, the base currency is set
    try {
        ref = m->baseCurrency();
        QCOMPARE(ref.id(), QLatin1String("EUR"));
        QCOMPARE(ref.name(), QLatin1String("Euro"));
        QVERIFY(ref.tradingSymbol().at(0) == QChar(0x20ac));
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testOpeningBalanceNoBase()
{
    MyMoneyAccount openingAcc;
    MyMoneySecurity base;

    try {
        base = m->baseCurrency();
        openingAcc = m->openingBalanceAccount(base);
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
    }
}

void MyMoneyFileTest::testOpeningBalance()
{
    MyMoneyAccount openingAcc;
    MyMoneySecurity second("USD", "US Dollar", "$");
    setupBaseCurrency();

    try {
        openingAcc = m->openingBalanceAccount(m->baseCurrency());
        QCOMPARE(openingAcc.parentAccountId(), m->equity().id());
        QCOMPARE(openingAcc.name(), MyMoneyFile::openingBalancesPrefix());
        QCOMPARE(openingAcc.openingDate(), QDate::currentDate());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // add a second currency
    MyMoneyFileTransaction ft;
    try {
        m->addCurrency(second);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    QString refName = QString("%1 (%2)").arg(MyMoneyFile::openingBalancesPrefix(), QLatin1String("USD"));
    try {
        openingAcc = m->openingBalanceAccount(second);
        QCOMPARE(openingAcc.parentAccountId(), m->equity().id());
        QCOMPARE(openingAcc.name(), refName);
        QCOMPARE(openingAcc.openingDate(), QDate::currentDate());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testImbalanceNoBase()
{
    MyMoneyAccount imbalanceAcc;
    MyMoneySecurity base;

    try {
        base = m->baseCurrency();
        imbalanceAcc = m->imbalanceAccount(base);
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
    }
}

void MyMoneyFileTest::testImbalance()
{
    MyMoneyAccount imbalanceAcc;
    MyMoneySecurity second("USD", "US Dollar", "$");
    setupBaseCurrency();

    // a const lookup must throw as long as no imbalance account exists yet
    try {
        static_cast<const MyMoneyFile*>(m)->imbalanceAccount(m->baseCurrency());
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
    }

    // the non-const variant creates it lazily below the equity account
    try {
        imbalanceAcc = m->imbalanceAccount(m->baseCurrency());
        QCOMPARE(imbalanceAcc.parentAccountId(), m->equity().id());
        QCOMPARE(imbalanceAcc.name(), MyMoneyFile::imbalancePrefix());
        QVERIFY(imbalanceAcc.accountType() == eMyMoney::Account::Type::Equity);
        QCOMPARE(imbalanceAcc.value(QStringLiteral("ImbalanceAccount")), QStringLiteral("Yes"));
        QCOMPARE(imbalanceAcc.openingDate(), QDate::currentDate());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // calling it again returns the same account, not a new one
    try {
        QCOMPARE(m->imbalanceAccount(m->baseCurrency()).id(), imbalanceAcc.id());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // the imbalance account must be distinct from the opening balances account
    try {
        const auto openingAcc = m->openingBalanceAccount(m->baseCurrency());
        QVERIFY(openingAcc.id() != imbalanceAcc.id());
        // creating the opening balance account must not have changed the
        // imbalance account lookup
        QCOMPARE(m->imbalanceAccount(m->baseCurrency()).id(), imbalanceAcc.id());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // a second currency gets its own imbalance account with a " (XXX)" suffix
    MyMoneyFileTransaction ft;
    try {
        m->addCurrency(second);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    const QString refName = QString("%1 (%2)").arg(MyMoneyFile::imbalancePrefix(), QLatin1String("USD"));
    try {
        const auto secondAcc = m->imbalanceAccount(second);
        QCOMPARE(secondAcc.parentAccountId(), m->equity().id());
        QCOMPARE(secondAcc.name(), refName);
        QVERIFY(secondAcc.id() != imbalanceAcc.id());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testAutoBalanceTransaction()
{
    testAddAccounts();
    setupBaseCurrency();

    // with auto-balance enabled, a single-split transaction is balanced
    // against the imbalance account when added
    m->setAutoBalanceMode(true);

    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    t.setMemo(QStringLiteral("single split"));
    MyMoneySplit split1;
    split1.setAccountId(QStringLiteral("A000001"));
    split1.setShares(MyMoneyMoney(-1000, 100));
    split1.setValue(MyMoneyMoney(-1000, 100));
    t.addSplit(split1);

    MyMoneyFileTransaction ft;
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // the stored transaction now has two splits and is balanced
    const auto stored = m->transaction(t.id());
    QCOMPARE(stored.splitCount(), static_cast<uint>(2));
    QVERIFY(stored.splitSum().isZero());

    // the original split is unchanged ...
    const auto s1 = stored.splitByAccount(QStringLiteral("A000001"));
    QCOMPARE(s1.value(), MyMoneyMoney(-1000, 100));

    // ... and the counter split lands in the imbalance account
    const auto imbAcc = static_cast<const MyMoneyFile*>(m)->imbalanceAccount(m->baseCurrency());
    const auto s2 = stored.splitByAccount(imbAcc.id());
    QCOMPARE(s2.value(), MyMoneyMoney(1000, 100));

    m->setAutoBalanceMode(false);
}

void MyMoneyFileTest::testNoAutoBalanceWhenDisabled()
{
    testAddAccounts();
    setupBaseCurrency();

    // with auto-balance disabled (the default), a single-split transaction is
    // stored as-is and is not balanced
    m->setAutoBalanceMode(false);

    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    MyMoneySplit split1;
    split1.setAccountId(QStringLiteral("A000001"));
    split1.setShares(MyMoneyMoney(-500, 100));
    split1.setValue(MyMoneyMoney(-500, 100));
    t.addSplit(split1);

    MyMoneyFileTransaction ft;
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    const auto stored = m->transaction(t.id());
    QCOMPARE(stored.splitCount(), static_cast<uint>(1));
    QVERIFY(!stored.splitSum().isZero());
}

void MyMoneyFileTest::testModifyTransactionAutoBalanceIdempotency()
{
    testAddAccounts();
    setupBaseCurrency();
    m->setAutoBalanceMode(true);

    // add a single-split transaction; it is balanced to 2 splits
    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    MyMoneySplit split1;
    split1.setAccountId(QStringLiteral("A000001"));
    split1.setShares(MyMoneyMoney(-1000, 100));
    split1.setValue(MyMoneyMoney(-1000, 100));
    t.addSplit(split1);

    MyMoneyFileTransaction ft;
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    const QString tid = t.id();
    const auto imbAcc = static_cast<const MyMoneyFile*>(m)->imbalanceAccount(m->baseCurrency());
    QCOMPARE(m->transaction(tid).splitCount(), static_cast<uint>(2));
    QCOMPARE(m->transaction(tid).splitByAccount(imbAcc.id()).value(), MyMoneyMoney(1000, 100));

    // modify the asset split amount; the imbalance split must be replaced, not
    // duplicated, and the transaction must remain balanced with exactly 2 splits
    auto stored = m->transaction(tid);
    auto assetSplit = stored.splitByAccount(QStringLiteral("A000001"));
    assetSplit.setShares(MyMoneyMoney(-500, 100));
    assetSplit.setValue(MyMoneyMoney(-500, 100));
    stored.modifySplit(assetSplit);

    ft.restart();
    try {
        m->modifyTransaction(stored);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    const auto modified = m->transaction(tid);
    QCOMPARE(modified.splitCount(), static_cast<uint>(2));
    QVERIFY(modified.splitSum().isZero());
    QCOMPARE(modified.splitByAccount(imbAcc.id()).value(), MyMoneyMoney(500, 100));

    m->setAutoBalanceMode(false);
}

void MyMoneyFileTest::testReassignImbalanceToCategory()
{
    // LH-F-06: a transaction first parked on the imbalance account must be
    // freely re-assignable to a "real" category later, without data loss.
    testAddAccounts();
    setupBaseCurrency();

    // a real expense category to move the booking onto later
    MyMoneyAccount expense;
    expense.setName(QStringLiteral("Groceries"));
    expense.setAccountType(eMyMoney::Account::Type::Expense);
    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount expenseParent = m->expense();
        m->addAccount(expense, expenseParent);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    m->setAutoBalanceMode(true);

    // add a single-split transaction; it gets parked on the imbalance account
    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    t.setMemo(QStringLiteral("groceries payment"));
    MyMoneySplit split1;
    split1.setAccountId(QStringLiteral("A000001"));
    split1.setMemo(QStringLiteral("asset side"));
    split1.setShares(MyMoneyMoney(-1000, 100));
    split1.setValue(MyMoneyMoney(-1000, 100));
    t.addSplit(split1);

    ft.restart();
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    const QString tid = t.id();
    const auto imbAcc = static_cast<const MyMoneyFile*>(m)->imbalanceAccount(m->baseCurrency());
    QCOMPARE(m->transaction(tid).splitCount(), static_cast<uint>(2));
    QVERIFY(!m->transaction(tid).splitByAccount(imbAcc.id()).id().isEmpty());

    // the user now assigns the auto-balance split to the real expense category
    auto stored = m->transaction(tid);
    auto imbSplit = stored.splitByAccount(imbAcc.id());
    imbSplit.setAccountId(expense.id());
    stored.modifySplit(imbSplit);

    ft.restart();
    try {
        m->modifyTransaction(stored);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    const auto modified = m->transaction(tid);
    // still exactly two splits, balanced, and NO imbalance split remains
    QCOMPARE(modified.splitCount(), static_cast<uint>(2));
    QVERIFY(modified.splitSum().isZero());
    bool hasImbalanceSplit = false;
    const auto modifiedSplits = modified.splits();
    for (const auto& s : modifiedSplits) {
        if (s.accountId() == imbAcc.id())
            hasImbalanceSplit = true;
    }
    QVERIFY(!hasImbalanceSplit);

    // the booking now lives on the real category with the full amount ...
    const auto catSplit = modified.splitByAccount(expense.id());
    QCOMPARE(catSplit.value(), MyMoneyMoney(1000, 100));

    // ... and the original asset side is untouched (no data loss)
    const auto assetSplit = modified.splitByAccount(QStringLiteral("A000001"));
    QCOMPARE(assetSplit.value(), MyMoneyMoney(-1000, 100));
    QCOMPARE(assetSplit.memo(), QStringLiteral("asset side"));
    QCOMPARE(modified.memo(), QStringLiteral("groceries payment"));

    m->setAutoBalanceMode(false);
}

void MyMoneyFileTest::testCategorizedImportNotBalancedToImbalance()
{
    // LH-F-14: a statement import whose payee carries a default category arrives as an
    // already-balanced two-split transaction (asset + category). Auto-balance must leave
    // it alone — balanceTransactionToImbalance early-returns on a zero residual — and must
    // NOT append a third split on the imbalance account. (The uncategorized counterpart —
    // a lone asset split that lands on the imbalance account, LH-F-02/LH-F-12 — is covered
    // by testAutoBalanceTransaction.) This is a regression guard for the engine invariant
    // the SimpleMode import gates rely on; the reader gates themselves are GUI-bound and
    // not unit-testable (the reader does not link in tests).
    testAddAccounts();
    setupBaseCurrency();

    // a real expense category, as a payee's defaultAccountId would point to
    MyMoneyAccount expense;
    expense.setName(QStringLiteral("Groceries"));
    expense.setAccountType(eMyMoney::Account::Type::Expense);
    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount expenseParent = m->expense();
        m->addAccount(expense, expenseParent);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    m->setAutoBalanceMode(true);

    // the already-balanced two-split shape the statement reader builds from a payee
    // defaultAccountId: an asset side plus the category side summing to zero
    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    t.setMemo(QStringLiteral("categorized import"));
    MyMoneySplit assetSplit;
    assetSplit.setAccountId(QStringLiteral("A000001"));
    assetSplit.setShares(MyMoneyMoney(-1000, 100));
    assetSplit.setValue(MyMoneyMoney(-1000, 100));
    t.addSplit(assetSplit);
    MyMoneySplit categorySplit;
    categorySplit.setAccountId(expense.id());
    categorySplit.setShares(MyMoneyMoney(1000, 100));
    categorySplit.setValue(MyMoneyMoney(1000, 100));
    t.addSplit(categorySplit);

    ft.restart();
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    const auto stored = m->transaction(t.id());
    // exactly the two original splits, still balanced — no imbalance split was appended
    QCOMPARE(stored.splitCount(), static_cast<uint>(2));
    QVERIFY(stored.splitSum().isZero());
    // positive whitelist: every split is one of the two we added, which proves no third
    // split on the imbalance account exists — without calling imbalanceAccount(), whose
    // non-const overload would lazily create the account as a side effect.
    const auto storedSplits = stored.splits();
    for (const auto& s : storedSplits) {
        QVERIFY(s.accountId() == QStringLiteral("A000001") || s.accountId() == expense.id());
    }
    QCOMPARE(stored.splitByAccount(expense.id()).value(), MyMoneyMoney(1000, 100));

    m->setAutoBalanceMode(false);
}

void MyMoneyFileTest::testAutoBalanceMultiCurrency()
{
    setupBaseCurrency();

    // add a foreign currency and an asset account denominated in it
    MyMoneySecurity usd("USD", "US Dollar", "$");
    MyMoneyAccount usdAcc;
    usdAcc.setName(QStringLiteral("USD Cash"));
    usdAcc.setAccountType(eMyMoney::Account::Type::Asset);
    usdAcc.setCurrencyId(QStringLiteral("USD"));

    MyMoneyFileTransaction ft;
    try {
        m->addCurrency(usd);
        MyMoneyAccount assetParent = m->asset();
        m->addAccount(usdAcc, assetParent);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    m->setAutoBalanceMode(true);

    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    t.setCommodity(QStringLiteral("USD"));
    MyMoneySplit split1;
    split1.setAccountId(usdAcc.id());
    split1.setShares(MyMoneyMoney(-1000, 100));
    split1.setValue(MyMoneyMoney(-1000, 100));
    t.addSplit(split1);

    ft.restart();
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    const auto stored = m->transaction(t.id());
    QCOMPARE(stored.splitCount(), static_cast<uint>(2));
    QVERIFY(stored.splitSum().isZero());

    // the counter split must land in the USD-specific imbalance account, with
    // value == shares (no exchange rate involved)
    const auto imbUsd = static_cast<const MyMoneyFile*>(m)->imbalanceAccount(usd);
    QVERIFY(imbUsd.name().contains(QLatin1String("USD")));
    const auto s2 = stored.splitByAccount(imbUsd.id());
    QCOMPARE(s2.value(), MyMoneyMoney(1000, 100));
    QCOMPARE(s2.shares(), MyMoneyMoney(1000, 100));

    m->setAutoBalanceMode(false);
}

void MyMoneyFileTest::testConsistencyCheckAutoBalance()
{
    testAddAccounts();
    setupBaseCurrency();

    // create a legacy single-split (not balanced) transaction while auto-balance
    // is off, mimicking an imported uncategorized transaction
    m->setSimpleMode(false);
    m->setAutoBalanceMode(false);

    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    MyMoneySplit split1;
    split1.setAccountId(QStringLiteral("A000001"));
    split1.setShares(MyMoneyMoney(-2500, 100));
    split1.setValue(MyMoneyMoney(-2500, 100));
    t.addSplit(split1);

    MyMoneyFileTransaction ft;
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    const QString tid = t.id();
    QCOMPARE(m->transaction(tid).splitCount(), static_cast<uint>(1));

    // enable simplified mode + auto-balance and run the consistency check;
    // it must repair the legacy transaction instead of reporting it
    m->setSimpleMode(true);
    m->setAutoBalanceMode(true);

    ft.restart();
    try {
        m->consistencyCheck();
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    const auto fixed = m->transaction(tid);
    QCOMPARE(fixed.splitCount(), static_cast<uint>(2));
    QVERIFY(fixed.splitSum().isZero());
    const auto imbAcc = static_cast<const MyMoneyFile*>(m)->imbalanceAccount(m->baseCurrency());
    QCOMPARE(fixed.splitByAccount(imbAcc.id()).value(), MyMoneyMoney(2500, 100));

    m->setSimpleMode(false);
    m->setAutoBalanceMode(false);
}

void MyMoneyFileTest::testConsistencyCheckSimpleModeNoAutoBalance()
{
    testAddAccounts();
    setupBaseCurrency();

    // create an unbalanced single-split transaction (auto-balance off)
    m->setSimpleMode(false);
    m->setAutoBalanceMode(false);

    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    MyMoneySplit split1;
    split1.setAccountId(QStringLiteral("A000001"));
    split1.setShares(MyMoneyMoney(-700, 100));
    split1.setValue(MyMoneyMoney(-700, 100));
    t.addSplit(split1);

    MyMoneyFileTransaction ft;
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    const QString tid = t.id();

    // simplified mode ON but auto-balance OFF: the consistency check must NOT
    // repair the transaction (it is reported as information only)
    m->setSimpleMode(true);
    m->setAutoBalanceMode(false);

    ft.restart();
    try {
        m->consistencyCheck();
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    const auto stored = m->transaction(tid);
    QCOMPARE(stored.splitCount(), static_cast<uint>(1));
    QVERIFY(!stored.splitSum().isZero());

    m->setSimpleMode(false);
}

// Shared setup for the opening-date derivation tests: an asset account with a
// late opening date plus one balanced transaction dated well before it. Returns
// the asset account id.
static QString setupLateOpeningDateScenario(MyMoneyFile* m)
{
    MyMoneyAccount asset;
    asset.setName(QStringLiteral("DeriveAsset"));
    asset.setAccountType(eMyMoney::Account::Type::Asset);
    asset.setOpeningDate(QDate(2026, 1, 1));

    MyMoneyAccount category;
    category.setName(QStringLiteral("DeriveCategory"));
    category.setAccountType(eMyMoney::Account::Type::Expense);

    MyMoneyFileTransaction ft;
    MyMoneyAccount assetParent = m->asset();
    MyMoneyAccount expenseParent = m->expense();
    m->addAccount(asset, assetParent);
    m->addAccount(category, expenseParent);

    MyMoneyTransaction t;
    t.setPostDate(QDate(2013, 6, 15));
    MyMoneySplit s1;
    s1.setAccountId(asset.id());
    s1.setShares(MyMoneyMoney(-1000, 100));
    s1.setValue(MyMoneyMoney(-1000, 100));
    MyMoneySplit s2;
    s2.setAccountId(category.id());
    s2.setShares(MyMoneyMoney(1000, 100));
    s2.setValue(MyMoneyMoney(1000, 100));
    t.addSplit(s1);
    t.addSplit(s2);
    m->addTransaction(t);
    ft.commit();

    return asset.id();
}

void MyMoneyFileTest::testSimpleModeOpeningDateDerivation()
{
    setupBaseCurrency();
    QString assetId;
    try {
        assetId = setupLateOpeningDateScenario(m);
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    QCOMPARE(m->account(assetId).openingDate(), QDate(2026, 1, 1));

    // with simplified mode + derivation enabled, the consistency check lowers the
    // account's opening date to its earliest transaction
    m->setSimpleMode(true);
    m->setSimpleModeDeriveOpeningDate(true);
    m->setAutoBalanceMode(false);

    MyMoneyFileTransaction ft;
    try {
        m->consistencyCheck();
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    QCOMPARE(m->account(assetId).openingDate(), QDate(2013, 6, 15));

    m->setSimpleMode(false);
    m->setSimpleModeDeriveOpeningDate(false);
}

void MyMoneyFileTest::testDeriveOpeningDateDisabled()
{
    setupBaseCurrency();
    QString assetId;
    try {
        assetId = setupLateOpeningDateScenario(m);
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // simplified mode on, but derivation off: the opening date stays as-is
    m->setSimpleMode(true);
    m->setSimpleModeDeriveOpeningDate(false);
    m->setAutoBalanceMode(false);

    MyMoneyFileTransaction ft;
    try {
        m->consistencyCheck();
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    QCOMPARE(m->account(assetId).openingDate(), QDate(2026, 1, 1));

    m->setSimpleMode(false);
}

void MyMoneyFileTest::testModeSwitchReversibility()
{
    // LH-N-03: switching between simplified and full bookkeeping mode must be
    // lossless and must not corrupt data. The only SimpleMode behaviour that
    // writes data is auto-balancing (it appends an imbalance counter-split to an
    // under-specified transaction). This test proves that such a transaction is a
    // perfectly valid balanced transaction in full mode, that a full-mode
    // consistency check neither reports nor rewrites it, and that a complete
    // round-trip (simplified -> full -> simplified) preserves every amount and
    // memo without duplicating or dropping the imbalance split.
    testAddAccounts();
    setupBaseCurrency();

    // --- Phase 1: create an under-specified transaction in simplified mode ---
    m->setSimpleMode(true);
    m->setAutoBalanceMode(true);

    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    t.setMemo(QStringLiteral("round-trip booking"));
    MyMoneySplit assetSplit;
    assetSplit.setAccountId(QStringLiteral("A000001"));
    assetSplit.setMemo(QStringLiteral("asset side"));
    assetSplit.setShares(MyMoneyMoney(-3300, 100));
    assetSplit.setValue(MyMoneyMoney(-3300, 100));
    t.addSplit(assetSplit);

    MyMoneyFileTransaction ft;
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    const QString tid = t.id();
    const auto imbAcc = static_cast<const MyMoneyFile*>(m)->imbalanceAccount(m->baseCurrency());
    {
        const auto parked = m->transaction(tid);
        QCOMPARE(parked.splitCount(), static_cast<uint>(2));
        QVERIFY(parked.splitSum().isZero());
        QCOMPARE(parked.splitByAccount(imbAcc.id()).value(), MyMoneyMoney(3300, 100));
    }

    // --- Phase 2: switch to full bookkeeping mode; stored data must be untouched ---
    m->setSimpleMode(false);
    m->setAutoBalanceMode(false);
    m->setSimpleModeDeriveOpeningDate(false);

    {
        const auto inFull = m->transaction(tid);
        QCOMPARE(inFull.splitCount(), static_cast<uint>(2));
        QVERIFY(inFull.splitSum().isZero());
        QCOMPARE(inFull.memo(), QStringLiteral("round-trip booking"));
        QCOMPARE(inFull.splitByAccount(QStringLiteral("A000001")).value(), MyMoneyMoney(-3300, 100));
        QCOMPARE(inFull.splitByAccount(QStringLiteral("A000001")).memo(), QStringLiteral("asset side"));
        QCOMPARE(inFull.splitByAccount(imbAcc.id()).value(), MyMoneyMoney(3300, 100));
    }

    // a full-mode consistency check must NOT report or modify the auto-balanced
    // transaction (it is a balanced transaction like any other)
    ft.restart();
    try {
        m->consistencyCheck();
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    {
        const auto afterCheck = m->transaction(tid);
        QCOMPARE(afterCheck.splitCount(), static_cast<uint>(2));
        QVERIFY(afterCheck.splitSum().isZero());
        QCOMPARE(afterCheck.splitByAccount(QStringLiteral("A000001")).value(), MyMoneyMoney(-3300, 100));
        QCOMPARE(afterCheck.splitByAccount(imbAcc.id()).value(), MyMoneyMoney(3300, 100));
    }

    // editing the transaction in full mode (here: changing a memo) stays a normal
    // edit — no extra split is created, nothing is dropped
    {
        auto edit = m->transaction(tid);
        edit.setMemo(QStringLiteral("edited in full mode"));
        ft.restart();
        try {
            m->modifyTransaction(edit);
            ft.commit();
        } catch (const MyMoneyException& e) {
            unexpectedException(e);
        }
        const auto edited = m->transaction(tid);
        QCOMPARE(edited.splitCount(), static_cast<uint>(2));
        QVERIFY(edited.splitSum().isZero());
        QCOMPARE(edited.memo(), QStringLiteral("edited in full mode"));
    }

    // --- Phase 3: switch back to simplified mode and modify; the round-trip must
    //     stay idempotent (no duplicated imbalance split, no lost data) ---
    m->setSimpleMode(true);
    m->setAutoBalanceMode(true);

    {
        auto edit = m->transaction(tid);
        edit.setPostDate(QDate(2002, 3, 1));
        ft.restart();
        try {
            m->modifyTransaction(edit);
            ft.commit();
        } catch (const MyMoneyException& e) {
            unexpectedException(e);
        }
        const auto roundTripped = m->transaction(tid);
        // still exactly two splits — the prior imbalance split was reused, not
        // duplicated — balanced, with every amount preserved
        QCOMPARE(roundTripped.splitCount(), static_cast<uint>(2));
        QVERIFY(roundTripped.splitSum().isZero());
        QCOMPARE(roundTripped.postDate(), QDate(2002, 3, 1));
        QCOMPARE(roundTripped.splitByAccount(QStringLiteral("A000001")).value(), MyMoneyMoney(-3300, 100));
        QCOMPARE(roundTripped.splitByAccount(QStringLiteral("A000001")).memo(), QStringLiteral("asset side"));
        QCOMPARE(roundTripped.splitByAccount(imbAcc.id()).value(), MyMoneyMoney(3300, 100));
        // and no second imbalance split sneaked in
        int imbalanceSplits = 0;
        const auto rtSplits = roundTripped.splits();
        for (const auto& s : rtSplits) {
            if (s.accountId() == imbAcc.id())
                ++imbalanceSplits;
        }
        QCOMPARE(imbalanceSplits, 1);
    }

    m->setSimpleMode(false);
    m->setAutoBalanceMode(false);
}

void MyMoneyFileTest::testEnableSimpleModeKeepsFullModeDataIntact()
{
    // LH-N-03 / LH-N-05: enabling simplified mode on a file authored in full mode
    // must not retroactively rewrite existing, already-balanced transactions.
    // Auto-balancing only runs when a transaction is added, modified, or repaired
    // by the consistency check; flipping the mode flag alone changes nothing.
    testAddAccounts();
    setupBaseCurrency();

    MyMoneyAccount expense;
    expense.setName(QStringLiteral("Groceries"));
    expense.setAccountType(eMyMoney::Account::Type::Expense);
    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount expenseParent = m->expense();
        m->addAccount(expense, expenseParent);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // a fully categorized, balanced two-split transaction created in full mode
    m->setSimpleMode(false);
    m->setAutoBalanceMode(false);

    MyMoneyTransaction t;
    t.setPostDate(QDate(2002, 2, 1));
    MyMoneySplit assetSplit;
    assetSplit.setAccountId(QStringLiteral("A000001"));
    assetSplit.setShares(MyMoneyMoney(-1500, 100));
    assetSplit.setValue(MyMoneyMoney(-1500, 100));
    t.addSplit(assetSplit);
    MyMoneySplit categorySplit;
    categorySplit.setAccountId(expense.id());
    categorySplit.setShares(MyMoneyMoney(1500, 100));
    categorySplit.setValue(MyMoneyMoney(1500, 100));
    t.addSplit(categorySplit);

    ft.restart();
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    const QString tid = t.id();

    // enabling simplified mode + auto-balance must NOT touch the stored transaction
    m->setSimpleMode(true);
    m->setAutoBalanceMode(true);

    {
        const auto unchanged = m->transaction(tid);
        QCOMPARE(unchanged.splitCount(), static_cast<uint>(2));
        QVERIFY(unchanged.splitSum().isZero());
        QCOMPARE(unchanged.splitByAccount(expense.id()).value(), MyMoneyMoney(1500, 100));
    }

    // a simplified-mode consistency check leaves the already-balanced transaction
    // alone (no imbalance split appended)
    ft.restart();
    try {
        m->consistencyCheck();
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
    {
        const auto afterCheck = m->transaction(tid);
        QCOMPARE(afterCheck.splitCount(), static_cast<uint>(2));
        QVERIFY(afterCheck.splitSum().isZero());
        // positive whitelist: the two original splits only — no imbalance split
        const auto splits = afterCheck.splits();
        for (const auto& s : splits) {
            QVERIFY(s.accountId() == QStringLiteral("A000001") || s.accountId() == expense.id());
        }
    }

    m->setSimpleMode(false);
    m->setAutoBalanceMode(false);
}

void MyMoneyFileTest::testModifyStdAccount()
{
    QVERIFY(m->asset().currencyId().isEmpty());
    QCOMPARE(m->asset().name(), QLatin1String("Asset accounts"));
    setupBaseCurrency();
    // testBaseCurrency();
    QVERIFY(m->asset().currencyId().isEmpty());
    QVERIFY(!m->baseCurrency().id().isEmpty());

    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount acc = m->asset();
        acc.setName("Anlagen");
        acc.setCurrencyId(m->baseCurrency().id());
        m->modifyAccount(acc);
        ft.commit();

        QCOMPARE(m->asset().name(), QLatin1String("Anlagen"));
        QCOMPARE(m->asset().currencyId(), m->baseCurrency().id());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    ft.restart();
    try {
        MyMoneyAccount acc = m->asset();
        acc.setNumber("Test");
        m->modifyAccount(acc);
        QFAIL("Missing expected exception");
    } catch (const MyMoneyException&) {
        ft.rollback();
    }
}

void MyMoneyFileTest::testAddPrice()
{
    testAddAccounts();
    setupBaseCurrency();
    MyMoneyAccount p;

    MyMoneyFileTransaction ft;
    // make sure the currency exists
    MyMoneySecurity foreignCurrency("RON", "Romanian Leu (new)", "RON");
    try {
        m->currency(foreignCurrency.id());
    } catch (const MyMoneyException&) {
        m->addCurrency(foreignCurrency);
    }
    ft.commit();

    clearObjectLists();
    ft.restart();
    try {
        p = m->account("A000002");
        p.setCurrencyId("RON");
        m->modifyAccount(p);
        ft.commit();

        QCOMPARE(m->account("A000002").currencyId(), QLatin1String("RON"));
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    clearObjectLists();
    ft.restart();
    MyMoneyPrice price("EUR", "RON", QDate::currentDate(), MyMoneyMoney(4.1), "Test source");
    m->addPrice(price);
    ft.commit();
    QCOMPARE(m_balanceChanged.count(), 0);
    QCOMPARE(m_valueChanged.count(), 1);
    QCOMPARE(m_valueChanged.count("A000002"), 1);

    clearObjectLists();
    ft.restart();
    MyMoneyPrice priceReciprocal("RON", "EUR", QDate::currentDate(), MyMoneyMoney(1 / 4.1), "Test source reciprocal price");
    m->addPrice(priceReciprocal);
    ft.commit();
    QCOMPARE(m_balanceChanged.count(), 0);
    QCOMPARE(m_valueChanged.count(), 1);
    QCOMPARE(m_valueChanged.count("A000002"), 1);
}

void MyMoneyFileTest::testRemovePrice()
{
    testAddPrice();
    clearObjectLists();
    MyMoneyFileTransaction ft;
    MyMoneyPrice price("EUR", "RON", QDate::currentDate(), MyMoneyMoney(4.1), "Test source");
    m->removePrice(price);
    ft.commit();
    QCOMPARE(m_balanceChanged.count(), 0);
    QCOMPARE(m_valueChanged.count(), 1);
    QCOMPARE(m_valueChanged.count("A000002"), 1);
}

void MyMoneyFileTest::testGetPrice()
{
    testAddPrice();
    // the price for the current date is found
    QVERIFY(m->price("EUR", "RON", QDate::currentDate()).isValid());
    // the price for the current date is returned when asking for the next day with exact date set to false
    {
        const MyMoneyPrice& price = m->price("EUR", "RON", QDate::currentDate().addDays(1), false);
        QVERIFY(price.isValid() && price.date() == QDate::currentDate());
    }
    // no price is returned while asking for the next day with exact date set to true
    QVERIFY(!m->price("EUR", "RON", QDate::currentDate().addDays(1), true).isValid());

    // no price is returned while asking for the previous day with exact date set to true/false because all prices are newer
    QVERIFY(!m->price("EUR", "RON", QDate::currentDate().addDays(-1), false).isValid());
    QVERIFY(!m->price("EUR", "RON", QDate::currentDate().addDays(-1), true).isValid());

    // add two more prices
    MyMoneyFileTransaction ft;
    m->addPrice(MyMoneyPrice("EUR", "RON", QDate::currentDate().addDays(3), MyMoneyMoney(4.1), "Test source"));
    m->addPrice(MyMoneyPrice("EUR", "RON", QDate::currentDate().addDays(5), MyMoneyMoney(4.1), "Test source"));
    ft.commit();
    clearObjectLists();

    // extra tests for the exactDate=false behavior
    {
        const MyMoneyPrice& price = m->price("EUR", "RON", QDate::currentDate().addDays(2), false);
        QVERIFY(price.isValid() && price.date() == QDate::currentDate());
    }
    {
        const MyMoneyPrice& price = m->price("EUR", "RON", QDate::currentDate().addDays(3), false);
        QVERIFY(price.isValid() && price.date() == QDate::currentDate().addDays(3));
    }
    {
        const MyMoneyPrice& price = m->price("EUR", "RON", QDate::currentDate().addDays(4), false);
        QVERIFY(price.isValid() && price.date() == QDate::currentDate().addDays(3));
    }
    {
        const MyMoneyPrice& price = m->price("EUR", "RON", QDate::currentDate().addDays(5), false);
        QVERIFY(price.isValid() && price.date() == QDate::currentDate().addDays(5));
    }
    {
        const MyMoneyPrice& price = m->price("EUR", "RON", QDate::currentDate().addDays(6), false);
        QVERIFY(price.isValid() && price.date() == QDate::currentDate().addDays(5));
    }
}

void MyMoneyFileTest::testAddAccountMissingCurrency()
{
    testAddTwoInstitutions();
    MyMoneySecurity base("EUR", "Euro", QChar(0x20ac));
    MyMoneyAccount a;
    a.setAccountType(eMyMoney::Account::Type::Checkings);

    MyMoneyInstitution institution;

    m->setDirty(false);

    QCOMPARE(m->accountsModel()->itemList().count(), 0);

    institution = m->institution("I000001");
    QCOMPARE(institution.id(), QLatin1String("I000001"));

    a.setName("Account1");
    a.setInstitutionId(institution.id());

    clearObjectLists();
    MyMoneyFileTransaction ft;
    try {
        m->addCurrency(base);
        m->setBaseCurrency(base);
        MyMoneyAccount parent = m->asset();
        m->addAccount(a, parent);
        ft.commit();
        QCOMPARE(m->account("A000001").currencyId(), QLatin1String("EUR"));
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testAddTransactionToClosedAccount()
{
    QSKIP("Test not implemented yet", SkipAll);
}

void MyMoneyFileTest::testRemoveTransactionFromClosedAccount()
{
    QSKIP("Test not implemented yet", SkipAll);
}

void MyMoneyFileTest::testModifyTransactionInClosedAccount()
{
    QSKIP("Test not implemented yet", SkipAll);
}

void MyMoneyFileTest::testStorageId()
{
    // make sure id will be setup if it does not exist
    MyMoneyFileTransaction ft;
    try {
        m->setValue("kmm-id", "");
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    try {
        // check for a new id
        auto id = m->storageId();
        QVERIFY(!id.isNull());
        // check that it is the same if we ask again
        QCOMPARE(id, m->storageId());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testHasMatchingOnlineBalance_emptyAccountWithoutImportedBalance()
{
    addOneAccount();

    MyMoneyAccount a = m->account("A000001");

    QCOMPARE(m->hasMatchingOnlineBalance(a), false);
}

void MyMoneyFileTest::testHasMatchingOnlineBalance_emptyAccountWithEqualImportedBalance()
{
    addOneAccount();

    MyMoneyAccount a = m->account("A000001");

    a.setValue("lastImportedTransactionDate", QDate(2011, 12, 1).toString(Qt::ISODate));
    a.setValue("lastStatementBalance", MyMoneyMoney().toString());

    MyMoneyFileTransaction ft;
    m->modifyAccount(a);
    ft.commit();

    QCOMPARE(m->hasMatchingOnlineBalance(a), true);
}

void MyMoneyFileTest::testHasMatchingOnlineBalance_emptyAccountWithUnequalImportedBalance()
{
    addOneAccount();

    MyMoneyAccount a = m->account("A000001");

    a.setValue("lastImportedTransactionDate", QDate(2011, 12, 1).toString(Qt::ISODate));
    a.setValue("lastStatementBalance", MyMoneyMoney::ONE.toString());

    MyMoneyFileTransaction ft;
    m->modifyAccount(a);
    ft.commit();

    QCOMPARE(m->hasMatchingOnlineBalance(a), false);
}

void MyMoneyFileTest::testHasNewerTransaction_withoutAnyTransaction_afterLastImportedTransaction()
{
    addOneAccount();

    MyMoneyAccount a = m->account("A000001");

    QDate dateOfLastTransactionImport(2011, 12, 1);

    // There are no transactions at all:
    QCOMPARE(m->hasNewerTransaction(a.id(), dateOfLastTransactionImport), false);
}

void MyMoneyFileTest::testHasNewerTransaction_withoutNewerTransaction_afterLastImportedTransaction()
{
    addOneAccount();
    setupBaseCurrency();

    QString accId("A000001");
    QDate dateOfLastTransactionImport(2011, 12, 1);

    MyMoneyFileTransaction ft;
    MyMoneyTransaction t;

    // construct a transaction at the day of the last transaction import and add it to the pool
    t.setPostDate(dateOfLastTransactionImport);

    MyMoneySplit split1;

    split1.setAccountId(accId);
    split1.setShares(MyMoneyMoney(-1000, 100));
    split1.setValue(MyMoneyMoney(-1000, 100));
    t.addSplit(split1);

    ft.restart();
    m->addTransaction(t);
    ft.commit();

    QCOMPARE(m->hasNewerTransaction(accId, dateOfLastTransactionImport), false);
}

void MyMoneyFileTest::testHasNewerTransaction_withNewerTransaction_afterLastImportedTransaction()
{
    addOneAccount();
    setupBaseCurrency();

    QString accId("A000001");
    QDate dateOfLastTransactionImport(2011, 12, 1);
    QDate dateOfDayAfterLastTransactionImport(dateOfLastTransactionImport.addDays(1));

    MyMoneyFileTransaction ft;
    MyMoneyTransaction t;

    // construct a transaction a day after the last transaction import and add it to the pool
    t.setPostDate(dateOfDayAfterLastTransactionImport);

    MyMoneySplit split1;

    split1.setAccountId(accId);
    split1.setShares(MyMoneyMoney(-1000, 100));
    split1.setValue(MyMoneyMoney(-1000, 100));
    t.addSplit(split1);

    ft.restart();
    m->addTransaction(t);
    ft.commit();

    QCOMPARE(m->hasNewerTransaction(accId, dateOfLastTransactionImport), true);
}

void MyMoneyFileTest::addOneAccount()
{
    setupBaseCurrency();
    QString accountId = "A000001";
    MyMoneyAccount a;
    a.setAccountType(eMyMoney::Account::Type::Checkings);

    m->setDirty(false);

    QCOMPARE(m->accountsModel()->itemList().count(), 0);

    a.setName("Account1");
    a.setCurrencyId("EUR");

    clearObjectLists();
    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount parent = m->asset();
        m->addAccount(a, parent);
        ft.commit();
        QCOMPARE(m->accountsModel()->itemList().count(), 1);
        QCOMPARE(a.parentAccountId(), QLatin1String("AStd::Asset"));
        QCOMPARE(a.id(), accountId);
        QCOMPARE(a.currencyId(), QLatin1String("EUR"));
        QCOMPARE(m->dirty(), true);
        QCOMPARE(m->asset().accountList().count(), 1);
        QCOMPARE(m->asset().accountList()[0], accountId);

        QCOMPARE(m_objectsRemoved.count(), 0);
        QCOMPARE(m_objectsAdded.count(), 1);
        QCOMPARE(m_objectsModified.count(), 1);
        QCOMPARE(m_balanceChanged.count(), 0);
        QCOMPARE(m_valueChanged.count(), 0);
        QVERIFY(m_objectsAdded.contains(accountId.toLatin1()));
        QVERIFY(m_objectsModified.contains(QLatin1String("AStd::Asset")));

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testCountTransactionsWithSpecificReconciliationState_noTransactions()
{
    addOneAccount();
    QString accountId = "A000001";

    QCOMPARE(m->countTransactionsWithSpecificReconciliationState(accountId, eMyMoney::TransactionFilter::State::NotReconciled), 0);
}

void MyMoneyFileTest::testCountTransactionsWithSpecificReconciliationState_transactionWithWantedReconcileState()
{
    addOneAccount();
    setupBaseCurrency();

    QString accountId = "A000001";

    // construct split & transaction
    MyMoneySplit split;
    split.setAccountId(accountId);
    split.setShares(MyMoneyMoney(-1000, 100));
    split.setValue(MyMoneyMoney(-1000, 100));

    MyMoneyTransaction transaction;
    transaction.setPostDate(QDate(2013, 1, 1));
    transaction.addSplit(split);

    // add transaction
    MyMoneyFileTransaction ft;
    m->addTransaction(transaction);
    ft.commit();

    QCOMPARE(m->countTransactionsWithSpecificReconciliationState(accountId, eMyMoney::TransactionFilter::State::NotReconciled), 1);
}

void MyMoneyFileTest::testCountTransactionsWithSpecificReconciliationState_transactionWithUnwantedReconcileState()
{
    addOneAccount();
    setupBaseCurrency();

    QString accountId = "A000001";

    // construct split & transaction
    MyMoneySplit split;
    split.setAccountId(accountId);
    split.setShares(MyMoneyMoney(-1000, 100));
    split.setValue(MyMoneyMoney(-1000, 100));
    split.setReconcileFlag(eMyMoney::Split::State::Reconciled);

    MyMoneyTransaction transaction;
    transaction.setPostDate(QDate(2013, 1, 1));
    transaction.addSplit(split);

    // add transaction
    MyMoneyFileTransaction ft;
    m->addTransaction(transaction);
    ft.commit();

    QCOMPARE(m->countTransactionsWithSpecificReconciliationState(accountId, eMyMoney::TransactionFilter::State::NotReconciled), 0);
}

void MyMoneyFileTest::testAddOnlineJob()
{
    QSKIP("Need dummy task for this test", SkipAll);
#if 0
    // Add a onlineJob
    onlineJob job(new germanOnlineTransfer());

    MyMoneyFileTransaction ft;
    m->addOnlineJob(job);
    QCOMPARE(job.id(), QString("O000001"));
    ft.commit();

    QCOMPARE(m_objectsRemoved.count(), 0);
    QCOMPARE(m_objectsAdded.count(), 1);
    QCOMPARE(m_objectsModified.count(), 0);
    QCOMPARE(m_balanceChanged.count(), 0);
    QCOMPARE(m_valueChanged.count(), 0);
#endif
}

void MyMoneyFileTest::testGetOnlineJob()
{
    QSKIP("Need dummy task for this test", SkipAll);
#if 0
    testAddOnlineJob();

    const onlineJob requestedJob = m->getOnlineJob("O000001");
    QVERIFY(!requestedJob.isNull());
    QCOMPARE(requestedJob.id(), QString("O000001"));
#endif
}

void MyMoneyFileTest::testRemoveOnlineJob()
{
    QSKIP("Need dummy task for this test", SkipAll);
#if 0
    // Add a onlineJob
    onlineJob job(new germanOnlineTransfer());
    onlineJob job2(new germanOnlineTransfer());
    onlineJob job3(new germanOnlineTransfer());


    MyMoneyFileTransaction ft;
    m->addOnlineJob(job);
    m->addOnlineJob(job2);
    m->addOnlineJob(job3);
    ft.commit();

    clearObjectLists();

    ft.restart();
    m->removeOnlineJob(job);
    m->removeOnlineJob(job2);
    ft.commit();

    QCOMPARE(m_objectsRemoved.count(), 2);
    QCOMPARE(m_objectsAdded.count(), 0);
    QCOMPARE(m_objectsModified.count(), 0);
    QCOMPARE(m_balanceChanged.count(), 0);
    QCOMPARE(m_valueChanged.count(), 0);
#endif
}

void MyMoneyFileTest::testOnlineJobRollback()
{
    QSKIP("Need dummy task for this test", SkipAll);
#if 0
    // Add a onlineJob
    onlineJob job(new germanOnlineTransfer());
    onlineJob job2(new germanOnlineTransfer());
    onlineJob job3(new germanOnlineTransfer());

    MyMoneyFileTransaction ft;
    m->addOnlineJob(job);
    m->addOnlineJob(job2);
    m->addOnlineJob(job3);
    ft.rollback();

    QCOMPARE(m_objectsRemoved.count(), 0);
    QCOMPARE(m_objectsAdded.count(), 0);
    QCOMPARE(m_objectsModified.count(), 0);
    QCOMPARE(m_balanceChanged.count(), 0);
    QCOMPARE(m_valueChanged.count(), 0);
#endif
}

void MyMoneyFileTest::testRemoveLockedOnlineJob()
{
    QSKIP("Need dummy task for this test", SkipAll);
#if 0
    // Add a onlineJob
    onlineJob job(new germanOnlineTransfer());
    job.setLock(true);
    QVERIFY(job.isLocked());

    MyMoneyFileTransaction ft;
    m->addOnlineJob(job);
    ft.commit();

    clearObjectLists();

    // Try removing locked transfer
    ft.restart();
    m->removeOnlineJob(job);
    ft.commit();
    QVERIFY2(m_objectsRemoved.count() == 0, "Online Job was locked, removing is not allowed");
    QVERIFY(m_objectsAdded.count() == 0);
    QVERIFY(m_objectsModified.count() == 0);
    QVERIFY(m_balanceChanged.count() == 0);
    QVERIFY(m_valueChanged.count() == 0);

#endif
}

/** @todo */
void MyMoneyFileTest::testModifyOnlineJob()
{
    QSKIP("Need dummy task for this test", SkipAll);
#if 0
    // Add a onlineJob
    onlineJob job(new germanOnlineTransfer());
    MyMoneyFileTransaction ft;
    m->addOnlineJob(job);
    ft.commit();

    clearObjectLists();

    // Modify online job
    job.setJobSend();
    ft.restart();
    m->modifyOnlineJob(job);
    ft.commit();

    QCOMPARE(m_objectsRemoved.count(), 0);
    QCOMPARE(m_objectsAdded.count(), 0);
    QCOMPARE(m_objectsModified.count(), 1);
    QCOMPARE(m_balanceChanged.count(), 0);
    QCOMPARE(m_valueChanged.count(), 0);

    //onlineJob modifiedJob = m->getOnlineJob( job.id() );
    //QCOMPARE(modifiedJob.responsibleAccount(), QString("Std::Assert"));
#endif
}

void MyMoneyFileTest::testClearedBalance()
{
    testAddTransaction();
    MyMoneyTransaction t1;
    MyMoneyTransaction t2;

    // construct a transaction and add it to the pool
    t1.setPostDate(QDate(2002, 2, 1));
    t1.setMemo("Memotext");

    t2.setPostDate(QDate(2002, 2, 4));
    t2.setMemo("Memotext");

    MyMoneySplit split1;
    MyMoneySplit split2;
    MyMoneySplit split3;
    MyMoneySplit split4;

    MyMoneyFileTransaction ft;
    try {
        split1.setAccountId("A000002");
        split1.setShares(MyMoneyMoney(-1000, 100));
        split1.setValue(MyMoneyMoney(-1000, 100));
        split1.setReconcileFlag(eMyMoney::Split::State::Cleared);
        split2.setAccountId("A000004");
        split2.setValue(MyMoneyMoney(1000, 100));
        split2.setShares(MyMoneyMoney(1000, 100));
        split2.setReconcileFlag(eMyMoney::Split::State::Cleared);
        t1.addSplit(split1);
        t1.addSplit(split2);
        m->addTransaction(t1);
        ft.commit();
        ft.restart();
        QCOMPARE(t1.id(), QLatin1String("T000000000000000002"));
        split3.setAccountId("A000002");
        split3.setShares(MyMoneyMoney(-2000, 100));
        split3.setValue(MyMoneyMoney(-2000, 100));
        split3.setReconcileFlag(eMyMoney::Split::State::Cleared);
        split4.setAccountId("A000004");
        split4.setValue(MyMoneyMoney(2000, 100));
        split4.setShares(MyMoneyMoney(2000, 100));
        split4.setReconcileFlag(eMyMoney::Split::State::Cleared);
        t2.addSplit(split3);
        t2.addSplit(split4);
        m->addTransaction(t2);
        ft.commit();
        ft.restart();

        QCOMPARE(m->balance("A000001", QDate(2002, 2, 4)), MyMoneyMoney(-1000, 100));
        QCOMPARE(m->balance("A000002", QDate(2002, 2, 4)), MyMoneyMoney(-3000, 100));
        // Date of last cleared transaction
        QCOMPARE(m->clearedBalance("A000002", QDate(2002, 2, 1)), MyMoneyMoney(-1000, 100));

        // Date of last transaction
        QCOMPARE(m->balance("A000002", QDate(2002, 2, 4)), MyMoneyMoney(-3000, 100));

        // Date before first transaction
        QVERIFY(m->clearedBalance("A000002", QDate(2002, 1, 15)).isZero());

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testAdjustedValues()
{
    // create a checking account, an expense, an investment account and a stock
    addOneAccount();

    MyMoneyAccount exp1;
    exp1.setAccountType(eMyMoney::Account::Type::Expense);
    exp1.setName("Expense1");
    exp1.setCurrencyId("EUR");

    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount parent = m->expense();
        m->addAccount(exp1, parent);
        ft.commit();
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    testAddEquityAccount();

    MyMoneySecurity stockSecurity(QLatin1String("Blubber"), QLatin1String("TestStockSecurity"), QLatin1String("BLUB"), 1000, 1000, 4);
    stockSecurity.setTradingCurrency(QLatin1String("BLUB"));
    MyMoneySecurity tradingCurrency("BLUB", "BlubCurrency");

    // add the security
    ft.restart();
    try {
        m->addCurrency(tradingCurrency);
        m->addSecurity(stockSecurity);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    MyMoneyAccount i = m->accountByName("Investment");
    MyMoneyAccount stock;
    ft.restart();
    try {
        stock.setName("Teststock");
        stock.setCurrencyId(stockSecurity.id());
        stock.setAccountType(eMyMoney::Account::Type::Stock);
        m->addAccount(stock, i);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // values taken from real example on https://bugs.kde.org/show_bug.cgi?id=345655
    MyMoneySplit s1, s2, s3;
    s1.setAccountId(QLatin1String("A000001"));
    s1.setShares(MyMoneyMoney(QLatin1String("-99901/1000")));
    s1.setValue(MyMoneyMoney(QLatin1String("-999/10")));

    s2.setAccountId(exp1.id());
    s2.setShares(MyMoneyMoney(QLatin1String("-611/250")));
    s2.setValue(MyMoneyMoney(QLatin1String("-61/25")));

    s3.setAccountId(stock.id());
    s3.setAction(eMyMoney::Split::InvestmentTransactionType::BuyShares);
    s3.setShares(MyMoneyMoney(QLatin1String("64901/100000")));
    s3.setPrice(MyMoneyMoney(QLatin1String("157689/1000")));
    s3.setValue(MyMoneyMoney(QLatin1String("102340161/1000000")));

    MyMoneyTransaction t;
    t.setCommodity(QLatin1String("EUR"));
    t.setPostDate(QDate::currentDate());
    t.addSplit(s1);
    t.addSplit(s2);
    t.addSplit(s3);

    // make sure the split sum is not zero
    QVERIFY(!t.splitSum().isZero());

    ft.restart();
    try {
        m->addTransaction(t);
        ft.commit();
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    QCOMPARE(t.splitById(s1.id()).shares(), MyMoneyMoney(QLatin1String("-999/10")));
    QCOMPARE(t.splitById(s1.id()).value(), MyMoneyMoney(QLatin1String("-999/10")));

    QCOMPARE(t.splitById(s2.id()).shares(), MyMoneyMoney(QLatin1String("-61/25")));
    QCOMPARE(t.splitById(s2.id()).value(), MyMoneyMoney(QLatin1String("-61/25")));

    QCOMPARE(t.splitById(s3.id()).shares(), MyMoneyMoney(QLatin1String("649/1000")));
    QCOMPARE(t.splitById(s3.id()).value(), MyMoneyMoney(QLatin1String("10234/100")));
    QCOMPARE(t.splitById(s3.id()).price(), MyMoneyMoney(QLatin1String("157689/1000")));
    QCOMPARE(t.splitSum(), MyMoneyMoney());

    // now reset and check if modify also works
    s1.setShares(MyMoneyMoney(QLatin1String("-999/10")));
    s1.setValue(MyMoneyMoney(QLatin1String("-999/10")));

    s2.setShares(MyMoneyMoney(QLatin1String("-61/25")));
    s2.setValue(MyMoneyMoney(QLatin1String("-61/25")));

    s3.setShares(MyMoneyMoney(QLatin1String("649/1000")));
    s3.setPrice(MyMoneyMoney(QLatin1String("157689/1000")));
    s3.setValue(MyMoneyMoney(QLatin1String("102340161/1000000")));

    t.modifySplit(s1);
    t.modifySplit(s2);
    t.modifySplit(s3);

    // make sure the split sum is not zero
    QVERIFY(!t.splitSum().isZero());

    ft.restart();
    try {
        m->modifyTransaction(t);
        ft.commit();
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // we need to get the transaction from the engine, as modifyTransaction does
    // not return the modified values
    MyMoneyTransaction t2 = m->transaction(t.id());

    QCOMPARE(t2.splitById(s3.id()).shares(), MyMoneyMoney(QLatin1String("649/1000")));
    QCOMPARE(t2.splitById(s3.id()).value(), MyMoneyMoney(QLatin1String("10234/100")));
    QCOMPARE(t2.splitById(s3.id()).price(), MyMoneyMoney(QLatin1String("157689/1000")));
    QCOMPARE(t2.splitSum(), MyMoneyMoney());
}

void MyMoneyFileTest::testVatAssignment()
{
    MyMoneyAccount acc;
    MyMoneyAccount vat;
    MyMoneyAccount expense;

    testAddTransaction();

    vat.setName("VAT");
    vat.setCurrencyId("EUR");
    vat.setAccountType(eMyMoney::Account::Type::Expense);
    // make it a VAT account
    vat.setValue(QLatin1String("VatRate"), QLatin1String("20/100"));

    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount parent = m->expense();
        m->addAccount(vat, parent);
        QVERIFY(!vat.id().isEmpty());
        acc = m->account(QLatin1String("A000001"));
        expense = m->account(QLatin1String("A000003"));
        QCOMPARE(acc.name(), QLatin1String("Account1"));
        QCOMPARE(expense.name(), QLatin1String("Expense1"));
        expense.setValue(QLatin1String("VatAccount"), vat.id());
        m->modifyAccount(expense);
        ft.commit();
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // the categories are setup now for gross value entry
    MyMoneyTransaction tr;
    MyMoneySplit sp;
    MyMoneyMoney amount(1707, 100);

    // setup the transaction
    sp.setShares(amount);
    sp.setValue(amount);
    sp.setAccountId(acc.id());
    tr.addSplit(sp);
    sp.clearId();
    sp.setShares(-amount);
    sp.setValue(-amount);
    sp.setAccountId(expense.id());
    tr.addSplit(sp);

    QCOMPARE(m->addVATSplit(tr, acc, expense, amount), true);
    QCOMPARE(tr.splits().count(), 3);
    QCOMPARE(tr.splitByAccount(acc.id()).shares().toString(), MyMoneyMoney(1707, 100).toString());
    QCOMPARE(tr.splitByAccount(expense.id()).shares().toString(), MyMoneyMoney(-1422, 100).toString());
    QCOMPARE(tr.splitByAccount(vat.id()).shares().toString(), MyMoneyMoney(-285, 100).toString());
    QCOMPARE(tr.splitSum().toString(), MyMoneyMoney().toString());

    tr.removeSplits();
    ft.restart();
    try {
        expense.setValue(QLatin1String("VatAmount"), QLatin1String("net"));
        m->modifyAccount(expense);
        ft.commit();
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    // the categories are setup now for net value entry
    amount = MyMoneyMoney(1422, 100);
    sp.clearId();
    sp.setShares(amount);
    sp.setValue(amount);
    sp.setAccountId(acc.id());
    tr.addSplit(sp);
    sp.clearId();
    sp.setShares(-amount);
    sp.setValue(-amount);
    sp.setAccountId(expense.id());
    tr.addSplit(sp);

    QCOMPARE(m->addVATSplit(tr, acc, expense, amount), true);
    QCOMPARE(tr.splits().count(), 3);
    QCOMPARE(tr.splitByAccount(acc.id()).shares().toString(), MyMoneyMoney(1706, 100).toString());
    QCOMPARE(tr.splitByAccount(expense.id()).shares().toString(), MyMoneyMoney(-1422, 100).toString());
    QCOMPARE(tr.splitByAccount(vat.id()).shares().toString(), MyMoneyMoney(-284, 100).toString());
    QCOMPARE(tr.splitSum().toString(), MyMoneyMoney().toString());
}

void MyMoneyFileTest::testEmptyFilter()
{
    testAddTransaction();

    try {
        QList<QPair<MyMoneyTransaction, MyMoneySplit>> tList;
        MyMoneyTransactionFilter filter;
        MyMoneyFile::instance()->transactionList(tList, filter);
        QCOMPARE(tList.count(), 2);

    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }
}

void MyMoneyFileTest::testAddSecurity()
{
    // create a checking account, an expense, an investment account and a stock
    addOneAccount();

    MyMoneyAccount exp1;
    exp1.setAccountType(eMyMoney::Account::Type::Expense);
    exp1.setName("Expense1");
    exp1.setCurrencyId("EUR");

    MyMoneyFileTransaction ft;
    try {
        MyMoneyAccount parent = m->expense();
        m->addAccount(exp1, parent);
        ft.commit();
    } catch (const MyMoneyException&) {
        QFAIL("Unexpected exception!");
    }

    testAddEquityAccount();

    MyMoneySecurity stockSecurity(QLatin1String("Blubber"), QLatin1String("TestsockSecurity"), QLatin1String("BLUB"), 1000, 1000, 4);
    stockSecurity.setTradingCurrency(QLatin1String("BLUB"));
    // add the security
    ft.restart();
    try {
        m->addSecurity(stockSecurity);
        ft.commit();
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // check that we can get it via the security method
    try {
        MyMoneySecurity sec = m->security(stockSecurity.id());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }

    // and also via the currency method
    try {
        MyMoneySecurity sec = m->currency(stockSecurity.id());
    } catch (const MyMoneyException& e) {
        unexpectedException(e);
    }
}

void MyMoneyFileTest::testNextCheckNumber_data()
{
    QTest::addColumn<QString>("lastnumber");
    QTest::addColumn<QString>("nextnumber");

    QTest::newRow("empty") << QString() << QStringLiteral("1");
    QTest::newRow("simple number") << QStringLiteral("123") << QStringLiteral("124");
    QTest::newRow("text in front") << QStringLiteral("No 123") << QStringLiteral("No 124");
    QTest::newRow("text following") << QStringLiteral("123 ABC") << QStringLiteral("124 ABC");
    QTest::newRow("enclosed in text") << QStringLiteral("No 123 ABC") << QStringLiteral("No 124 ABC");
    QTest::newRow("number with hyphen") << QStringLiteral("No 123-001 ABC") << QStringLiteral("No 123-002 ABC");
    QTest::newRow("number with dot") << QStringLiteral("2012.001") << QStringLiteral("2012.002");
}

void MyMoneyFileTest::testNextCheckNumber()
{
    QFETCH(QString, lastnumber);
    QFETCH(QString, nextnumber);

    QCOMPARE(m->nextCheckNumberFromTemplate(lastnumber), nextnumber);
}
