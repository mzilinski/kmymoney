/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "homeviewbridge.h"

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// QT Includes

#include <QSet>

// ----------------------------------------------------------------------------
// Project Includes

#include "accountsmodel.h"
#include "khomeview.h"
#include "kmymoneysettings.h"
#include "menuenums.h"
#include "mymoneyenums.h"
#include "mymoneyexception.h"
#include "mymoneyfile.h"
#include "mymoneysecurity.h"

namespace {
// Formats a base-currency amount the way the header should read. baseCurrency() and
// formatMoney() throw when no file is open, so callers must only format with a file
// open; we additionally guard with try/catch to stay crash-safe during teardown.
QString formatBaseCurrency(const MyMoneyMoney& value, bool fileOpen)
{
    if (!fileOpen)
        return QString();
    try {
        const auto base = MyMoneyFile::instance()->baseCurrency();
        return value.formatMoney(base.tradingSymbol(), MyMoneyMoney::denomToPrec(base.smallestAccountFraction()));
    } catch (const MyMoneyException&) {
        return QString();
    }
}
}

HomeViewBridge::HomeViewBridge(KHomeView* view, QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_fileOpen(false)
{
    // The net-worth signal is emit-on-change and aggregate-only, so seed once now and
    // refresh on every change. Per-account edits that do not move the aggregate are
    // covered by loadView() calling refreshSummary() on each refresh().
    connect(MyMoneyFile::instance()->accountsModel(), &AccountsModel::netWorthChanged, this, &HomeViewBridge::refreshSummary);
    refreshSummary();
}

QString HomeViewBridge::netWorthText() const
{
    return formatBaseCurrency(m_netWorth, m_fileOpen);
}

QString HomeViewBridge::assetsText() const
{
    return formatBaseCurrency(m_assets, m_fileOpen);
}

QString HomeViewBridge::liabilitiesText() const
{
    return formatBaseCurrency(m_liabilities, m_fileOpen);
}

double HomeViewBridge::assetsValue() const
{
    // Absolute magnitude for the pie wedge (a pie cannot render a negative area; the
    // sign is conveyed by the header text). Zero when no file is open.
    return m_fileOpen ? qAbs(m_assets.toDouble()) : 0.0;
}

double HomeViewBridge::liabilitiesValue() const
{
    // m_liabilities is stored in net-worth convention (negative); take the magnitude.
    return m_fileOpen ? qAbs(m_liabilities.toDouble()) : 0.0;
}

bool HomeViewBridge::fileOpen() const
{
    return m_fileOpen;
}

void HomeViewBridge::openAccountLedger(const QString& accountId)
{
    if (m_view && !accountId.isEmpty())
        m_view->triggerActionForBridge(eMenu::Action::GoToAccount, accountId);
}

void HomeViewBridge::enterSchedule(const QString& scheduleId)
{
    if (m_view && !scheduleId.isEmpty())
        m_view->triggerActionForBridge(eMenu::Action::EnterSchedule, scheduleId);
}

void HomeViewBridge::skipSchedule(const QString& scheduleId)
{
    if (m_view && !scheduleId.isEmpty())
        m_view->triggerActionForBridge(eMenu::Action::SkipSchedule, scheduleId);
}

QString HomeViewBridge::formatValue(double value) const
{
    if (!m_fileOpen)
        return QString();
    try {
        // Construct with the base-currency fraction (not the default denom 100) so the
        // legend keeps the same precision as assetsText()/liabilitiesText() for
        // currencies whose smallest fraction is not 1/100 (e.g. dinar 1/1000).
        const auto fraction = MyMoneyFile::instance()->baseCurrency().smallestAccountFraction();
        return formatBaseCurrency(MyMoneyMoney(value, fraction), m_fileOpen);
    } catch (const MyMoneyException&) {
        return QString();
    }
}

void HomeViewBridge::setFileOpen(bool open)
{
    if (m_fileOpen == open)
        return;
    m_fileOpen = open;
    Q_EMIT fileOpenChanged();
    // Re-read so the header reflects the new file (or zeroes on close).
    refreshSummary();
}

void HomeViewBridge::refreshSummary()
{
    auto* model = MyMoneyFile::instance()->accountsModel();
    // The asset/liability group indices are valid even with empty storage and then
    // return zero, so this never throws (mirrors AccountsModelPrivate::netWorth()).
    m_assets = model->assetIndex().data(eMyMoney::Model::AccountTotalValueRole).value<MyMoneyMoney>();
    m_liabilities = model->liabilityIndex().data(eMyMoney::Model::AccountTotalValueRole).value<MyMoneyMoney>();
    m_netWorth = m_assets + m_liabilities;
    Q_EMIT summaryChanged();
}

bool HomeViewBridge::showScheduledPayments() const
{
    return m_showScheduledPayments;
}

bool HomeViewBridge::showAccounts() const
{
    return m_showAccounts;
}

bool HomeViewBridge::showAssetsLiabilities() const
{
    return m_showAssetsLiabilities;
}

void HomeViewBridge::refreshSections()
{
    // listOfItems() returns the classic home-page items; a hidden item is encoded as a
    // negative code (e.g. "-8"), a shown one as its positive code. Collect the shown codes
    // and map the three that have a QML counterpart. Codes without a QML section
    // (4 reports, 5/7 forecast, 6 net-worth line graph, 9 budget, 10 cash flow) are ignored.
    QSet<int> shown;
    const auto items = KMyMoneySettings::listOfItems();
    for (const auto& item : items) {
        const int code = item.toInt();
        if (code > 0)
            shown.insert(code);
    }

    const bool schedules = shown.contains(1);
    const bool accounts = shown.contains(2) || shown.contains(3);
    const bool assetsLiabilities = shown.contains(8);

    if (schedules == m_showScheduledPayments && accounts == m_showAccounts && assetsLiabilities == m_showAssetsLiabilities)
        return;

    m_showScheduledPayments = schedules;
    m_showAccounts = accounts;
    m_showAssetsLiabilities = assetsLiabilities;
    Q_EMIT sectionsChanged();
}

#endif // ENABLE_QML_HOME
