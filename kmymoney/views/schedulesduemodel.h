/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SCHEDULESDUEMODEL_H
#define SCHEDULESDUEMODEL_H

#include <config-kmymoney.h>

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// QT Includes

#include <QAbstractListModel>
#include <QDate>
#include <QMap>
#include <QVector>

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneyforecast.h"
#include "mymoneymoney.h"

class MyMoneyAccount;
class MyMoneySchedule;

/**
 * View-layer list model of the scheduled payments that are due soon, for the
 * experimental Kirigami/QML Home view. It reproduces the selection of the classic
 * home view's showScheduledPayments(): the overdue schedules plus the ones due
 * within one month, finished schedules excluded, overdue first then by due date.
 *
 * For each schedule it precomputes the same presentation the classic showPaymentEntry()
 * produces: the main account/amount, the projected balance-after (via MyMoneyForecast,
 * computed once per refresh), and -- for transfers between two asset/liability accounts --
 * the counter account/amount/balance-after on a second line. All formatting lives here;
 * the engine stays GUI-free. The model refreshes itself on MyMoneyFile::dataChanged().
 */
class SchedulesDueModel : public QAbstractListModel
{
    Q_OBJECT

    /// Row count exposed as a reactive property (QAbstractItemModel::rowCount is an
    /// invokable method, not a property, so QML cannot bind to it for visibility).
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        ScheduleIdRole = Qt::UserRole + 1000,
        ScheduleNameRole,
        DueDateTextRole,
        AmountTextRole,
        AccountNameRole,
        IsOverdueRole,
        IsNegativeAmountRole,
        OverdueCountTextRole,
        BalanceAfterTextRole,
        IsNegativeBalanceAfterRole,
        IsTransferRole,
        CounterAccountNameRole,
        CounterAmountTextRole,
        CounterIsNegativeAmountRole,
        CounterBalanceAfterTextRole,
        CounterIsNegativeBalanceAfterRole,
    };

    explicit SchedulesDueModel(QObject* parent = nullptr);

    int count() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public Q_SLOTS:
    /// Re-reads the due schedules from the engine and resets the model.
    void refresh();

Q_SIGNALS:
    void countChanged();

private:
    struct Entry {
        QString id;
        QString name;
        QString dueDateText;
        QString amountText;
        QString accountName;
        QString overdueCountText;
        QString balanceAfterText;
        QString counterAccountName;
        QString counterAmountText;
        QString counterBalanceAfterText;
        bool isOverdue = false;
        bool isNegativeAmount = false;
        bool isNegativeBalanceAfter = false;
        bool isTransfer = false;
        bool counterIsNegativeAmount = false;
        bool counterIsNegativeBalanceAfter = false;
    };

    /// Builds one row from a schedule (main split + optional transfer counter split +
    /// balance-after). Returns an Entry with an empty id when the schedule should be
    /// skipped (no usable main account).
    Entry buildEntry(const MyMoneySchedule& sched, int cnt);

    /// Lazily runs the forecast once per refresh() (it scans history, so it is computed
    /// only when a balance-after is actually needed), mirroring the classic doForecast().
    void ensureForecast();

    /// Projected balance of @p acc right after @p payment on @p paymentDate, accumulating
    /// successive payments per (account, date). Mirrors KHomeViewPrivate::forecastPaymentBalance().
    MyMoneyMoney forecastPaymentBalance(const MyMoneyAccount& acc, const MyMoneyMoney& payment, QDate paymentDate);

    QVector<Entry> m_entries;
    MyMoneyForecast m_forecast;
    bool m_forecastDone = false;
    QMap<QString, QMap<QDate, MyMoneyMoney>> m_balanceCache;
};

#endif // ENABLE_QML_HOME
#endif // SCHEDULESDUEMODEL_H
