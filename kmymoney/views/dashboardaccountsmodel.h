/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DASHBOARDACCOUNTSMODEL_H
#define DASHBOARDACCOUNTSMODEL_H

#include <config-kmymoney.h>

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// QT Includes

#include <QSortFilterProxyModel>

// ----------------------------------------------------------------------------
// Project Includes

class AccountsModel;
class KDescendantsProxyModel;

/**
 * View-layer proxy that turns the engine's account tree into a flat, deduplicated
 * list of the real asset/liability accounts for the experimental Kirigami/QML Home
 * dashboard.
 *
 * It flattens the tree (via an internal @c KDescendantsProxyModel) and keeps only
 * the non-closed accounts that are strict descendants of the asset or liability
 * group nodes. That single ancestry test conveniently drops the six group
 * pseudo-nodes, the income/expense/equity accounts, and the duplicated Favorite
 * subtree (whose rows descend from the favorite node, not from asset/liability).
 *
 * On top of the engine role names it exposes two presentation roles:
 *   - @c balanceText : the balance formatted exactly as the ledger shows it
 *   - @c isNegative  : whether the displayed (group-adjusted) balance is negative
 *
 * All presentation logic lives here, never in the engine model.
 */
class DashboardAccountsModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum Roles {
        BalanceTextRole = Qt::UserRole + 1000,
        IsNegativeRole,
    };

    explicit DashboardAccountsModel(QObject* parent = nullptr);
    ~DashboardAccountsModel() override;

    /// Accepts the engine @c AccountsModel; the flattening proxy is inserted internally.
    void setSourceModel(QAbstractItemModel* sourceModel) override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Re-reads the home-view account preferences (hide-zero-balance / show-all-accounts)
    /// from KMyMoneySettings and re-runs the filter. Called from the home view's refresh
    /// path so toggling the settings (or balances crossing zero) updates the cards.
    void updateSettings();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private Q_SLOTS:
    /// Re-runs the filter and re-emits dataChanged for the balance roles when the engine
    /// finishes computing per-account balances (AccountsModel::netWorthChanged), so the
    /// cards pick up the balances filled in asynchronously after the file loads.
    void refreshBalances();

private:
    bool isDescendantOf(const QModelIndex& index, const QModelIndex& ancestor) const;

    KDescendantsProxyModel* m_descendants;
    AccountsModel* m_accountsModel;
    // Cached home-view preferences (read via updateSettings()), mirroring the classic
    // showAccounts() rule in khomeview_p.h: a zero-balance account is hidden only when
    // hideZeroBalanceAccountsHome AND not showAllAccounts; closed accounts appear only
    // when showAllAccounts is set.
    bool m_hideZeroBalance;
    bool m_showAllAccounts;
};

#endif // ENABLE_QML_HOME
#endif // DASHBOARDACCOUNTSMODEL_H
