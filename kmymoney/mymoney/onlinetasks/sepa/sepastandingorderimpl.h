/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SEPASTANDINGORDERIMPL_H
#define SEPASTANDINGORDERIMPL_H

#include "kmm_mymoney_export.h"

#include "mymoneymoney.h"
#include "sepastandingorder.h"

/**
 * @brief Concrete SEPA standing order (LH-F-21).
 */
class KMM_MYMONEY_EXPORT sepaStandingOrderImpl : public sepaStandingOrder
{
public:
    ONLINETASK_META(sepaStandingOrder, "org.kmymoney.creditTransfer.sepa.standingOrder");
    sepaStandingOrderImpl();
    sepaStandingOrderImpl(const sepaStandingOrderImpl& other);

    Action action() const final override
    {
        return _action;
    }
    void setAction(Action action) final override
    {
        _action = action;
    }

    Period period() const final override
    {
        return _period;
    }
    void setPeriod(Period period) final override
    {
        _period = period;
    }

    int cycle() const final override
    {
        return _cycle;
    }
    void setCycle(int cycle) final override
    {
        _cycle = cycle;
    }

    int executionDay() const final override
    {
        return _executionDay;
    }
    void setExecutionDay(int day) final override
    {
        _executionDay = day;
    }

    QDate firstExecutionDate() const final override
    {
        return _firstExecutionDate;
    }
    void setFirstExecutionDate(const QDate& date) final override
    {
        _firstExecutionDate = date;
    }

    QDate lastExecutionDate() const final override
    {
        return _lastExecutionDate;
    }
    void setLastExecutionDate(const QDate& date) final override
    {
        _lastExecutionDate = date;
    }

    QDate nextExecutionDate() const final override
    {
        return _nextExecutionDate;
    }
    void setNextExecutionDate(const QDate& date) final override
    {
        _nextExecutionDate = date;
    }

    QString bankOrderId() const final override
    {
        return _bankOrderId;
    }
    void setBankOrderId(const QString& id) final override
    {
        _bankOrderId = id;
    }

    QString responsibleAccount() const final override
    {
        return _originAccount;
    }
    void setOriginAccount(const QString& accountId) final override;

    MyMoneyMoney value() const final override
    {
        return _value;
    }
    void setValue(MyMoneyMoney value) final override
    {
        _value = value;
    }

    void setBeneficiary(const payeeIdentifiers::ibanBic& accountIdentifier) final override
    {
        _beneficiaryAccount = accountIdentifier;
    }
    payeeIdentifier beneficiary() const final override
    {
        return payeeIdentifier(_beneficiaryAccount.clone());
    }
    payeeIdentifiers::ibanBic beneficiaryTyped() const final override
    {
        return _beneficiaryAccount;
    }

    void setPurpose(const QString purpose) final override
    {
        _purpose = purpose;
    }
    QString purpose() const final override
    {
        return _purpose;
    }

    void setEndToEndReference(const QString& reference) final override
    {
        _endToEndReference = reference;
    }
    QString endToEndReference() const final override
    {
        return _endToEndReference;
    }

    payeeIdentifier originAccountIdentifier() const final override;

    MyMoneySecurity currency() const final override;

    bool isValid() const final override;

    QString jobTypeName() const final override;

    unsigned short int textKey() const final override
    {
        return _textKey;
    }
    void setTextKey(unsigned short int textKey) final override
    {
        _textKey = textKey;
    }

    unsigned short int subTextKey() const final override
    {
        return _subTextKey;
    }
    void setSubTextKey(unsigned short int subTextKey) final override
    {
        _subTextKey = subTextKey;
    }

    bool hasReferenceTo(const QString& id) const final override;

    KMMStringSet referencedObjects() const override;

    void writeXML(QXmlStreamWriter* writer) const final override;

protected:
    sepaStandingOrder* clone() const final override;

    sepaStandingOrder* createFromXml(QXmlStreamReader* reader) const final override;

private:
    QString _originAccount;
    MyMoneyMoney _value;
    QString _purpose;
    QString _endToEndReference;

    Action _action = Action::Create;
    Period _period = Period::Monthly;
    int _cycle = 1;
    int _executionDay = 0;
    QDate _firstExecutionDate;
    QDate _lastExecutionDate;
    QDate _nextExecutionDate;
    QString _bankOrderId;

    payeeIdentifiers::ibanBic _beneficiaryAccount;

    unsigned short int _textKey;
    unsigned short int _subTextKey;
};

class MyMoneyFile;

/**
 * @brief Merge bank-retrieved standing orders into the file's online jobs (LH-F-21 T2).
 *
 * Upserts each task in @p retrieved (which must carry Action::Retrieved + a
 * bankOrderId) as a read-only documentary online job, keyed by
 * (responsibleAccount, bankOrderId), and PRUNES the existing bank-sourced
 * records for @p accountId whose bankOrderId is no longer present (deleted at
 * the bank). User-authored jobs and other accounts are never touched. Pure
 * over the file's online-job model — call only after a *successful* retrieval
 * (a failed Abruf must not prune), inside a MyMoneyFileTransaction.
 */
KMM_MYMONEY_EXPORT void mergeRetrievedStandingOrders(MyMoneyFile* file, const QString& accountId, const QList<sepaStandingOrderImpl>& retrieved);

#endif // SEPASTANDINGORDERIMPL_H
