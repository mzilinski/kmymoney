/*
    KMyMoney transaction importing module - tests for payee IBAN/BIC learning

    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test-payeeibanlearner.h"

#include <QTest>

#include "mymoneypayee.h"
#include "payeeibanlearner.h"
#include "payeeidentifier/ibanbic/ibanbic.h"
#include "payeeidentifier/payeeidentifiertyped.h"

QTEST_GUILESS_MAIN(PayeeIbanLearnerTest)

namespace {
// well-known, checksum-valid test IBANs
const QString ibanA = QStringLiteral("DE89370400440532013000");
const QString ibanB = QStringLiteral("DE12500105170648489890");

QList<payeeIdentifierTyped<payeeIdentifiers::ibanBic>> ibanIdentifiers(const MyMoneyPayee& payee)
{
    QList<payeeIdentifierTyped<payeeIdentifiers::ibanBic>> result;
    const auto idents = payee.payeeIdentifiers();
    for (const auto& ident : idents) {
        if (ident.iid() != payeeIdentifiers::ibanBic::staticPayeeIdentifierIid())
            continue;
        try {
            result.append(payeeIdentifierTyped<payeeIdentifiers::ibanBic>(ident));
        } catch (const payeeIdentifier::badCast&) {
        } catch (const payeeIdentifier::empty&) {
        }
    }
    return result;
}
}

void PayeeIbanLearnerTest::initTestCase()
{
    // the IBANs the tests rely on must actually be valid
    QVERIFY(payeeIdentifiers::ibanBic::isIbanValid(ibanA));
    QVERIFY(payeeIdentifiers::ibanBic::isIbanValid(ibanB));
}

void PayeeIbanLearnerTest::learnsNewIdentifier()
{
    MyMoneyPayee payee;
    QVERIFY(learnPayeeIban(payee, ibanA, QStringLiteral("COBADEFF"), QStringLiteral("ACME")));

    const auto idents = ibanIdentifiers(payee);
    QCOMPARE(idents.count(), 1);
    QCOMPARE(idents.at(0)->electronicIban(), payeeIdentifiers::ibanBic::ibanToElectronic(ibanA));
    QCOMPARE(idents.at(0)->storedBic(), QStringLiteral("COBADEFF"));
}

void PayeeIbanLearnerTest::dedupsSameIbanAndKeepsOriginal()
{
    MyMoneyPayee payee;
    QVERIFY(learnPayeeIban(payee, ibanA, QStringLiteral("COBADEFF"), QStringLiteral("First")));
    // same account, different BIC/owner in a later statement -> must not duplicate
    QVERIFY(!learnPayeeIban(payee, ibanA, QStringLiteral("MARKDEF1"), QStringLiteral("Second")));

    const auto idents = ibanIdentifiers(payee);
    QCOMPARE(idents.count(), 1);
    // the original identifier is kept untouched
    QCOMPARE(idents.at(0)->storedBic(), QStringLiteral("COBADEFF"));
}

void PayeeIbanLearnerTest::learnsSecondDifferentIban()
{
    MyMoneyPayee payee;
    QVERIFY(learnPayeeIban(payee, ibanA, QStringLiteral("COBADEFF"), QStringLiteral("ACME")));
    QVERIFY(learnPayeeIban(payee, ibanB, QStringLiteral("INGDDEFF"), QStringLiteral("ACME")));

    QCOMPARE(ibanIdentifiers(payee).count(), 2);
}

void PayeeIbanLearnerTest::doesNotOverwriteUserSeededIdentifier()
{
    MyMoneyPayee payee;
    // user curated this identifier by hand
    payeeIdentifierTyped<payeeIdentifiers::ibanBic> seeded(new payeeIdentifiers::ibanBic);
    seeded->setIban(ibanA);
    seeded->setBic(QStringLiteral("MARKDEF1"));
    payee.addPayeeIdentifier(seeded);

    // an import of the same account with a different BIC must not touch it
    QVERIFY(!learnPayeeIban(payee, ibanA, QStringLiteral("COBADEFF"), QStringLiteral("ACME")));

    const auto idents = ibanIdentifiers(payee);
    QCOMPARE(idents.count(), 1);
    QCOMPARE(idents.at(0)->storedBic(), QStringLiteral("MARKDEF1"));
}

void PayeeIbanLearnerTest::skipsInvalidIban()
{
    MyMoneyPayee payee;
    // valid structure but wrong checksum (last digit flipped)
    QVERIFY(!learnPayeeIban(payee, QStringLiteral("DE89370400440532013001"), QStringLiteral("COBADEFF"), QStringLiteral("ACME")));
    QCOMPARE(ibanIdentifiers(payee).count(), 0);
}

void PayeeIbanLearnerTest::skipsEmptyIban()
{
    MyMoneyPayee payee;
    QVERIFY(!learnPayeeIban(payee, QString(), QStringLiteral("COBADEFF"), QStringLiteral("ACME")));
    QCOMPARE(ibanIdentifiers(payee).count(), 0);
}

void PayeeIbanLearnerTest::learnsIbanWithEmptyBic()
{
    MyMoneyPayee payee;
    QVERIFY(learnPayeeIban(payee, ibanA, QString(), QStringLiteral("ACME")));

    const auto idents = ibanIdentifiers(payee);
    QCOMPARE(idents.count(), 1);
    QVERIFY(idents.at(0)->storedBic().isEmpty());
}
