/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "homeviewbridge.h"

#ifdef ENABLE_QML_HOME

// ----------------------------------------------------------------------------
// Project Includes

#include "accountsmodel.h"
#include "khomeview.h"
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

bool HomeViewBridge::fileOpen() const
{
    return m_fileOpen;
}

void HomeViewBridge::openAccountLedger(const QString& accountId)
{
    if (m_view && !accountId.isEmpty())
        m_view->triggerActionForBridge(eMenu::Action::GoToAccount, accountId);
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

#endif // ENABLE_QML_HOME
