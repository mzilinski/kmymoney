/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SPLITMODELTEST_H
#define SPLITMODELTEST_H

#include <QObject>

class SplitModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // LH-F-16: SplitModel's pushed-in single-amount (no Soll/Haben) presentation.
    void testDefaultIsPaymentDepositColumns();
    void testSingleAmountColumnRelabelsPaymentAndKeepsColumnCount();
    void testSingleAmountColumnRestoresHeaderOnToggleOff();
    void testCopyPreservesSingleAmountColumn();
};

#endif
