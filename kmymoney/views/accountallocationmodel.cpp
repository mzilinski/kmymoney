/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "accountallocationmodel.h"

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// Std Includes

#include <algorithm>

// ----------------------------------------------------------------------------
// QT Includes

#include <QDate>

// ----------------------------------------------------------------------------
// Project Includes

#include "accountsmodel.h"
#include "kmymoneysettings.h"
#include "mymoneyaccount.h"
#include "mymoneyexception.h"
#include "mymoneyfile.h"
#include "mymoneymoney.h"
#include "mymoneyprice.h"
#include "mymoneysecurity.h"

AccountAllocationModel::AccountAllocationModel(QObject* parent)
    : QAbstractListModel(parent)
{
    connect(MyMoneyFile::instance(), &MyMoneyFile::dataChanged, this, &AccountAllocationModel::refresh);
    // Per-account balances are filled asynchronously after the file loads and that path
    // only emits netWorthChanged (not MyMoneyFile::dataChanged), so without this the pie
    // would keep the empty/zero state it computed at startup. file->balance() returns the
    // freshly computed totals once this fires.
    connect(MyMoneyFile::instance()->accountsModel(), &AccountsModel::netWorthChanged, this, &AccountAllocationModel::refresh);
    refresh();
}

int AccountAllocationModel::count() const
{
    return m_entries.size();
}

int AccountAllocationModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

void AccountAllocationModel::refresh()
{
    beginResetModel();
    m_entries.clear();

    auto* file = MyMoneyFile::instance();
    // ShowAllAccounts also reveals closed accounts here, matching the dashboard cards.
    // A zero-balance account is always skipped below (a pie cannot draw a zero wedge),
    // which implicitly satisfies the HideZeroBalanceAccountsHome preference too.
    const bool showAllAccounts = KMyMoneySettings::showAllAccounts();

    try {
        const auto base = file->baseCurrency();
        QList<MyMoneyAccount> accounts;
        file->accountList(accounts);
        for (const auto& acc : std::as_const(accounts)) {
            // Real asset/liability accounts only. Stock (investment share) accounts carry
            // a share balance rather than a currency amount, so they are excluded from the
            // magnitude pie (the parent investment account still contributes its cash).
            if (!acc.isAssetLiability() || acc.isInvest())
                continue;
            if (acc.isClosed() && !showAllAccounts)
                continue;

            try {
                MyMoneyMoney value = file->balance(acc.id(), QDate::currentDate());
                if (value.isZero())
                    continue;

                // Convert foreign-currency balances to the base currency (mirrors
                // KHomeViewPrivate::showAccountEntry()); omit an account whose rate is
                // unknown rather than inventing a number.
                if (acc.currencyId() != base.id()) {
                    const auto price = file->price(acc.tradingCurrencyId(), base.id(), QDate::currentDate());
                    if (!price.isValid())
                        continue;
                    value = (value * price.rate(base.id())).convert(base.smallestAccountFraction());
                }

                const double magnitude = qAbs(value.toDouble());
                if (magnitude == 0.0)
                    continue;

                Entry e;
                e.id = acc.id();
                e.name = acc.name();
                e.value = magnitude;
                m_entries.append(e);
            } catch (const MyMoneyException&) {
                // skip just this account; never abort the whole pie
            }
        }
    } catch (const MyMoneyException&) {
        // no file open / no base currency yet -> empty pie
    }

    // Largest wedge first for a tidy pie and legend.
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) {
        return a.value > b.value;
    });

    endResetModel();
    Q_EMIT countChanged();
}

QVariant AccountAllocationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const Entry& e = m_entries.at(index.row());
    switch (role) {
    case AccountNameRole:
        return e.name;
    case AllocationValueRole:
        return e.value;
    case AccountIdRole:
        return e.id;
    default:
        return {};
    }
}

QHash<int, QByteArray> AccountAllocationModel::roleNames() const
{
    return {
        {AccountNameRole, "accountName"},
        {AllocationValueRole, "allocationValue"},
        {AccountIdRole, "accountId"},
    };
}

#endif // ENABLE_QML_HOME
