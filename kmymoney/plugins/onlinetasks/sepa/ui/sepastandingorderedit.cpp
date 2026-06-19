/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sepastandingorderedit.h"
#include "ui_sepastandingorderedit.h"

#include <QDate>
#include <QStandardItemModel>

#include "kguiutils.h"
#include "kmymoneydateedit.h"

#include "misc/charvalidator.h"
#include "mymoney/mymoneyfile.h"
#include "mymoney/onlinejobadministration.h"
#include "onlinejobtyped.h"
#include "onlinetasks/sepa/sepastandingorder.h"
#include "payeeidentifier/ibanbic/ibanbic.h"
#include "payeeidentifier/payeeidentifiertyped.h"
#include "widgetenums.h"
#include "widgets/payeeidentifier/ibanbic/bicvalidator.h"
#include "widgets/payeeidentifier/ibanbic/ibanvalidator.h"

sepaStandingOrderEdit::sepaStandingOrderEdit(QWidget* parent, QVariantList args)
    : IonlineJobEdit(parent, args)
    , ui(new Ui::sepaStandingOrderEdit)
    , m_onlineJob(onlineJobTyped<sepaStandingOrder>())
    , m_requiredFields(new KMandatoryFieldGroup(this))
    , m_readOnly(false)
    , m_showAllErrors(false)
{
    ui->setupUi(this);

    m_requiredFields->add(ui->beneficiaryIban);
    m_requiredFields->add(ui->value);
    connect(m_requiredFields, &KMandatoryFieldGroup::stateChanged, this, [this](bool) {
        Q_EMIT validityChanged(isValid());
    });

    // Interval selector. Item order matches the Period enum (Monthly=0, Weekly=1).
    ui->period->addItem(i18nc("@item:inlistbox standing order interval", "Monthly"), static_cast<int>(sepaStandingOrder::Period::Monthly));
    ui->period->addItem(i18nc("@item:inlistbox standing order interval", "Weekly"), static_cast<int>(sepaStandingOrder::Period::Weekly));
    populateExecutionDayCombo();

    connect(ui->beneficiaryName, &KLineEdit::textChanged, this, &sepaStandingOrderEdit::beneficiaryNameChanged);
    connect(ui->beneficiaryIban, &KIbanLineEdit::textChanged, this, &sepaStandingOrderEdit::beneficiaryIbanChanged);
    connect(ui->beneficiaryBankCode, &KBicEdit::textChanged, this, &sepaStandingOrderEdit::beneficiaryBicChanged);
    connect(ui->value, &AmountEdit::amountChanged, this, &sepaStandingOrderEdit::valueChanged);
    connect(ui->period, &QComboBox::currentIndexChanged, this, &sepaStandingOrderEdit::periodChanged);
    connect(ui->cycle, &QSpinBox::valueChanged, this, &sepaStandingOrderEdit::recurrenceChanged);
    connect(ui->executionDay, &QComboBox::currentIndexChanged, this, &sepaStandingOrderEdit::recurrenceChanged);
    connect(ui->firstExecutionDate, &KMyMoneyDateEdit::dateChanged, this, &sepaStandingOrderEdit::recurrenceChanged);
    connect(ui->lastExecutionDate, &KMyMoneyDateEdit::dateChanged, this, &sepaStandingOrderEdit::recurrenceChanged);

    connect(qApp, &QApplication::focusChanged, this, &sepaStandingOrderEdit::updateEveryStatus);

    // Read-only wiring.
    connect(this, &sepaStandingOrderEdit::readOnlyChanged, ui->beneficiaryName, &KLineEdit::setReadOnly);
    connect(this, &sepaStandingOrderEdit::readOnlyChanged, ui->beneficiaryIban, &KIbanLineEdit::setReadOnly);
    connect(this, &sepaStandingOrderEdit::readOnlyChanged, ui->beneficiaryBankCode, &KBicEdit::setReadOnly);
    connect(this, &sepaStandingOrderEdit::readOnlyChanged, ui->value, &AmountEdit::setReadOnly);
    connect(this, &sepaStandingOrderEdit::readOnlyChanged, ui->sepaReference, &KLineEdit::setReadOnly);
    connect(this, &sepaStandingOrderEdit::readOnlyChanged, ui->purpose, &KMyMoneyTextEdit::setReadOnly);
    connect(this, &sepaStandingOrderEdit::readOnlyChanged, this, [this](bool readOnly) {
        ui->period->setEnabled(!readOnly);
        ui->cycle->setEnabled(!readOnly);
        ui->executionDay->setEnabled(!readOnly);
        ui->firstExecutionDate->setEnabled(!readOnly);
        ui->lastExecutionDate->setEnabled(!readOnly);
    });

    ui->labelUnsupportedHint->setVisible(false);
}

