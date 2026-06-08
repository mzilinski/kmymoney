/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulesduemodel.h"

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// Std Includes

#include <algorithm>

// ----------------------------------------------------------------------------
// QT Includes

#include <QDate>
#include <QSet>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KLocalizedString>

// ----------------------------------------------------------------------------
// Project Includes

#include "kmymoneyutils.h"
#include "mymoneyaccount.h"
#include "mymoneyenums.h"
#include "mymoneyexception.h"
#include "mymoneyfile.h"
#include "mymoneymoney.h"
#include "mymoneyschedule.h"
#include "mymoneysecurity.h"
#include "mymoneysplit.h"
#include "mymoneytransaction.h"
#include "mymoneyutils.h"

SchedulesDueModel::SchedulesDueModel(QObject* parent)
    : QAbstractListModel(parent)
{
    connect(MyMoneyFile::instance(), &MyMoneyFile::dataChanged, this, &SchedulesDueModel::refresh);
    refresh();
}

int SchedulesDueModel::count() const
{
    return m_entries.size();
}

int SchedulesDueModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

void SchedulesDueModel::refresh()
{
    beginResetModel();
    m_entries.clear();
    // Invalidate the forecast and its per-(account,date) accumulator; both are recomputed
    // lazily the first time a balance-after is needed during this refresh (see ensureForecast).
    m_forecastDone = false;
    m_balanceCache.clear();

    auto* file = MyMoneyFile::instance();

    // Same two queries as showScheduledPayments(): all overdue schedules, then the ones
    // due within one month (the preview window is hardcoded there too). scheduleList()
    // throws when no file is open; then the section simply stays empty.
    QList<MyMoneySchedule> overdues;
    QList<MyMoneySchedule> future;
    try {
        overdues = file->scheduleList(QString(),
                                      eMyMoney::Schedule::Type::Any,
                                      eMyMoney::Schedule::Occurrence::Any,
                                      eMyMoney::Schedule::PaymentType::Any,
                                      QDate(),
                                      QDate(),
                                      true);
        future = file->scheduleList(QString(),
                                    eMyMoney::Schedule::Type::Any,
                                    eMyMoney::Schedule::Occurrence::Any,
                                    eMyMoney::Schedule::PaymentType::Any,
                                    QDate::currentDate(),
                                    QDate::currentDate().addMonths(1),
                                    false);
    } catch (const MyMoneyException&) {
        endResetModel();
        Q_EMIT countChanged();
        return;
    }

    const auto dropFinished = [](QList<MyMoneySchedule>& list) {
        list.erase(std::remove_if(list.begin(),
                                  list.end(),
                                  [](const MyMoneySchedule& s) {
                                      return s.isFinished();
                                  }),
                   list.end());
    };
    dropFinished(overdues);
    dropFinished(future);

    // MyMoneySchedule::operator< sorts by (adjusted) next due date, matching classic.
    std::sort(overdues.begin(), overdues.end());
    std::sort(future.begin(), future.end());

    // Build each schedule in its own try/catch so one corrupt/dangling schedule is
    // skipped individually (matching showPaymentEntry's per-entry guard) instead of
    // emptying the whole list.
    QSet<QString> overdueIds;
    for (const auto& sched : std::as_const(overdues)) {
        try {
            const int cnt = sched.transactionsRemainingUntil(QDate::currentDate().addDays(-1));
            const Entry e = buildEntry(sched, cnt);
            if (e.id.isEmpty())
                continue;
            overdueIds.insert(sched.id());
            m_entries.append(e);
        } catch (const MyMoneyException&) {
            // skip just this schedule
        }
    }
    // Deliberate simplification vs the classic future loop: with one row per schedule, a
    // schedule already shown as overdue is skipped here entirely (the classic HTML view
    // can additionally list a later recurrence of it).
    for (const auto& sched : std::as_const(future)) {
        if (overdueIds.contains(sched.id()))
            continue;
        try {
            const Entry e = buildEntry(sched, 1);
            if (e.id.isEmpty())
                continue;
            m_entries.append(e);
        } catch (const MyMoneyException&) {
            // skip just this schedule
        }
    }

    endResetModel();
    Q_EMIT countChanged();
}

