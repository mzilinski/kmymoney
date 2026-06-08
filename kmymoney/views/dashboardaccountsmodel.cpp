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
#include "mymoneymoney.h"

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
        // Reuse the engine's Balance column formatting (adjustedBalance + formatMoney)
        // so a card shows exactly what the ledger/accounts view shows.
        return QSortFilterProxyModel::data(index.siblingAtColumn(AccountsModel::Column::Balance), Qt::DisplayRole);

    case IsNegativeRole: {
        const auto group = static_cast<eMyMoney::Account::Type>(QSortFilterProxyModel::data(index, eMyMoney::Model::AccountGroupRole).toInt());
        auto balance = QSortFilterProxyModel::data(index, eMyMoney::Model::AccountBalanceRole).value<MyMoneyMoney>();
        // Mirror AccountsModelPrivate::adjustedBalance(): liability/income/equity flip sign.
        switch (group) {
        case eMyMoney::Account::Type::Liability:
        case eMyMoney::Account::Type::Income:
        case eMyMoney::Account::Type::Equity:
            balance = -balance;
            break;
        default:
            break;
        }
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
