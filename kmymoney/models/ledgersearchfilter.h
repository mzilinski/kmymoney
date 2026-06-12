/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LEDGERSEARCHFILTER_H
#define LEDGERSEARCHFILTER_H

#include "kmm_models_export.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <memory>

class QLineEdit;

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// Project Includes

#include "ledgerfilter.h"

class LedgerSearchFilterPrivate;

/**
 * A LedgerFilter for an instant search across the transactions of
 * all accounts. It must be sourced directly on the JournalModel
 * (which carries one row per split) and
 *
 * - shows nothing while the filter expression is empty or the
 *   filter is suspended,
 * - reduces every matching transaction to a single row (the first
 *   split referencing an asset or liability account, falling back
 *   to the transaction's first row),
 * - owns the debounce of an attached line edit.
 *
 * @note All filter changes must go through setFilterFixedString()
 * (or the attached line edit); the bypassing base class mutators
 * are deleted below.
 *
 * @note The match is evaluated on the representative row, so fields
 * that the base class only reads from the evaluated row itself
 * (payee, number, activity, tags, formatted amounts) are not found
 * when they are set only on another split, e.g. the second bank side
 * of a transfer. Account names and memos of all splits are covered.
 */
class KMM_MODELS_EXPORT LedgerSearchFilter : public LedgerFilter
{
    Q_OBJECT
    Q_DISABLE_COPY(LedgerSearchFilter)

public:
    explicit LedgerSearchFilter(QObject* parent);
    ~LedgerSearchFilter() override;

    /**
     * Use @a lineEdit as the source of the filter expression. Changes
     * are applied debounced through setFilterFixedString(). May only
     * be called once per instance.
     */
    void attachLineEdit(QLineEdit* lineEdit);

    /**
     * Apply a still pending (debounced) expression of the attached
     * line edit immediately. No-op while nothing is pending.
     */
    void flushPendingFilter();

    /**
     * The single mutation funnel of this class: invalidates the
     * per-transaction match cache, records @a pattern, forwards to the
     * base class (which runs the filter synchronously) and emits
     * filterApplied().
     *
     * @note Intentionally shadows the non-virtual base class method.
     */
    void setFilterFixedString(const QString& pattern);

    /**
     * While suspended, all rows are rejected without inspecting them,
     * so that imports or file loads happening while the embedding
     * widget is hidden never cause a full scan. The filter expression
     * is kept and applied again when resuming.
     */
    void setSuspended(bool suspended);

    /**
     * Reimplemented to maintain the per-transaction match cache across
     * source model changes.
     */
    void setSourceModel(QAbstractItemModel* sourceModel) override;

    // these base class mutators would bypass the filter expression and
    // cache bookkeeping of this class
    void setLineEdit(QLineEdit*) = delete;
    void clearFilter() = delete;
    void setStateFilter(LedgerFilter::State) = delete;
    void setEndDate(const QDate&) = delete;

Q_SIGNALS:
    /**
     * Emitted whenever a filter expression has been applied or the
     * filter resumed. In contrast to the row signals this also fires
     * when the set of accepted rows did not change (e.g. one no-match
     * expression replacing another).
     */
    void filterApplied();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
    std::unique_ptr<LedgerSearchFilterPrivate> d;
};

#endif // LEDGERSEARCHFILTER_H
