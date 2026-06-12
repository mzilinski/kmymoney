/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ledgersearchfilter.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QLineEdit>
#include <QTimer>

#include <utility>

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// Project Includes

#include "journalmodel.h"
#include "mymoneyaccount.h"
#include "mymoneyenums.h"
#include "mymoneyexception.h"
#include "mymoneyfile.h"

class LedgerSearchFilterPrivate
{
public:
    QString filterText;
    QString pendingText;
    bool suspended = false;
    QTimer debounceTimer;
    QList<QMetaObject::Connection> sourceConnections;

    // One-entry per-transaction match cache. The rows of a transaction are
    // adjacent in the journal model and mostly evaluated in source order, so
    // one entry amortizes to one evaluation per transaction and pass segment;
    // out-of-order visits only cost a re-evaluation, never staleness.
    QString cachedTxId;
    int cachedRepresentativeRow = -1;
    bool cachedVerdict = false;

    void invalidateCache()
    {
        cachedTxId.clear();
        cachedRepresentativeRow = -1;
        cachedVerdict = false;
    }
};

LedgerSearchFilter::LedgerSearchFilter(QObject* parent)
    : LedgerFilter(parent)
    , d(new LedgerSearchFilterPrivate)
{
    setObjectName(QLatin1String("LedgerSearchFilter"));
    // The journal model inserts empty rows and fills them afterwards,
    // announced only by dataChanged() (JournalModel::doAddItem). The
    // base class turns dynamic filtering off and relies on explicit
    // invalidation, so added transactions would never show up here.
    // Dynamic filtering re-evaluates exactly the changed rows; sorting
    // stays untouched (this proxy never activates a sort column).
    setDynamicSortFilter(true);
    d->debounceTimer.setSingleShot(true);
    connect(&d->debounceTimer, &QTimer::timeout, this, [&]() {
        setFilterFixedString(d->pendingText);
    });
}

LedgerSearchFilter::~LedgerSearchFilter() = default;

void LedgerSearchFilter::attachLineEdit(QLineEdit* lineEdit)
{
    Q_ASSERT(lineEdit != nullptr);
    lineEdit->setClearButtonEnabled(true);
    connect(lineEdit, &QLineEdit::textChanged, this, [&](const QString& text) {
        d->pendingText = text;
        d->debounceTimer.start(200);
    });
}

void LedgerSearchFilter::flushPendingFilter()
{
    if (d->debounceTimer.isActive()) {
        d->debounceTimer.stop();
        setFilterFixedString(d->pendingText);
    }
}

void LedgerSearchFilter::setFilterFixedString(const QString& pattern)
{
    d->invalidateCache();
    d->filterText = pattern;
    // keeps ActiveFilterTextRole truthful and runs the filter synchronously
    LedgerFilter::setFilterFixedString(pattern);
    Q_EMIT filterApplied();
}

void LedgerSearchFilter::setSuspended(bool suspended)
{
    if (d->suspended == suspended)
        return;
    d->suspended = suspended;
    d->invalidateCache();
    invalidateFilter();
    if (!suspended) {
        Q_EMIT filterApplied();
    }
}

void LedgerSearchFilter::setSourceModel(QAbstractItemModel* sourceModel)
{
    // the row arithmetic in filterAcceptsRow() relies on the journal model
    Q_ASSERT(sourceModel == nullptr || sourceModel == MyMoneyFile::instance()->journalModel());

    for (const auto& connection : std::as_const(d->sourceConnections)) {
        disconnect(connection);
    }
    d->sourceConnections.clear();
    d->invalidateCache();

    if (sourceModel) {
        // The cache refers to source rows and verdicts, so any source change
        // invalidates it. These connections must be made before the base class
        // call: the proxy installs its own handlers there, which re-evaluate
        // the affected rows and must not see a stale cache. Slots of the same
        // signal run in connection order.
        const auto clearCache = [&]() {
            d->invalidateCache();
        };
        d->sourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this, clearCache));
        d->sourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, clearCache));
        d->sourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeMoved, this, clearCache));
        d->sourceConnections.append(connect(sourceModel, &QAbstractItemModel::layoutAboutToBeChanged, this, clearCache));
        d->sourceConnections.append(connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset, this, clearCache));
        d->sourceConnections.append(connect(sourceModel, &QAbstractItemModel::dataChanged, this, clearCache));
    }

    LedgerFilter::setSourceModel(sourceModel);
}

bool LedgerSearchFilter::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    if (d->suspended || d->filterText.isEmpty()) {
        return false;
    }

    const auto idx = sourceModel()->index(source_row, 0, source_parent);
    const auto txId = idx.data(eMyMoney::Model::JournalTransactionIdRole).toString();
    if (txId != d->cachedTxId) {
        // evaluate the transaction once on its representative row: the first
        // split referencing an asset or liability account (so a transfer
        // shows up only once), falling back to the transaction's first row
        const auto file = MyMoneyFile::instance();
        const auto indexes = file->journalModel()->indexesByTransactionId(txId);
        if (indexes.isEmpty()) {
            return false;
        }
        auto representative = indexes.first();
        for (const auto& rowIndex : indexes) {
            const auto accountId = rowIndex.data(eMyMoney::Model::SplitAccountIdRole).toString();
            if (!accountId.isEmpty()) {
                try {
                    if (file->account(accountId).isAssetLiability()) {
                        representative = rowIndex;
                        break;
                    }
                } catch (const MyMoneyException&) {
                    // an orphaned split reference never represents the transaction
                }
            }
        }
        d->cachedTxId = txId;
        d->cachedRepresentativeRow = representative.row();
        d->cachedVerdict = LedgerFilter::filterAcceptsRow(representative.row(), source_parent);
    }
    return (source_row == d->cachedRepresentativeRow) && d->cachedVerdict;
}
