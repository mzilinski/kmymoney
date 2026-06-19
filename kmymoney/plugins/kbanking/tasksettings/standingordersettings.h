/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef STANDINGORDERSETTINGS_H
#define STANDINGORDERSETTINGS_H

#include "onlinetasks/sepa/sepastandingorder.h"

/**
 * @brief Concrete standing-order capability, filled from an account's AqBanking
 * transaction limits (LH-F-21). Built by AB_TransactionLimits_toStandingOrderSettings().
 */
class standingOrderSettings : public sepaStandingOrder::settings
{
public:
    standingOrderSettings() = default;

    bool supportsStandingOrders() const final override
    {
        return m_supported;
    }
    bool allowMonthly() const final override
    {
        return m_allowMonthly;
    }
    bool allowWeekly() const final override
    {
        return m_allowWeekly;
    }
    QList<int> allowedCyclesMonthly() const final override
    {
        return m_cyclesMonthly;
    }
    QList<int> allowedCyclesWeekly() const final override
    {
        return m_cyclesWeekly;
    }
    QList<int> allowedExecutionDaysMonthly() const final override
    {
        return m_daysMonthly;
    }
    QList<int> allowedExecutionDaysWeekly() const final override
    {
        return m_daysWeekly;
    }

    void setSupported(bool supported)
    {
        m_supported = supported;
    }
    void setAllowMonthly(bool allow)
    {
        m_allowMonthly = allow;
    }
    void setAllowWeekly(bool allow)
    {
        m_allowWeekly = allow;
    }
    void setAllowedCyclesMonthly(const QList<int>& values)
    {
        m_cyclesMonthly = values;
    }
    void setAllowedCyclesWeekly(const QList<int>& values)
    {
        m_cyclesWeekly = values;
    }
    void setAllowedExecutionDaysMonthly(const QList<int>& values)
    {
        m_daysMonthly = values;
    }
    void setAllowedExecutionDaysWeekly(const QList<int>& values)
    {
        m_daysWeekly = values;
    }

private:
    bool m_supported = false;
    bool m_allowMonthly = false;
    bool m_allowWeekly = false;
    QList<int> m_cyclesMonthly;
    QList<int> m_cyclesWeekly;
    QList<int> m_daysMonthly;
    QList<int> m_daysWeekly;
};

#endif // STANDINGORDERSETTINGS_H