sepaStandingOrderEdit::~sepaStandingOrderEdit()
{
    // Disconnect field signals before destroying the widgets (the slots
    // dereference ui->..., and widgets can emit during teardown).
    disconnect(ui->beneficiaryName, &KLineEdit::textChanged, this, &sepaStandingOrderEdit::beneficiaryNameChanged);
    disconnect(ui->beneficiaryIban, &KIbanLineEdit::textChanged, this, &sepaStandingOrderEdit::beneficiaryIbanChanged);
    disconnect(ui->beneficiaryBankCode, &KBicEdit::textChanged, this, &sepaStandingOrderEdit::beneficiaryBicChanged);
    disconnect(ui->value, &AmountEdit::amountChanged, this, &sepaStandingOrderEdit::valueChanged);
    disconnect(ui->period, &QComboBox::currentIndexChanged, this, &sepaStandingOrderEdit::periodChanged);
    disconnect(ui->cycle, &QSpinBox::valueChanged, this, &sepaStandingOrderEdit::recurrenceChanged);
    disconnect(ui->executionDay, &QComboBox::currentIndexChanged, this, &sepaStandingOrderEdit::recurrenceChanged);
    disconnect(ui->firstExecutionDate, &KMyMoneyDateEdit::dateChanged, this, &sepaStandingOrderEdit::recurrenceChanged);
    disconnect(ui->lastExecutionDate, &KMyMoneyDateEdit::dateChanged, this, &sepaStandingOrderEdit::recurrenceChanged);
    delete ui;
}

void sepaStandingOrderEdit::showEvent(QShowEvent* event)
{
    updateEveryStatus();
    QWidget::showEvent(event);
}

void sepaStandingOrderEdit::showAllErrorMessages(const bool state)
{
    if (m_showAllErrors != state) {
        m_showAllErrors = state;
        updateEveryStatus();
    }
}

QSharedPointer<const sepaStandingOrder::settings> sepaStandingOrderEdit::taskSettings() const
{
    return onlineJobAdministration::instance()->taskSettings<sepaStandingOrder::settings>(sepaStandingOrder::name(),
                                                                                          m_onlineJob.constTask()->responsibleAccount());
}

bool sepaStandingOrderEdit::supportsStandingOrders() const
{
    const auto settings = taskSettings();
    return !settings.isNull() && settings->supportsStandingOrders();
}

void sepaStandingOrderEdit::populateExecutionDayCombo()
{
    const int previous = ui->executionDay->currentData().isValid() ? ui->executionDay->currentData().toInt() : -1;
    ui->executionDay->clear();

    const bool monthly = (ui->period->currentData().toInt() == static_cast<int>(sepaStandingOrder::Period::Monthly));
    const auto settings = taskSettings();

    if (monthly) {
        ui->labelCycle->setText(i18nc("@label every N months", "Every (months)"));
        const QList<int> advertised = settings.isNull() ? QList<int>() : settings->allowedExecutionDaysMonthly();
        const auto label = [](int day) -> QString {
            switch (day) {
            case 99:
                return i18n("Last day of month");
            case 98:
                return i18n("Second to last day of month");
            case 97:
                return i18n("Third to last day of month");
            default:
                return i18n("Day %1", day);
            }
        };
        if (!advertised.isEmpty()) {
            for (int day : advertised)
                ui->executionDay->addItem(label(day), day);
        } else {
            for (int day = 1; day <= 31; ++day)
                ui->executionDay->addItem(label(day), day);
            ui->executionDay->addItem(label(99), 99);
            ui->executionDay->addItem(label(98), 98);
            ui->executionDay->addItem(label(97), 97);
        }
    } else {
        ui->labelCycle->setText(i18nc("@label every N weeks", "Every (weeks)"));
        static const char* const dayNames[7] = {
            QT_TRANSLATE_NOOP("sepaStandingOrderEdit", "Monday"),
            QT_TRANSLATE_NOOP("sepaStandingOrderEdit", "Tuesday"),
            QT_TRANSLATE_NOOP("sepaStandingOrderEdit", "Wednesday"),
            QT_TRANSLATE_NOOP("sepaStandingOrderEdit", "Thursday"),
            QT_TRANSLATE_NOOP("sepaStandingOrderEdit", "Friday"),
            QT_TRANSLATE_NOOP("sepaStandingOrderEdit", "Saturday"),
            QT_TRANSLATE_NOOP("sepaStandingOrderEdit", "Sunday"),
        };
        const QList<int> advertised = settings.isNull() ? QList<int>() : settings->allowedExecutionDaysWeekly();
        if (!advertised.isEmpty()) {
            for (int day : advertised)
                if (day >= 1 && day <= 7)
                    ui->executionDay->addItem(i18n(dayNames[day - 1]), day);
        } else {
            for (int day = 1; day <= 7; ++day)
                ui->executionDay->addItem(i18n(dayNames[day - 1]), day);
        }
    }

    const int restore = ui->executionDay->findData(previous);
    if (restore != -1)
        ui->executionDay->setCurrentIndex(restore);
}

