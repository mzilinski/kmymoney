/*
    SPDX-FileCopyrightText: 2013-2016 Christian Dávid <christian-david@web.de>
    SPDX-FileCopyrightText: 2019 Thomas Baumgart <tbaumgart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SEPAONLINETRANSFER_H
#define SEPAONLINETRANSFER_H

#include "kmm_mymoney_export.h"

#include <QDate>

#include "misc/validators.h"
#include "onlinetasks/interfaces/tasks/onlinetask.h"
#include "onlinetasks/interfaces/tasks/credittransfer.h"
#include "onlinetasks/interfaces/tasks/ionlinetasksettings.h"
#include "payeeidentifier/ibanbic/ibanbic.h"

/**
 * @brief SEPA Credit Transfer
 */
class KMM_MYMONEY_EXPORT sepaOnlineTransfer : public onlineTask, public creditTransfer
{

public:
    ONLINETASK_META(sepaOnlineTransfer, "org.kmymoney.creditTransfer.sepa");
    sepaOnlineTransfer();
    sepaOnlineTransfer(const sepaOnlineTransfer &other);

    /**
     * @brief Kind of SEPA credit transfer (LH-F-20).
     *
     * The numeric values are part of the on-disk file format (XML attribute and
     * SQL column) and must stay stable. @c Standard is the default and the only
     * value a file written before this feature can carry, so a missing/zero
     * value always decodes to @c Standard.
     */
    enum class TransferType : unsigned short {
        Standard = 0, //!< ordinary SEPA credit transfer (HKCCS / executed immediately)
        Instant = 1, //!< SEPA Instant credit transfer (reserved; no AqBanking command yet)
        Dated = 2, //!< dated transfer executed by the bank on @ref executionDate() (HKCSE)
    };

    virtual TransferType transferType() const = 0;
    virtual void setTransferType(TransferType type) = 0;

    /**
     * @brief Date on which a @ref TransferType::Dated transfer shall be executed.
     * Invalid for all other transfer types.
     */
    virtual QDate executionDate() const = 0;
    virtual void setExecutionDate(const QDate& date) = 0;

    virtual QString responsibleAccount() const override = 0;
    virtual void setOriginAccount(const QString& accountId) = 0;

    virtual MyMoneyMoney value() const override = 0;
    virtual void setValue(MyMoneyMoney value) = 0;

    virtual void setBeneficiary(const payeeIdentifiers::ibanBic& accountIdentifier) = 0;
    virtual payeeIdentifiers::ibanBic beneficiaryTyped() const = 0;

    virtual void setPurpose(const QString purpose) = 0;
    virtual QString purpose() const override = 0;

    virtual void setEndToEndReference(const QString& reference) = 0;
    virtual QString endToEndReference() const = 0;

    /**
     * @brief Returns the origin account identifier
     * @return you are owner of the object
     */
    virtual payeeIdentifier originAccountIdentifier() const = 0;

    /**
     * National account can handle the currency of the related account only.
     */
    virtual MyMoneySecurity currency() const override = 0;

    virtual bool isValid() const override = 0;

    virtual QString jobTypeName() const override = 0;

    virtual unsigned short int textKey() const = 0;
    virtual void setTextKey(unsigned short int textKey) = 0;
    virtual unsigned short int subTextKey() const = 0;
    virtual void setSubTextKey(unsigned short int setSubTextKey) = 0;

    virtual bool hasReferenceTo(const QString& id) const override = 0;

    /**
     * @copydoc MyMoneyObject::referencedObjects
     */
    virtual KMMStringSet referencedObjects() const override = 0;

    class settings : public IonlineTaskSettings
    {
    public:
        // Limits getter
        virtual int purposeMaxLines() const = 0;
        virtual int purposeLineLength() const = 0;
        virtual int purposeMinLength() const = 0;

        virtual int recipientNameLineLength() const = 0;
        virtual int recipientNameMinLength() const = 0;

        virtual int payeeNameLineLength() const = 0;
        virtual int payeeNameMinLength() const = 0;

        virtual QString allowedChars() const = 0;

        // Checker
        virtual bool checkPurposeCharset(const QString& string) const = 0;
        virtual bool checkPurposeLineLength(const QString& purpose) const = 0;
        virtual validators::lengthStatus checkPurposeLength(const QString& purpose) const = 0;
        virtual bool checkPurposeMaxLines(const QString& purpose) const = 0;

        virtual validators::lengthStatus checkNameLength(const QString& name) const = 0;
        virtual bool checkNameCharset(const QString& name) const = 0;

        virtual validators::lengthStatus checkRecipientLength(const QString& name) const = 0;
        virtual bool checkRecipientCharset(const QString& name) const = 0;

        virtual int endToEndReferenceLength() const = 0;
        virtual validators::lengthStatus checkEndToEndReferenceLength(const QString& reference) const = 0;

        virtual bool checkRecipientBic(const QString& bic) const = 0;

        /**
         * @brief Checks if the bic is mandatory for the given iban
         *
         * For the check usually only the first two chars are needed. So you do not
         * need to validate the IBAN.
         *
         * @todo LOW: Implement, should be simple to test: if the country code in iban is the same as in origin iban and
         * the iban belongs to a sepa country a bic is not necessary. Will change 1. Feb 2016.
         */
        virtual bool isBicMandatory(const QString& payeeiban, const QString& beneficiaryIban) const = 0;

        /**
         * @brief Whether the account/backend offers dated transfers (HKCSE, LH-F-20).
         *
         * Non-pure on purpose: every existing @ref settings implementation keeps
         * compiling unchanged and reports "not supported" until a backend that
         * probes the capability overrides it.
         */
        virtual bool supportsDatedTransfer() const
        {
            return false;
        }
        //! @brief Whether the account/backend offers SEPA Instant transfers (no AqBanking command exists yet).
        virtual bool supportsInstantTransfer() const
        {
            return false;
        }
        //! @brief Minimum number of (calendar) days a dated transfer must lie in the future.
        virtual int minDatedTransferLeadDays() const
        {
            return 1;
        }
        //! @brief Maximum lead days for a dated transfer; 0 means unknown/unbounded.
        virtual int maxDatedTransferLeadDays() const
        {
            return 0;
        }
    };

    virtual QSharedPointer<const settings> getSettings() const = 0;

protected:
    virtual sepaOnlineTransfer* clone() const override = 0;

    virtual sepaOnlineTransfer* createFromXml(QXmlStreamReader* reader) const override = 0;
};

#endif // SEPAONLINETRANSFER_H
