/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SEPASTANDINGORDER_H
#define SEPASTANDINGORDER_H

#include "kmm_mymoney_export.h"

#include <QDate>

#include "misc/validators.h"
#include "onlinetasks/interfaces/tasks/credittransfer.h"
#include "onlinetasks/interfaces/tasks/onlinetask.h"
#include "payeeidentifier/ibanbic/ibanbic.h"

/**
 * @brief SEPA standing order (Dauerauftrag), LH-F-21.
 *
 * A recurring credit transfer the bank executes on a schedule (FinTS
 * HKCDE/HKCDB/HKCDN/HKCDL). Modelled as its own online task type — separate
 * from @ref sepaOnlineTransfer — because it carries a recurrence block and a
 * bank-assigned order id that are meaningless for a one-off transfer.
 */
class KMM_MYMONEY_EXPORT sepaStandingOrder : public onlineTask, public creditTransfer
{
public:
    ONLINETASK_META(sepaStandingOrder, "org.kmymoney.creditTransfer.sepa.standingOrder");
    sepaStandingOrder();
    sepaStandingOrder(const sepaStandingOrder& other);

    /**
     * @brief What this task does to a standing order at the bank.
     * Numeric values are part of the file format and must stay stable.
     */
    enum class Action : unsigned short {
        Create = 0, //!< create a new standing order (HKCDE)
        Modify = 1, //!< modify an existing one (HKCDN); requires bankOrderId + nextExecutionDate
        Delete = 2, //!< delete an existing one (HKCDL); requires bankOrderId + nextExecutionDate
    };

    //! @brief Recurrence period; numeric values are part of the file format.
    enum class Period : unsigned short {
        Monthly = 0,
        Weekly = 1,
    };

    virtual Action action() const = 0;
    virtual void setAction(Action action) = 0;

    virtual Period period() const = 0;
    virtual void setPeriod(Period period) = 0;

    //! @brief Runs every @c cycle periods (e.g. cycle 1 + Monthly = every month).
    virtual int cycle() const = 0;
    virtual void setCycle(int cycle) = 0;

    /**
     * @brief Day on which the order executes.
     * Monthly: day-of-month 1..31, or the ultimo encodings 97/98/99 (last,
     * last-but-one, last-but-two day of month). Weekly: day-of-week 1..7 (Mon=1).
     */
    virtual int executionDay() const = 0;
    virtual void setExecutionDay(int day) = 0;

    //! @brief First execution date (mandatory).
    virtual QDate firstExecutionDate() const = 0;
    virtual void setFirstExecutionDate(const QDate& date) = 0;

    //! @brief Optional last execution date (open-ended when invalid).
    virtual QDate lastExecutionDate() const = 0;
    virtual void setLastExecutionDate(const QDate& date) = 0;

    //! @brief Next execution date as reported by the bank; needed for modify/delete.
    virtual QDate nextExecutionDate() const = 0;
    virtual void setNextExecutionDate(const QDate& date) = 0;

    //! @brief Bank-assigned order id (fiId); required for modify/delete.
    virtual QString bankOrderId() const = 0;
    virtual void setBankOrderId(const QString& id) = 0;

    // --- credit-transfer payload (mirrors sepaOnlineTransfer) ---
    virtual void setOriginAccount(const QString& accountId) = 0;
    virtual void setValue(MyMoneyMoney value) = 0;

    virtual void setBeneficiary(const payeeIdentifiers::ibanBic& accountIdentifier) = 0;
    virtual payeeIdentifiers::ibanBic beneficiaryTyped() const = 0;

    virtual void setPurpose(const QString purpose) = 0;

    virtual void setEndToEndReference(const QString& reference) = 0;
    virtual QString endToEndReference() const = 0;

    virtual payeeIdentifier originAccountIdentifier() const = 0;

    virtual unsigned short int textKey() const = 0;
    virtual void setTextKey(unsigned short int textKey) = 0;
    virtual unsigned short int subTextKey() const = 0;
    virtual void setSubTextKey(unsigned short int subTextKey) = 0;

    // --- creditTransfer interface ---
    virtual MyMoneyMoney value() const override = 0;
    virtual QString purpose() const override = 0;
    virtual MyMoneySecurity currency() const override = 0;
    virtual QString responsibleAccount() const override = 0;

    // --- onlineTask interface ---
    virtual bool isValid() const override = 0;
    virtual QString jobTypeName() const override = 0;
    virtual bool hasReferenceTo(const QString& id) const override = 0;
    virtual KMMStringSet referencedObjects() const override = 0;

protected:
    virtual sepaStandingOrder* clone() const override = 0;
    virtual sepaStandingOrder* createFromXml(QXmlStreamReader* reader) const override = 0;
};

#endif // SEPASTANDINGORDER_H