SchedulesDueModel::Entry SchedulesDueModel::buildEntry(const MyMoneySchedule& sched, int cnt)
{
    auto* file = MyMoneyFile::instance();
    Entry e;

    MyMoneyAccount mainAccount = sched.account();
    if (mainAccount.id().isEmpty())
        return e; // empty id => caller skips this schedule

    MyMoneyTransaction t = sched.transaction();
    if (sched.type() == eMyMoney::Schedule::Type::LoanPayment) {
        // adjust the local copy so it carries the actual next-payment amounts
        KMyMoneyUtils::calculateAutoLoan(sched, t, QMap<QString, MyMoneyMoney>());
    }

    // Resolve each split's account once (mirrors showPaymentEntry()). A throwing account()
    // lookup propagates to refresh()'s per-schedule try/catch, skipping just this schedule.
    const auto splits = t.splits();
    QVector<MyMoneyAccount> accounts;
    accounts.reserve(splits.size());
    for (const auto& sp : splits)
        accounts.append(file->account(sp.accountId()));

    // A transfer is exactly two asset/liability splits; then both sides are shown.
    const bool isTransfer = (splits.count() == 2) && (accounts.count() == 2) && accounts.at(0).isAssetLiability() && accounts.at(1).isAssetLiability();

    // Locate the main split (the one on sched.account()).
    int mainIdx = -1;
    for (int i = 0; i < splits.size(); ++i) {
        if (splits.at(i).accountId() == mainAccount.id()) {
            mainIdx = i;
            break;
        }
    }
    if (mainIdx < 0)
        return e; // no split on the main account => skip

    const QDate dueDate = sched.adjustedNextDueDate();

    // Amount (scaled by the overdue count) and formatting for one split index, each in its
    // own account's currency (transfers between differing currencies format independently).
    const auto amountOf = [&](int i) -> MyMoneyMoney {
        const auto currency = file->currency(accounts.at(i).currencyId());
        return splits.at(i).value(t.commodity(), currency.id()) * cnt;
    };
    const auto formatFor = [&](int i, const MyMoneyMoney& amount) -> QString {
        return MyMoneyUtils::formatMoney(amount, accounts.at(i), file->currency(accounts.at(i).currencyId()));
    };

    const MyMoneyMoney mainAmount = amountOf(mainIdx);
    const MyMoneyMoney mainBalanceAfter = forecastPaymentBalance(accounts.at(mainIdx), mainAmount, dueDate);

    e.id = sched.id();
    e.name = sched.name();
    e.accountName = accounts.at(mainIdx).name();
    e.dueDateText = MyMoneyUtils::formatDate(dueDate);
    e.amountText = formatFor(mainIdx, mainAmount);
    e.isOverdue = sched.isOverdue();
    e.isNegativeAmount = mainAmount.isNegative();
    e.overdueCountText = (cnt > 1) ? i18np("(%1 payment)", "(%1 payments)", cnt) : QString();
    e.balanceAfterText = formatFor(mainIdx, mainBalanceAfter);
    e.isNegativeBalanceAfter = mainBalanceAfter.isNegative();

    if (isTransfer) {
        const int counterIdx = mainIdx ^ 1; // 0<->1, valid because there are exactly two splits
        const MyMoneyMoney counterAmount = amountOf(counterIdx);
        const MyMoneyMoney counterBalanceAfter = forecastPaymentBalance(accounts.at(counterIdx), counterAmount, dueDate);
        e.isTransfer = true;
        e.counterAccountName = accounts.at(counterIdx).name();
        e.counterAmountText = formatFor(counterIdx, counterAmount);
        e.counterIsNegativeAmount = counterAmount.isNegative();
        e.counterBalanceAfterText = formatFor(counterIdx, counterBalanceAfter);
        e.counterIsNegativeBalanceAfter = counterBalanceAfter.isNegative();
    }
    return e;
}

