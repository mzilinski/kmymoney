/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ACCOUNTALLOCATIONMODEL_H
#define ACCOUNTALLOCATIONMODEL_H

#include <config-kmymoney.h>

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// QT Includes

#include <QAbstractListModel>
#include <QVector>

/**
 * View-layer list model of the user's real accounts and their absolute base-currency
 * magnitude, feeding the per-account allocation pie of the experimental Kirigami/QML
 * Home view (the MoneyMoney-style "where is my money" donut). It selects the same
 * asset/liability accounts the dashboard cards show (honoring ShowAllAccounts for closed
 * accounts) and converts every balance to the base currency. Zero-magnitude accounts are
 * always dropped, since a pie cannot render a zero wedge (this also satisfies the
 * hide-zero-balance preference implicitly). Stock (investment share) accounts are excluded
 * because their balance is in shares, not a currency.
 *
 * All presentation values are precomputed here from MyMoneyFile; the engine stays
 * GUI-free. The model refreshes itself on MyMoneyFile::dataChanged.
 */
class AccountAllocationModel : public QAbstractListModel
{
    Q_OBJECT

    /// Row count exposed as a reactive property (QAbstractItemModel::rowCount is an
    /// invokable method, not a property, so QML cannot bind to it for visibility).
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        AccountNameRole = Qt::UserRole + 1000,
        AllocationValueRole, ///< absolute base-currency magnitude (double) for the pie wedge
        AccountIdRole,
    };

    explicit AccountAllocationModel(QObject* parent = nullptr);

    int count() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public Q_SLOTS:
    /// Re-reads the accounts and their base-currency magnitudes and resets the model.
    void refresh();

Q_SIGNALS:
    void countChanged();

private:
    struct Entry {
        QString id;
        QString name;
        double value = 0.0;
    };

    QVector<Entry> m_entries;
};

#endif // ENABLE_QML_HOME
#endif // ACCOUNTALLOCATIONMODEL_H
