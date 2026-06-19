/*
    SPDX-FileCopyrightText: 2013 Christian Dávid <christian-david@web.de>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CREDITTRANSFERSETTINGSBASE_H
#define CREDITTRANSFERSETTINGSBASE_H

#include "onlinetasks/sepa/sepaonlinetransfer.h"

/**
 * @brief Base class for sepaCreditTransfer and germanCreditTransfer settings
 *
 * @internal Both credit transfers have similar fields
 */
class creditTransferSettingsBase : public sepaOnlineTransfer::settings
{
public:
    creditTransferSettingsBase()
        : _purposeMaxLines(0)
        , _purposeLineLength(0)
        , _purposeMinLength(0)
        , _recipientNameMaxLines(0)
        , _recipientNameLength(0)
        , _recipientNameMinLength(0)
        , _payeeNameMaxLines(0)
        , _payeeNameLength(0)
        , _payeeNameMinLength(0)
        , m_endToEndReferenceLength(0)
        , m_supportsDatedTransfer(false)
        , m_minDatedTransferLeadDays(1)
        , m_maxDatedTransferLeadDays(0)
    {}

    // Limits getter
    int purposeMaxLines() const final override {
        return _purposeMaxLines;
    }
    int purposeLineLength() const final override {
        return _purposeLineLength;
    }
    int purposeMinLength() const final override {
        return _purposeMinLength;
    }

    int recipientNameLineLength() const final override {
        return _recipientNameLength;
    }
    int recipientNameMinLength() const final override {
        return _recipientNameMinLength;
    }

    int payeeNameLineLength() const final override {
        return _payeeNameLength;
    }
    int payeeNameMinLength() const final override {
        return _payeeNameMinLength;
    }

    QString allowedChars() const final override {
        return _allowedChars;
    }

    virtual int endToEndReferenceLength() const final override {
        return m_endToEndReferenceLength;
    }

    // LH-F-20: dated-transfer capability, derived from the account's transaction
    // limits for AB_Transaction_CommandSepaCreateDatedTransfer. supportsInstantTransfer()
    // is intentionally left at the base default (false): AqBanking has no instant command.
    bool supportsDatedTransfer() const final override
    {
        return m_supportsDatedTransfer;
    }
    int minDatedTransferLeadDays() const final override
    {
        return m_minDatedTransferLeadDays;
    }
    int maxDatedTransferLeadDays() const final override
    {
        return m_maxDatedTransferLeadDays;
    }

    // Checker
    bool checkPurposeCharset(const QString& string) const final override;
    bool checkPurposeLineLength(const QString& purpose) const final override;
    validators::lengthStatus checkPurposeLength(const QString& purpose) const final override;
    bool checkPurposeMaxLines(const QString& purpose) const final override;

    validators::lengthStatus checkNameLength(const QString& name) const final override;
    bool checkNameCharset(const QString& name) const final override;

    validators::lengthStatus checkRecipientLength(const QString& name) const final override;
    bool checkRecipientCharset(const QString& name) const final override;

    virtual validators::lengthStatus checkEndToEndReferenceLength(const QString& reference) const final override;

    virtual bool checkRecipientBic(const QString& bic) const final override;

    /**
     * @brief Checks if the bic is mandatory for the given iban
     *
     * For the check usually only the first two chars are needed. So you do not
     * need to validate the IBAN.
     *
     * There is no need to format fromIban or toIban in any way (it is trimmed automatically).
     */
    virtual bool isBicMandatory(const QString& fromIban, const QString& toIban) const final override;

    validators::lengthStatus checkRecipientAccountNumber(const QString& accountNumber) const;

    validators::lengthStatus checkRecipientBankCode(const QString& bankCode) const;

    // Limits setter
    void setEndToEndReferenceLength(const int& length) {
        m_endToEndReferenceLength = length;
    }

    void setPurposeLimits(const int& lines, const int& lineLength, const int& minLength) {
        _purposeMaxLines = lines;
        _purposeLineLength = lineLength;
        _purposeMinLength = minLength;
    }

    void setRecipientNameLimits(const int& lines, const int& lineLength, const int& minLength) {
        _recipientNameMaxLines = lines;
        _recipientNameLength = lineLength;
        _recipientNameMinLength = minLength;
    }

    void setPayeeNameLimits(const int& lines, const int& lineLength, const int& minLength) {
        _payeeNameMaxLines = lines;
        _payeeNameLength = lineLength;
        _payeeNameMinLength = minLength;
    }

    void setAllowedChars(QString characters) {
        _allowedChars = characters;
    }

    /**
     * @brief Mark dated transfers as supported and record the bank's lead-time window.
     * @param minLeadDays minimum days a dated transfer must lie in the future (>= 1)
     * @param maxLeadDays maximum lead days; 0 means unknown/unbounded
     */
    void setDatedTransferLimits(int minLeadDays, int maxLeadDays)
    {
        m_supportsDatedTransfer = true;
        m_minDatedTransferLeadDays = (minLeadDays > 0) ? minLeadDays : 1;
        m_maxDatedTransferLeadDays = (maxLeadDays > 0) ? maxLeadDays : 0;
    }

private:

    /** @brief number of lines allowed in purpose */
    int _purposeMaxLines;
    /** @brief number of chars allowed in each purpose line */
    int _purposeLineLength;
    /** @brief Minimal number of chars needed for purpose */
    int _purposeMinLength;

    /** @brief number of lines allowed for recipient name */
    int _recipientNameMaxLines;
    /** @brief number of chars allowed in each recipient line */
    int _recipientNameLength;
    /** @brief Minimal number of chars needed as recipient name */
    int _recipientNameMinLength;

    /** @brief number of lines allowed for payee name */
    int _payeeNameMaxLines;
    /** @brief number of chars allowed in each payee line */
    int _payeeNameLength;
    /** @brief Minibal number of chars for payee name */
    int _payeeNameMinLength;

    /** @brief characters allowed in purpose and recipient name */
    QString _allowedChars;

    /** @brief Number of chars allowed for sepa reference */
    int m_endToEndReferenceLength;

    /** @brief Whether the account/backend offers dated transfers (LH-F-20) */
    bool m_supportsDatedTransfer;
    /** @brief Minimum lead days for a dated transfer */
    int m_minDatedTransferLeadDays;
    /** @brief Maximum lead days for a dated transfer; 0 = unknown/unbounded */
    int m_maxDatedTransferLeadDays;
};

#endif // CREDITTRANSFERSETTINGSBASE_H
