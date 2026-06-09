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
    // Per-section visibility derived from the classic home-page ItemList preference
    // (KMyMoneySettings::listOfItems()), so hiding a section in Settings ▸ Home page also
    // hides it here. Only the three classic codes that have a QML counterpart are honored:
    // 1 = scheduled payments, 2|3 = accounts, 8 = assets & liabilities (which gates both the
    // net-worth header and the assets-vs-liabilities pie). The QML-only Account Allocation
    // pie has no classic code and stays always-on. Order is NOT honored (fixed layout).
    Q_PROPERTY(bool showScheduledPayments READ showScheduledPayments NOTIFY sectionsChanged)
    Q_PROPERTY(bool showAccounts READ showAccounts NOTIFY sectionsChanged)
    Q_PROPERTY(bool showAssetsLiabilities READ showAssetsLiabilities NOTIFY sectionsChanged)

public:
    explicit HomeViewBridge(KHomeView* view, QObject* parent = nullptr);

    QString netWorthText() const;
    QString assetsText() const;
    QString liabilitiesText() const;
    double assetsValue() const;
    double liabilitiesValue() const;
    bool fileOpen() const;
    bool showScheduledPayments() const;
    bool showAccounts() const;
    bool showAssetsLiabilities() const;

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
    /// Re-reads the home-page ItemList preference and updates the per-section visibility
    /// flags. Called from the home view's load path (separate from refreshSummary(), which
    /// also fires on every balance change and must not re-parse the settings each time).
    void refreshSections();

Q_SIGNALS:
    void summaryChanged();
    void fileOpenChanged();
    void sectionsChanged();

private:
    KHomeView* m_view;
    bool m_fileOpen;
    MyMoneyMoney m_assets;
    MyMoneyMoney m_liabilities;
    MyMoneyMoney m_netWorth;
    bool m_showScheduledPayments = true;
    bool m_showAccounts = true;
    bool m_showAssetsLiabilities = true;
};

#endif // ENABLE_QML_HOME
#endif // HOMEVIEWBRIDGE_H
