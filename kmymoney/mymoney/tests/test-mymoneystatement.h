/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MYMONEYSTATEMENTTEST_H
#define MYMONEYSTATEMENTTEST_H

#include <QObject>

#include "mymoneytestutils.h"

class MyMoneyStatementTest : public QObject, public MyMoneyTestBase
{
    Q_OBJECT

private Q_SLOTS:
    void testTransactionIbanBicRoundTrip();
    void testTransactionMissingIbanBicReadsEmpty();
};

#endif // MYMONEYSTATEMENTTEST_H