onlineJobTyped<sepaStandingOrder> sepaStandingOrderEdit::getOnlineJobTyped() const
{
    onlineJobTyped<sepaStandingOrder> job(m_onlineJob);

    job.task()->setAction(sepaStandingOrder::Action::Create);
    job.task()->setValue(ui->value->value());
    job.task()->setPurpose(ui->purpose->toPlainText());
    job.task()->setEndToEndReference(ui->sepaReference->text());

    payeeIdentifiers::ibanBic accIdent;
    accIdent.setOwnerName(ui->beneficiaryName->text());
    accIdent.setIban(ui->beneficiaryIban->text());
    accIdent.setBic(ui->beneficiaryBankCode->text());
    job.task()->setBeneficiary(accIdent);

    job.task()->setPeriod(static_cast<sepaStandingOrder::Period>(ui->period->currentData().toInt()));
    job.task()->setCycle(ui->cycle->value());
    job.task()->setExecutionDay(ui->executionDay->currentData().toInt());
    job.task()->setFirstExecutionDate(ui->firstExecutionDate->date());
    job.task()->setLastExecutionDate(ui->lastExecutionDate->date());

    return job;
}

void sepaStandingOrderEdit::setOnlineJob(const onlineJobTyped<sepaStandingOrder>& job)
{
    m_onlineJob = job;
    updateSettings();
    setReadOnly(!job.isEditable());

    ui->purpose->setText(job.task()->purpose());
    ui->sepaReference->setText(job.task()->endToEndReference());
    ui->value->setValue(job.task()->value());
    ui->beneficiaryName->setText(job.task()->beneficiaryTyped().ownerName());
    ui->beneficiaryIban->setText(job.task()->beneficiaryTyped().paperformatIban());
    ui->beneficiaryBankCode->setText(job.task()->beneficiaryTyped().storedBic());

    ui->period->setCurrentIndex(ui->period->findData(static_cast<int>(job.task()->period())));
    populateExecutionDayCombo();
    if (job.task()->cycle() > 0)
        ui->cycle->setValue(job.task()->cycle());
    const int dayIndex = ui->executionDay->findData(job.task()->executionDay());
    if (dayIndex != -1)
        ui->executionDay->setCurrentIndex(dayIndex);
    ui->firstExecutionDate->setDate(job.task()->firstExecutionDate());
    ui->lastExecutionDate->setDate(job.task()->lastExecutionDate());
}

bool sepaStandingOrderEdit::setOnlineJob(const onlineJob& job)
{
    if (!job.isNull() && job.task()->taskName() == sepaStandingOrder::name()) {
        setOnlineJob(onlineJobTyped<sepaStandingOrder>(job));
        return true;
    }
    return false;
}

void sepaStandingOrderEdit::setOriginAccount(const QString& accountId)
{
    m_onlineJob.task()->setOriginAccount(accountId);
    updateSettings();
}

void sepaStandingOrderEdit::updateEveryStatus()
{
    beneficiaryNameChanged(ui->beneficiaryName->text());
    beneficiaryIbanChanged(ui->beneficiaryIban->text());
    beneficiaryBicChanged(ui->beneficiaryBankCode->text());
    valueChanged();
    recurrenceChanged();
}

