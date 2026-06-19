/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sepastandingorderimpl.h"

#include <QVariant>
#include <QXmlStreamReader>

#include "mymoney/mymoneyfile.h"
#include "mymoneyaccount.h"
#include "mymoneyinstitution.h"
#include "mymoneypayee.h"
#include "mymoneysecurity.h"
#include "payeeidentifier/ibanbic/ibanbic.h"
#include "payeeidentifiertyped.h"
#include "xmlhelper/xmlstoragehelper.h"

static const unsigned short defaultTextKey = 51;
static const unsigned short defaultSubTextKey = 0;

sepaStandingOrderImpl::sepaStandingOrderImpl()
    : sepaStandingOrder()
    , _originAccount(QString())
    , _value(0)
    , _purpose(QString())
    , _endToEndReference(QString())
    , _action(Action::Create)
    , _period(Period::Monthly)
    , _cycle(1)
    , _executionDay(0)
    , _firstExecutionDate(QDate())
    , _lastExecutionDate(QDate())
    , _nextExecutionDate(QDate())
    , _bankOrderId(QString())
    , _beneficiaryAccount(payeeIdentifiers::ibanBic())
    , _textKey(defaultTextKey)
    , _subTextKey(defaultSubTextKey)
{
}

sepaStandingOrderImpl::sepaStandingOrderImpl(const sepaStandingOrderImpl& other)
    : sepaStandingOrder(other)
    , _originAccount(other._originAccount)
    , _value(other._value)
    , _purpose(other._purpose)
    , _endToEndReference(other._endToEndReference)
    , _action(other._action)
    , _period(other._period)
    , _cycle(other._cycle)
    , _executionDay(other._executionDay)
    , _firstExecutionDate(other._firstExecutionDate)
    , _lastExecutionDate(other._lastExecutionDate)
    , _nextExecutionDate(other._nextExecutionDate)
    , _bankOrderId(other._bankOrderId)
    , _beneficiaryAccount(other._beneficiaryAccount)
    , _textKey(other._textKey)
    , _subTextKey(other._subTextKey)
{
}

sepaStandingOrder* sepaStandingOrderImpl::clone() const
{
    return new sepaStandingOrderImpl(*this);
}

bool sepaStandingOrderImpl::isValid() const
{
    // Deterministic (no current-date comparison): the recurrence schedule must be
    // fully specified and the payment leg must be a valid positive SEPA transfer.
    // The bank's per-account limits (allowed cycle/executionDay values) are
    // enforced by the editor and the kbanking send path, not here.
    if (!_beneficiaryAccount.isIbanValid())
        return false;
    if (!_value.isPositive())
        return false;
    if (_executionDay <= 0)
        return false;
    if (!_firstExecutionDate.isValid())
        return false;

    // Modifying or deleting an order requires the bank's identification of it.
    if (_action == Action::Modify || _action == Action::Delete) {
        if (_bankOrderId.isEmpty() || !_nextExecutionDate.isValid())
            return false;
    }
    return true;
}

payeeIdentifier sepaStandingOrderImpl::originAccountIdentifier() const
{
    if (!_originAccount.isEmpty()) {
        payeeIdentifierTyped<payeeIdentifiers::ibanBic> ident = payeeIdentifierTyped<payeeIdentifiers::ibanBic>(new payeeIdentifiers::ibanBic);
        auto acc = MyMoneyFile::instance()->account(_originAccount);
        ident->setIban(acc.value(QStringLiteral("iban")));

        if (!acc.institutionId().isEmpty()) {
            const auto institution = MyMoneyFile::instance()->institution(acc.institutionId());
            ident->setBic(institution.value(QStringLiteral("bic")));
        }

        ident->setOwnerName(MyMoneyFile::instance()->user().name());
        return ident;
    }
    return payeeIdentifier(new payeeIdentifiers::ibanBic);
}

MyMoneySecurity sepaStandingOrderImpl::currency() const
{
    if (!_originAccount.isEmpty()) {
        const QString currencyId = MyMoneyFile::instance()->account(_originAccount).currencyId();
        return MyMoneyFile::instance()->security(currencyId);
    }
    return MyMoneyFile::instance()->baseCurrency();
}

void sepaStandingOrderImpl::setOriginAccount(const QString& accountId)
{
    _originAccount = accountId;
}

