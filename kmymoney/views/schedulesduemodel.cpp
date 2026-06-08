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

    auto* file = MyMoneyFile::instance();

    // Build one cache entry from a schedule's main split, reproducing the classic home
    // view's showPaymentEntry() (upcoming section, single-row layout). Enums are fully
    // qualified here (no file-level `using namespace eMyMoney`).
    const auto buildEntry = [file](const MyMoneySchedule& sched, int cnt) -> Entry {
        Entry e;
        MyMoneyAccount mainAccount = sched.account();
        if (mainAccount.id().isEmpty())
            return e; // empty id => caller skips this schedule
        MyMoneyTransaction t = sched.transaction();
        if (sched.type() == eMyMoney::Schedule::Type::LoanPayment) {
            // adjust the local copy so it carries the actual next-payment amounts
            KMyMoneyUtils::calculateAutoLoan(sched, t, QMap<QString, MyMoneyMoney>());
        }

        const auto currency = file->currency(mainAccount.currencyId());
        MyMoneyMoney payment;
        const auto splits = t.splits();
        for (const auto& split : splits) {
            if (split.accountId() == mainAccount.id()) {
                payment = split.value(t.commodity(), currency.id()) * cnt;
                break;
            }
        }

        e.id = sched.id();
        e.name = sched.name();
        e.accountName = mainAccount.name();
        e.dueDateText = MyMoneyUtils::formatDate(sched.adjustedNextDueDate());
        e.amountText = MyMoneyUtils::formatMoney(payment, mainAccount, currency);
        e.isOverdue = sched.isOverdue();
        e.isNegativeAmount = payment.isNegative();
        e.overdueCountText = (cnt > 1) ? i18np("(%1 payment)", "(%1 payments)", cnt) : QString();
        return e;
    };

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
    };
}

#endif // ENABLE_QML_HOME
