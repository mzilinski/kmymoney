/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "homeviewbridge.h"

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// QT Includes

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
    // seed the section order before the very first QML read; listOfItems() is
    // config-only, so this is safe without an open file
    refreshSections();
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

QStringList HomeViewBridge::sectionOrder() const
{
    return m_sectionOrder;
}

void HomeViewBridge::refreshSections()
{
    // One ordered pass over the classic home-page ItemList: list order is render
    // order, a hidden item is encoded as a negative code (e.g. "-8"). Mirrors the
    // classic loadView() dispatch (only positive codes render). Codes without a QML
    // counterpart (4 reports, 5/7 forecast, 6 net-worth line graph, 9 budget,
    // 10 cash flow - still rendered by the classic HTML view) and unknown codes
    // are skipped.
    QStringList order;
    const auto items = KMyMoneySettings::listOfItems();
    for (const auto& item : items) {
        const int code = item.toInt();
        switch (code) {
        case 1:
            if (!order.contains(QLatin1String("schedules")))
                order << QStringLiteral("schedules");
            break;
        case 2:
        case 3:
            // one merged QML accounts section at the first shown of 2|3
            if (!order.contains(QLatin1String("accounts")))
                order << QStringLiteral("accounts");
            break;
        case 8:
        case -8:
            // The QML-only allocation pie has no classic code: it is always-on and
            // anchors at the list position of the wealth-overview entry regardless
            // of its sign, so it travels with the block when the user reorders.
            // The header+pie pair itself only renders when the entry is shown.
            if (code > 0 && !order.contains(QLatin1String("netWorthHeader")))
                order << QStringLiteral("netWorthHeader") << QStringLiteral("netWorthPie");
            if (!order.contains(QLatin1String("allocation")))
                order << QStringLiteral("allocation");
            break;
        default:
            break;
        }
    }
    // belt and braces: listOfItems()'s tail-merge guarantees an 8/-8 entry, but the
    // always-on allocation pie must never get lost
    if (!order.contains(QLatin1String("allocation")))
        order.prepend(QStringLiteral("allocation"));

    // the bool states are a pure function of the list, so this covers them too
    if (order == m_sectionOrder)
        return;

    m_sectionOrder = order;
    m_showScheduledPayments = order.contains(QLatin1String("schedules"));
    m_showAccounts = order.contains(QLatin1String("accounts"));
    m_showAssetsLiabilities = order.contains(QLatin1String("netWorthHeader"));
    Q_EMIT sectionsChanged();
}

#endif // ENABLE_QML_HOME
