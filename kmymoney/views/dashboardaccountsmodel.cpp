/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dashboardaccountsmodel.h"

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// KDE Includes

#include <KDescendantsProxyModel>

// ----------------------------------------------------------------------------
// Project Includes

#include "accountsmodel.h"
#include "kmymoneysettings.h"
#include "mymoneyenums.h"
#include "mymoneyexception.h"
#include "mymoneyfile.h"
#include "mymoneymoney.h"
#include "mymoneysecurity.h"

namespace {
// Sign convention of AccountsModelPrivate::adjustedBalance(): liability/income/equity
// balances flip sign so a card reads with the same sign the accounts view shows
// (e.g. a credit-card debt as a positive amount owed).
MyMoneyMoney adjustForGroup(MyMoneyMoney balance, eMyMoney::Account::Type group)
{
    switch (group) {
    case eMyMoney::Account::Type::Liability:
    case eMyMoney::Account::Type::Income:
    case eMyMoney::Account::Type::Equity:
        return -balance;
    default:
        return balance;
    }
}
}

DashboardAccountsModel::DashboardAccountsModel(QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_descendants(new KDescendantsProxyModel(this))
    , m_accountsModel(nullptr)
    , m_hideZeroBalance(KMyMoneySettings::hideZeroBalanceAccountsHome())
    , m_showAllAccounts(KMyMoneySettings::showAllAccounts())
{
    // Flatten the entire account tree (all nodes, all depths) into a single list.
    m_descendants->setExpandsByDefault(true);
    setDynamicSortFilter(true);
}

DashboardAccountsModel::~DashboardAccountsModel() = default;

void DashboardAccountsModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    m_accountsModel = qobject_cast<AccountsModel*>(sourceModel);
    m_descendants->setSourceModel(sourceModel);
    QSortFilterProxyModel::setSourceModel(m_descendants);

    // Per-account balances are filled asynchronously after the file loads: the engine's
    // updateAccountBalances() mutates the cached MyMoneyAccount values with per-row
    // dataChanged suppressed and only emits netWorthChanged. Without this hook the cards
    // would stay frozen at the zero balance they read on first paint.
    if (m_accountsModel)
        connect(m_accountsModel, &AccountsModel::netWorthChanged, this, &DashboardAccountsModel::refreshBalances, Qt::UniqueConnection);
}

void DashboardAccountsModel::refreshBalances()
{
    // Re-run the filter first: a hide-zero card may need to appear now that its balance is
    // no longer the startup zero (filterAcceptsRow tests AccountBalanceRole). Then re-emit
    // dataChanged for the balance roles so the visible cards re-read their fresh values.
    invalidateFilter();
    const int rows = rowCount();
    if (rows > 0)
        Q_EMIT dataChanged(index(0, 0), index(rows - 1, 0), {BalanceTextRole, IsNegativeRole});
}

bool DashboardAccountsModel::isDescendantOf(const QModelIndex& index, const QModelIndex& ancestor) const
{
    if (!ancestor.isValid())
        return false;
    for (QModelIndex p = index.parent(); p.isValid(); p = p.parent()) {
        if (p == ancestor)
            return true;
    }
    return false;
}

bool DashboardAccountsModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!m_accountsModel)
        return false;

    // sourceModel() is the flattening proxy; map the flat row back to the engine index.
    const QModelIndex flatIdx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QModelIndex idx = m_descendants->mapToSource(flatIdx);
    if (!idx.isValid())
        return false;

    // Hide closed accounts unless the user asked to show all accounts (classic rule:
    // !isClosed() || showAllAccounts).
    if (idx.data(eMyMoney::Model::AccountIsClosedRole).toBool() && !m_showAllAccounts)
        return false;

    // Hide zero-balance accounts when the home preference asks for it. Mirrors the
    // classic showAccounts() conjunction (hideZeroBalanceAccountsHome && !showAllAccounts);
    // showAllAccounts therefore wins over hide-zero just as it does in the classic view.
    if (m_hideZeroBalance && !m_showAllAccounts && idx.data(eMyMoney::Model::AccountBalanceRole).value<MyMoneyMoney>().isZero())
        return false;

    // Keep only the real accounts below the asset or liability group nodes. Being a
    // strict descendant of those two subtrees automatically excludes the six group
    // pseudo-nodes, the income/expense/equity accounts and the duplicated Favorite
    // subtree (whose ancestor is the favorite node, not asset/liability).
    return isDescendantOf(idx, m_accountsModel->assetIndex()) || isDescendantOf(idx, m_accountsModel->liabilityIndex());
}

void DashboardAccountsModel::updateSettings()
{
    m_hideZeroBalance = KMyMoneySettings::hideZeroBalanceAccountsHome();
    m_showAllAccounts = KMyMoneySettings::showAllAccounts();
    // The accepted set depends on the cached settings and on per-account balances (a
    // balance crossing zero must add/drop a card), so always re-run the filter; the
    // home view calls this on every refresh (show / dataChanged / settings change).
    invalidateFilter();
}

QVariant DashboardAccountsModel::data(const QModelIndex& index, int role) const
{
    switch (role) {
    case BalanceTextRole:
        // Format exactly as the accounts view Balance column does, but read the balance
        // via column-0 roles instead of siblingAtColumn(Balance): the intervening
        // KDescendantsProxyModel flattens the tree to a single column, so a sibling-column
        // lookup resolves to an invalid index and yields an empty string. The balance is
        // the engine model's cached MyMoneyAccount::balance(), which is filled
        // asynchronously after file load — see the netWorthChanged refresh below.
        try {
            const auto group = static_cast<eMyMoney::Account::Type>(QSortFilterProxyModel::data(index, eMyMoney::Model::AccountGroupRole).toInt());
            const auto balance = adjustForGroup(QSortFilterProxyModel::data(index, eMyMoney::Model::AccountBalanceRole).value<MyMoneyMoney>(), group);
            const auto security = MyMoneyFile::instance()->security(QSortFilterProxyModel::data(index, eMyMoney::Model::AccountCurrencyIdRole).toString());
            const auto fraction = QSortFilterProxyModel::data(index, eMyMoney::Model::AccountFractionRole).toInt();
            return balance.formatMoney(security.tradingSymbol(), MyMoneyMoney::denomToPrec(fraction));
        } catch (const MyMoneyException&) {
            return QString();
        }

    case IsNegativeRole: {
        const auto group = static_cast<eMyMoney::Account::Type>(QSortFilterProxyModel::data(index, eMyMoney::Model::AccountGroupRole).toInt());
        const auto balance = adjustForGroup(QSortFilterProxyModel::data(index, eMyMoney::Model::AccountBalanceRole).value<MyMoneyMoney>(), group);
        return balance.isNegative();
    }

    default:
        return QSortFilterProxyModel::data(index, role);
    }
}

QHash<int, QByteArray> DashboardAccountsModel::roleNames() const
{
    auto names = QSortFilterProxyModel::roleNames();
    names.insert(BalanceTextRole, "balanceText");
    names.insert(IsNegativeRole, "isNegative");
    return names;
}

#endif // ENABLE_QML_HOME
