/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SEPASTANDINGORDEREDIT_H
#define SEPASTANDINGORDEREDIT_H

#include <KLocalizedString>

#include "mymoney/onlinejobtyped.h"
#include "onlinetasks/interfaces/ui/ionlinejobedit.h"
#include "onlinetasks/sepa/sepastandingorder.h"

class KMandatoryFieldGroup;

namespace Ui {
class sepaStandingOrderEdit;
}

/**
 * @brief Widget to edit a SEPA standing order (Dauerauftrag, LH-F-21).
 *
 * Mirrors sepaCreditTransferEdit but adds a recurrence group (interval, cycle,
 * execution day incl. ultimo, first/last execution date). Shown only in
 * SimpleMode; capability-gated on the account's standing-order support.
 */
class sepaStandingOrderEdit : public IonlineJobEdit
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(onlineJob job READ getOnlineJob WRITE setOnlineJob)
    Q_INTERFACES(IonlineJobEdit)

public:
    explicit sepaStandingOrderEdit(QWidget* parent = nullptr, QVariantList args = QVariantList());
    ~sepaStandingOrderEdit();

    onlineJobTyped<sepaStandingOrder> getOnlineJobTyped() const;
    onlineJob getOnlineJob() const final override
    {
        return getOnlineJobTyped();
    }

    QStringList supportedOnlineTasks() const final override
    {
        return QStringList(sepaStandingOrder::name());
    }
    QString label() const
    {
        return i18n("SEPA Standing Order");
    }

    bool isValid() const final override
    {
        // The engine task isValid() stays deterministic; the editor additionally
        // requires bank support and a consistent date range (last not before
        // first), so Send/Enqueue is gated on the conditions the user sees flagged.
        return getOnlineJobTyped().isValid() && supportsStandingOrders() && recurrenceDatesValid();
    }

    bool isReadOnly() const final override
    {
        return m_readOnly;
    }

    void showAllErrorMessages(const bool) final override;
    void showEvent(QShowEvent*) final override;

Q_SIGNALS:
    void readOnlyChanged(bool);

public Q_SLOTS:
    void setOnlineJob(const onlineJobTyped<sepaStandingOrder>& job);
    bool setOnlineJob(const onlineJob& job) final override;
    void setOriginAccount(const QString& accountId) final override;
    void setReadOnly(const bool&);

private Q_SLOTS:
    void updateSettings();
    void updateEveryStatus();
    void periodChanged();
    void beneficiaryIbanChanged(const QString& iban);
    void beneficiaryBicChanged(const QString& bic);
    void beneficiaryNameChanged(const QString& name);
    void valueChanged();
    void recurrenceChanged();

private:
    Ui::sepaStandingOrderEdit* ui;
    onlineJobTyped<sepaStandingOrder> m_onlineJob;
    KMandatoryFieldGroup* m_requiredFields;
    bool m_readOnly;
    bool m_showAllErrors;

    QSharedPointer<const sepaStandingOrder::settings> taskSettings() const;
    bool supportsStandingOrders() const;
    //! @brief First date set and (if given) last date not before first.
    bool recurrenceDatesValid() const;
    //! @brief (Re)fill the execution-day combo for the currently selected interval.
    void populateExecutionDayCombo();
};

#endif // SEPASTANDINGORDEREDIT_H
