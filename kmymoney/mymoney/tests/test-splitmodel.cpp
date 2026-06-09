/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test-splitmodel.h"

#include <QTest>

#include <KLocalizedString>

#include "splitmodel.h"

QTEST_GUILESS_MAIN(SplitModelTest)

// These cover the engine-side substance of LH-F-16 (the single-amount, no-Soll/Haben
// presentation) and lock the blockers found in review: columnCount stays MaxColumns (so
// the Q_ASSERT in columnCount() holds and ColumnSelector's stable indices are preserved);
// the header is a pure function of the flag (not a mutated stored string), so a copy or a
// toggle never strands the wrong label; and the copy constructor preserves the flag (the
// inline split editor builds its working model via that copy). The actual signed-amount
// formatting in data() needs a MyMoneyFile/currency fixture and is covered by the manual
// visual QA (see MANUAL-CHECKS.md).

void SplitModelTest::testDefaultIsPaymentDepositColumns()
{
    SplitModel model(this);
    QVERIFY(!model.singleAmountColumn());
    QCOMPARE(model.columnCount(), static_cast<int>(SplitModel::Column::MaxColumns));
    QCOMPARE(model.headerData(SplitModel::Column::Payment, Qt::Horizontal, Qt::DisplayRole).toString(), i18nc("Split header", "Payment"));
    QCOMPARE(model.headerData(SplitModel::Column::Deposit, Qt::Horizontal, Qt::DisplayRole).toString(), i18nc("Split header", "Deposit"));
}

void SplitModelTest::testSingleAmountColumnRelabelsPaymentAndKeepsColumnCount()
{
    SplitModel model(this);
    model.setSingleAmountColumn(true);
    QVERIFY(model.singleAmountColumn());
    // The Deposit column is hidden by the view, not dropped, so the column set is unchanged.
    QCOMPARE(model.columnCount(), static_cast<int>(SplitModel::Column::MaxColumns));
    // The Payment column is now the signed "Amount" column.
    QCOMPARE(model.headerData(SplitModel::Column::Payment, Qt::Horizontal, Qt::DisplayRole).toString(), i18nc("Split header", "Amount"));
}

void SplitModelTest::testSingleAmountColumnRestoresHeaderOnToggleOff()
{
    SplitModel model(this);
    model.setSingleAmountColumn(true);
    model.setSingleAmountColumn(false);
    QVERIFY(!model.singleAmountColumn());
    QCOMPARE(model.headerData(SplitModel::Column::Payment, Qt::Horizontal, Qt::DisplayRole).toString(), i18nc("Split header", "Payment"));
}

void SplitModelTest::testCopyPreservesSingleAmountColumn()
{
    SplitModel original(this);
    original.setSingleAmountColumn(true);

    // copy constructor (used by NewTransactionEditor::editSplits() to build its working model)
    SplitModel copied(this, nullptr, original);
    QVERIFY(copied.singleAmountColumn());
    QCOMPARE(copied.headerData(SplitModel::Column::Payment, Qt::Horizontal, Qt::DisplayRole).toString(), i18nc("Split header", "Amount"));

    // assignment operator
    SplitModel assigned(this);
    assigned = original;
    QVERIFY(assigned.singleAmountColumn());
}