void SchedulesDueModel::ensureForecast()
{
    if (m_forecastDone)
        return;
    // Same setup as KHomeViewPrivate::doForecast(): build from the user's forecast config
    // and make sure the forecast window spans at least one accounts cycle.
    m_forecast = MyMoneyForecast::fromConfig(KMyMoneyUtils::forecastConfig());
    if (m_forecast.accountsCycle() > m_forecast.forecastDays())
        m_forecast.setForecastDays(m_forecast.accountsCycle());
    m_forecast.doForecast();
    m_forecastDone = true;
}

MyMoneyMoney SchedulesDueModel::forecastPaymentBalance(const MyMoneyAccount& acc, const MyMoneyMoney& payment, QDate paymentDate)
{
    ensureForecast();

    // Mirrors KHomeViewPrivate::forecastPaymentBalance(): accumulate successive payments
    // per (account, date) so several due payments to the same account stack correctly.
    if (paymentDate <= QDate::currentDate())
        paymentDate = QDate::currentDate().addDays(1);

    auto& dateMap = m_balanceCache[acc.id()];
    if (!dateMap.contains(paymentDate)) {
        if (paymentDate == QDate::currentDate())
            dateMap[paymentDate] = m_forecast.forecastBalance(acc, paymentDate);
        else
            dateMap[paymentDate] = m_forecast.forecastBalance(acc, paymentDate.addDays(-1));
    }
    dateMap[paymentDate] += payment;
    return dateMap[paymentDate];
}

QVariant SchedulesDueModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const Entry& e = m_entries.at(index.row());
    switch (role) {
    case ScheduleIdRole:
        return e.id;
    case ScheduleNameRole:
        return e.name;
    case DueDateTextRole:
        return e.dueDateText;
    case AmountTextRole:
        return e.amountText;
    case AccountNameRole:
        return e.accountName;
    case IsOverdueRole:
        return e.isOverdue;
    case IsNegativeAmountRole:
        return e.isNegativeAmount;
    case OverdueCountTextRole:
        return e.overdueCountText;
    case BalanceAfterTextRole:
        return e.balanceAfterText;
    case IsNegativeBalanceAfterRole:
        return e.isNegativeBalanceAfter;
    case IsTransferRole:
        return e.isTransfer;
    case CounterAccountNameRole:
        return e.counterAccountName;
    case CounterAmountTextRole:
        return e.counterAmountText;
    case CounterIsNegativeAmountRole:
        return e.counterIsNegativeAmount;
    case CounterBalanceAfterTextRole:
        return e.counterBalanceAfterText;
    case CounterIsNegativeBalanceAfterRole:
        return e.counterIsNegativeBalanceAfter;
    default:
        return {};
    }
}

QHash<int, QByteArray> SchedulesDueModel::roleNames() const
{
    return {
        {ScheduleIdRole, "scheduleId"},
        {ScheduleNameRole, "scheduleName"},
        {DueDateTextRole, "dueDateText"},
        {AmountTextRole, "amountText"},
        {AccountNameRole, "accountName"},
        {IsOverdueRole, "isOverdue"},
        {IsNegativeAmountRole, "isNegativeAmount"},
        {OverdueCountTextRole, "overdueCountText"},
        {BalanceAfterTextRole, "balanceAfterText"},
        {IsNegativeBalanceAfterRole, "isNegativeBalanceAfter"},
        {IsTransferRole, "isTransfer"},
        {CounterAccountNameRole, "counterAccountName"},
        {CounterAmountTextRole, "counterAmountText"},
        {CounterIsNegativeAmountRole, "counterIsNegativeAmount"},
        {CounterBalanceAfterTextRole, "counterBalanceAfterText"},
        {CounterIsNegativeBalanceAfterRole, "counterIsNegativeBalanceAfter"},
    };
}

#endif // ENABLE_QML_HOME
