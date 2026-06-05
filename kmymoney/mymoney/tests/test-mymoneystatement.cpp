/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test-mymoneystatement.h"

#include <QDomDocument>
#include <QDomElement>
#include <QTest>

#include "mymoneymoney.h"
#include "mymoneystatement.h"

QTEST_GUILESS_MAIN(MyMoneyStatementTest)

void MyMoneyStatementTest::testTransactionIbanBicRoundTrip()
{
    MyMoneyStatement st;
    MyMoneyStatement::Transaction tr;
    tr.m_datePosted = QDate(2026, 1, 2);
    tr.m_strPayee = QStringLiteral("ACME GmbH");
    tr.m_amount = MyMoneyMoney(-1234, 100);
    tr.m_strIBAN = QStringLiteral("DE89370400440532013000");
    tr.m_strBIC = QStringLiteral("COBADEFFXXX");
    st.m_listTransactions.append(tr);

    QDomDocument doc;
    QDomElement root = doc.createElement(QStringLiteral("ROOT"));
    doc.appendChild(root);
    st.write(root, &doc);

    const QDomElement stmtEl = root.firstChildElement(QStringLiteral("STATEMENT"));
    QVERIFY(!stmtEl.isNull());

    MyMoneyStatement readBack;
    QVERIFY(readBack.read(stmtEl));
    QCOMPARE(readBack.m_listTransactions.count(), 1);
    QCOMPARE(readBack.m_listTransactions.at(0).m_strIBAN, QStringLiteral("DE89370400440532013000"));
    QCOMPARE(readBack.m_listTransactions.at(0).m_strBIC, QStringLiteral("COBADEFFXXX"));
}

void MyMoneyStatementTest::testTransactionMissingIbanBicReadsEmpty()
{
    // An old statement element whose transaction carries no iban/bic attributes
    // must read back with empty counterparty fields (forward/backward compatible).
    QDomDocument doc;
    QDomElement stmt = doc.createElement(QStringLiteral("STATEMENT"));
    QDomElement tx = doc.createElement(QStringLiteral("TRANSACTION"));
    tx.setAttribute(QStringLiteral("dateposted"), QStringLiteral("2026-01-02"));
    tx.setAttribute(QStringLiteral("payee"), QStringLiteral("ACME GmbH"));
    tx.setAttribute(QStringLiteral("amount"), QStringLiteral("-1234/100"));
    stmt.appendChild(tx);
    doc.appendChild(stmt);

    MyMoneyStatement st;
    QVERIFY(st.read(stmt));
    QCOMPARE(st.m_listTransactions.count(), 1);
    QVERIFY(st.m_listTransactions.at(0).m_strIBAN.isEmpty());
    QVERIFY(st.m_listTransactions.at(0).m_strBIC.isEmpty());
}