void sepaStandingOrderEdit::setReadOnly(const bool& readOnly)
{
    if (readOnly != m_readOnly && (readOnly == true || getOnlineJobTyped().isEditable())) {
        m_readOnly = readOnly;
        Q_EMIT readOnlyChanged(m_readOnly);
    }
}

void sepaStandingOrderEdit::updateSettings()
{
    const auto settings = taskSettings();
    const bool supported = !settings.isNull() && settings->supportsStandingOrders();

    // Capability gate (LH-F-20 disabled-with-hint pattern): when the bank/backend
    // does not offer standing orders for this account, disable the inputs and say so.
    ui->labelUnsupportedHint->setVisible(!supported);
    const QList<QWidget*>
        inputs{ui->beneficiaryName, ui->beneficiaryIban, ui->beneficiaryBankCode, ui->value, ui->sepaReference, ui->purpose, ui->recurrenceGroup};
    for (QWidget* w : inputs)
        w->setEnabled(supported);

    // Enable the interval items the bank advertises (default permissive).
    if (auto* model = qobject_cast<QStandardItemModel*>(ui->period->model())) {
        if (auto* monthly = model->item(static_cast<int>(sepaStandingOrder::Period::Monthly)))
            monthly->setEnabled(settings.isNull() || settings->allowMonthly());
        if (auto* weekly = model->item(static_cast<int>(sepaStandingOrder::Period::Weekly)))
            weekly->setEnabled(settings.isNull() || settings->allowWeekly());
    }

    populateExecutionDayCombo();
    updateEveryStatus();
}

void sepaStandingOrderEdit::periodChanged()
{
    populateExecutionDayCombo();
    recurrenceChanged();
}

void sepaStandingOrderEdit::beneficiaryIbanChanged(const QString& iban)
{
    const QPair<eWidgets::ValidationFeedback::MessageType, QString> answer = ibanValidator::validateWithMessage(iban);
    if (m_showAllErrors || iban.length() > 5 || (!ui->beneficiaryIban->hasFocus() && !iban.isEmpty()))
        ui->feedbackIban->setFeedback(answer.first, answer.second);
    else
        ui->feedbackIban->removeFeedback();
    Q_EMIT validityChanged(isValid());
}

void sepaStandingOrderEdit::beneficiaryBicChanged(const QString& bic)
{
    const QPair<eWidgets::ValidationFeedback::MessageType, QString> answer = bicValidator::validateWithMessage(bic);
    if (m_showAllErrors || bic.length() >= 8 || (!ui->beneficiaryBankCode->hasFocus() && !bic.isEmpty()))
        ui->feedbackBic->setFeedback(answer.first, answer.second);
    else
        ui->feedbackBic->removeFeedback();
}

void sepaStandingOrderEdit::beneficiaryNameChanged(const QString& name)
{
    if (name.isEmpty() && (m_showAllErrors || !ui->beneficiaryName->hasFocus()))
        ui->feedbackName->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, i18n("A beneficiary name is needed."));
    else
        ui->feedbackName->removeFeedback();
}

void sepaStandingOrderEdit::valueChanged()
{
    if (!ui->value->isValid() && (m_showAllErrors || (!ui->value->hasFocus() && ui->value->value().toDouble() != 0))) {
        ui->feedbackAmount->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, i18n("A positive amount to transfer is needed."));
        return;
    }
    if (ui->value->value().isNegative())
        ui->feedbackAmount->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, i18n("A positive amount to transfer is needed."));
    else
        ui->feedbackAmount->removeFeedback();
    Q_EMIT validityChanged(isValid());
}

void sepaStandingOrderEdit::recurrenceChanged()
{
    QString message;
    if (!ui->firstExecutionDate->date().isValid())
        message = i18n("A first execution date is needed.");
    else if (ui->lastExecutionDate->date().isValid() && ui->lastExecutionDate->date() < ui->firstExecutionDate->date())
        message = i18n("The last execution date must not be before the first.");

    if (!message.isEmpty() && (m_showAllErrors || !ui->firstExecutionDate->hasFocus()))
        ui->feedbackRecurrence->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, message);
    else
        ui->feedbackRecurrence->removeFeedback();

    Q_EMIT validityChanged(isValid());
}
