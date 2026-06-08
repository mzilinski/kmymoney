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
#include <QVector>

/**
 * View-layer list model of the scheduled payments that are due soon, for the
 * experimental Kirigami/QML Home view. It reproduces the selection of the classic
 * home view's showScheduledPayments(): the overdue schedules plus the ones due
 * within one month, finished schedules excluded, overdue first then by due date.
 *
 * All presentation strings (formatted amount/date) are precomputed here from
 * MyMoneyFile::scheduleList(); the engine stays GUI-free. The model refreshes
 * itself on MyMoneyFile::dataChanged().
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
        bool isOverdue = false;
        bool isNegativeAmount = false;
    };

    QVector<Entry> m_entries;
};

#endif // ENABLE_QML_HOME
#endif // SCHEDULESDUEMODEL_H
