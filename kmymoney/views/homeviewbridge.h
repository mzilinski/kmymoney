/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HOMEVIEWBRIDGE_H
#define HOMEVIEWBRIDGE_H

#include <config-kmymoney.h>

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// QT Includes

#include <QObject>
#include <QString>

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneymoney.h"

class KHomeView;

/**
 * GUI-layer bridge between the experimental Kirigami/QML Home view and the rest of
 * the application. It exposes the read-only net-worth summary as text properties and
 * a single navigation entry point to QML, and relays clicks back into the widget
 * world through @c KHomeView::triggerActionForBridge().
 *
 * Read-only summary plus the few navigation/actions the home dashboard needs
 * (open a ledger, enter/skip a due schedule); general editing, reports and full
 * schedule management remain out of scope.
 */
class HomeViewBridge : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString netWorthText READ netWorthText NOTIFY summaryChanged)
    Q_PROPERTY(QString assetsText READ assetsText NOTIFY summaryChanged)
    Q_PROPERTY(QString liabilitiesText READ liabilitiesText NOTIFY summaryChanged)
    // Numeric magnitudes (absolute, base currency) for the QuickCharts net-worth pie.
    Q_PROPERTY(double assetsValue READ assetsValue NOTIFY summaryChanged)
    Q_PROPERTY(double liabilitiesValue READ liabilitiesValue NOTIFY summaryChanged)
    Q_PROPERTY(bool fileOpen READ fileOpen NOTIFY fileOpenChanged)

public:
    explicit HomeViewBridge(KHomeView* view, QObject* parent = nullptr);

    QString netWorthText() const;
    QString assetsText() const;
    QString liabilitiesText() const;
    double assetsValue() const;
    double liabilitiesValue() const;
    bool fileOpen() const;

    /// Opens the ledger of @p accountId.
    Q_INVOKABLE void openAccountLedger(const QString& accountId);
    /// Formats a base-currency magnitude for display (used by the pie legend).
    Q_INVOKABLE QString formatValue(double value) const;
    /// Records the next occurrence of schedule @p scheduleId (opens the enter dialog).
    Q_INVOKABLE void enterSchedule(const QString& scheduleId);
    /// Skips the next occurrence of schedule @p scheduleId.
    Q_INVOKABLE void skipSchedule(const QString& scheduleId);

public Q_SLOTS:
    /// Updates the welcome/dashboard state; called from the load/new/close paths.
    void setFileOpen(bool open);
    /// Recomputes the net-worth aggregates from the engine accounts model.
    void refreshSummary();

Q_SIGNALS:
    void summaryChanged();
    void fileOpenChanged();

private:
    KHomeView* m_view;
    bool m_fileOpen;
    MyMoneyMoney m_assets;
    MyMoneyMoney m_liabilities;
    MyMoneyMoney m_netWorth;
};

#endif // ENABLE_QML_HOME
#endif // HOMEVIEWBRIDGE_H
