/*
    KMyMoney transaction importing module - tests for payee IBAN/BIC learning

    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PAYEEIBANLEARNERTEST_H
#define PAYEEIBANLEARNERTEST_H

#include <QObject>

#include "mymoneytestutils.h"

class PayeeIbanLearnerTest : public QObject, public MyMoneyTestBase
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void learnsNewIdentifier();
    void dedupsSameIbanAndKeepsOriginal();
    void learnsSecondDifferentIban();
    void doesNotOverwriteUserSeededIdentifier();
    void skipsInvalidIban();
    void skipsEmptyIban();
    void learnsIbanWithEmptyBic();
};

#endif // PAYEEIBANLEARNERTEST_H