void sepaStandingOrderImpl::writeXML(QXmlStreamWriter* writer) const
{
    writer->writeAttribute("originAccount", _originAccount);
    writer->writeAttribute("value", _value.toString());
    writer->writeAttribute("textKey", QString::number(_textKey));
    writer->writeAttribute("subTextKey", QString::number(_subTextKey));

    if (!_purpose.isEmpty())
        writer->writeAttribute("purpose", _purpose);
    if (!_endToEndReference.isEmpty())
        writer->writeAttribute("endToEndReference", _endToEndReference);

    // Recurrence block. action/lastExecutionDate/nextExecutionDate/bankOrderId
    // are written only when they carry information, so a plain create order stays
    // compact; period/cycle/executionDay/firstExecutionDate are intrinsic.
    if (_action != Action::Create)
        writer->writeAttribute("action", QString::number(static_cast<unsigned short>(_action)));
    writer->writeAttribute("period", QString::number(static_cast<unsigned short>(_period)));
    writer->writeAttribute("cycle", QString::number(_cycle));
    writer->writeAttribute("executionDay", QString::number(_executionDay));
    if (_firstExecutionDate.isValid())
        writer->writeAttribute("firstExecutionDate", _firstExecutionDate.toString(Qt::ISODate));
    if (_lastExecutionDate.isValid())
        writer->writeAttribute("lastExecutionDate", _lastExecutionDate.toString(Qt::ISODate));
    if (_nextExecutionDate.isValid())
        writer->writeAttribute("nextExecutionDate", _nextExecutionDate.toString(Qt::ISODate));
    if (!_bankOrderId.isEmpty())
        writer->writeAttribute("bankOrderId", _bankOrderId);

    writer->writeStartElement("beneficiary");
    _beneficiaryAccount.writeXML(writer);
    writer->writeEndElement();
}

sepaStandingOrder* sepaStandingOrderImpl::createFromXml(QXmlStreamReader* reader) const
{
    sepaStandingOrderImpl* task = new sepaStandingOrderImpl();
    task->setOriginAccount(MyMoneyXmlHelper::readStringAttribute(reader, QLatin1String("originAccount")));
    task->setValue(MyMoneyXmlHelper::readValueAttribute(reader, QLatin1String("value")));
    task->_textKey = MyMoneyXmlHelper::readUintAttribute(reader, QLatin1String("textKey"), defaultTextKey);
    task->_subTextKey = MyMoneyXmlHelper::readUintAttribute(reader, QLatin1String("subTextKey"), defaultSubTextKey);
    task->setPurpose(MyMoneyXmlHelper::readStringAttribute(reader, QLatin1String("purpose")));
    task->setEndToEndReference(MyMoneyXmlHelper::readStringAttribute(reader, QLatin1String("endToEndReference")));

    // A missing/unknown action decodes to Create, a missing period to Monthly.
    const auto rawAction = MyMoneyXmlHelper::readUintAttribute(reader, QLatin1String("action"), 0);
    if (rawAction == static_cast<unsigned int>(Action::Modify))
        task->_action = Action::Modify;
    else if (rawAction == static_cast<unsigned int>(Action::Delete))
        task->_action = Action::Delete;
    else
        task->_action = Action::Create;

    task->_period = (MyMoneyXmlHelper::readUintAttribute(reader, QLatin1String("period"), 0) == static_cast<unsigned int>(Period::Weekly)) ? Period::Weekly
                                                                                                                                           : Period::Monthly;
    task->_cycle = MyMoneyXmlHelper::readUintAttribute(reader, QLatin1String("cycle"), 1);
    task->_executionDay = MyMoneyXmlHelper::readUintAttribute(reader, QLatin1String("executionDay"), 0);
    task->_firstExecutionDate = QDate::fromString(MyMoneyXmlHelper::readStringAttribute(reader, QLatin1String("firstExecutionDate")), Qt::ISODate);
    task->_lastExecutionDate = QDate::fromString(MyMoneyXmlHelper::readStringAttribute(reader, QLatin1String("lastExecutionDate")), Qt::ISODate);
    task->_nextExecutionDate = QDate::fromString(MyMoneyXmlHelper::readStringAttribute(reader, QLatin1String("nextExecutionDate")), Qt::ISODate);
    task->_bankOrderId = MyMoneyXmlHelper::readStringAttribute(reader, QLatin1String("bankOrderId"));

    payeeIdentifiers::ibanBic beneficiary;
    payeeIdentifiers::ibanBic* beneficiaryPtr = nullptr;

    while (reader->readNextStartElement()) {
        if (reader->name() == QLatin1String("beneficiary")) {
            delete beneficiaryPtr;
            beneficiaryPtr = beneficiary.createFromXml(reader);
        } else {
            reader->skipCurrentElement();
        }
    }

    if (beneficiaryPtr == nullptr) {
        task->_beneficiaryAccount = beneficiary;
    } else {
        task->_beneficiaryAccount = *beneficiaryPtr;
        delete beneficiaryPtr;
    }

    return task;
}

bool sepaStandingOrderImpl::hasReferenceTo(const QString& id) const
{
    return (id == _originAccount);
}

KMMStringSet sepaStandingOrderImpl::referencedObjects() const
{
    return KMMStringSet(QStringList{_originAccount});
}

QString sepaStandingOrderImpl::jobTypeName() const
{
    return QLatin1String("SEPA Standing Order");
}
