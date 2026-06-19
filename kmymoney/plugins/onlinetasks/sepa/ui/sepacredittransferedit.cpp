/*
    SPDX-FileCopyrightText: 2013 Christian Dávid <christian-david@web.de>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sepacredittransferedit.h"
#include "ui_sepacredittransferedit.h"

#include <QCompleter>
#include <QDate>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>

#include <KDescendantsProxyModel>

#include "kguiutils.h"
#include "kmymoneydateedit.h"

#include "misc/charvalidator.h"
#include "mymoney/mymoneyfile.h"
#include "mymoney/payeeidentifiermodel.h"
#include "mymoneyaccount.h"
#include "onlinejobtyped.h"
#include "onlinetasks/sepa/sepaonlinetransfer.h"
#include "payeeidentifier/ibanbic/ibanbic.h"
#include "payeeidentifier/payeeidentifiertyped.h"
#include "styleditemdelegateforwarder.h"
#include "widgetenums.h"
#include "widgets/payeeidentifier/ibanbic/bicvalidator.h"
#include "widgets/payeeidentifier/ibanbic/ibanbicitemdelegate.h"
#include "widgets/payeeidentifier/ibanbic/ibanvalidator.h"

class ibanBicCompleterDelegate : public StyledItemDelegateForwarder
{
    Q_OBJECT

public:
    ibanBicCompleterDelegate(QObject *parent)
        : StyledItemDelegateForwarder(parent) {}

protected:
    QAbstractItemDelegate* getItemDelegate(const QModelIndex &index) const final override {
        static QPointer<QAbstractItemDelegate> defaultDelegate;
        static QPointer<QAbstractItemDelegate> ibanBicDelegate;

        const bool ibanBicRequested = index.model()->data(index, payeeIdentifierModel::isPayeeIdentifier).toBool();

        QAbstractItemDelegate* delegate = (ibanBicRequested)
                                          ? ibanBicDelegate
                                          : defaultDelegate;

        if (delegate == nullptr) {
            if (ibanBicRequested) {
                // Use this->parent() as parent because "this" is const
                ibanBicDelegate = new ibanBicItemDelegate(this->parent());
                delegate = ibanBicDelegate;
            } else {
                // Use this->parent() as parent because "this" is const
                defaultDelegate = new QStyledItemDelegate(this->parent());
                delegate = defaultDelegate;
            }
            connectSignals(delegate, Qt::UniqueConnection);
        }
        Q_ASSERT(delegate);
        return delegate;
    }
};

class payeeIdentifierCompleterPopup : public QTreeView
{
    Q_OBJECT

public:
    payeeIdentifierCompleterPopup(QWidget* parent = nullptr)
        : QTreeView(parent)
    {
        setRootIsDecorated(false);
        setAlternatingRowColors(true);
        setAnimated(true);
        setHeaderHidden(true);
        setUniformRowHeights(false);
        expandAll();
    }
};

class ibanBicFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum roles {
        payeeIban = payeeIdentifierModel::payeeIdentifierUserRole, /**< electornic IBAN of payee */
    };

    ibanBicFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

    QVariant data(const QModelIndex &index, int role) const final override {
        if (role == payeeIban) {
            if (!index.isValid())
                return QVariant();

            try {
                payeeIdentifierTyped<payeeIdentifiers::ibanBic> iban = payeeIdentifierTyped<payeeIdentifiers::ibanBic>(
                            index.model()->data(index, payeeIdentifierModel::payeeIdentifier).value<payeeIdentifier>()
                        );
                return iban->electronicIban();
            } catch (const payeeIdentifier::empty &) {
                return QVariant();
            } catch (const payeeIdentifier::badCast &) {
                return QVariant();
            }
        }

        return QSortFilterProxyModel::data(index, role);
    }

    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const final override {
        if (!source_parent.isValid())
            return true;

        QModelIndex index = source_parent.model()->index(source_row, 0, source_parent);
        return (source_parent.model()->data(index, payeeIdentifierModel::payeeIdentifierType).toString() == payeeIdentifiers::ibanBic::staticPayeeIdentifierIid());
    }
};

class ibanBicCompleter : public QCompleter
{
    Q_OBJECT

public:
    ibanBicCompleter(QObject* parent = nullptr);

Q_SIGNALS:
    void activatedName(const QString& name) const;
    void highlightedName(const QString& name) const;

    void activatedBic(const QString& bic) const;
    void highlightedBic(const QString& bic) const;

    void activatedIban(const QString& iban) const;
    void highlightedIban(const QString& iban) const;

private Q_SLOTS:
    void slotActivated(const QModelIndex& index) const;
    void slotHighlighted(const QModelIndex& index) const;
};

ibanBicCompleter::ibanBicCompleter(QObject *parent)
    : QCompleter(parent)
{
    connect(this, qOverload<const QModelIndex&>(&QCompleter::activated), this, &ibanBicCompleter::slotActivated);
    connect(this, qOverload<const QModelIndex&>(&QCompleter::highlighted), this, &ibanBicCompleter::slotHighlighted);
}

void ibanBicCompleter::slotActivated(const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    Q_EMIT activatedName(index.model()->data(index, payeeIdentifierModel::payeeName).toString());
    try {
        payeeIdentifierTyped<payeeIdentifiers::ibanBic> iban = payeeIdentifierTyped<payeeIdentifiers::ibanBic>(
                    index.model()->data(index, payeeIdentifierModel::payeeIdentifier).value<payeeIdentifier>()
                );
        Q_EMIT activatedIban(iban->electronicIban());
        Q_EMIT activatedBic(iban->storedBic());
    } catch (const payeeIdentifier::empty &) {
    } catch (const payeeIdentifier::badCast &) {
    }
}

void ibanBicCompleter::slotHighlighted(const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    Q_EMIT highlightedName(index.model()->data(index, payeeIdentifierModel::payeeName).toString());
    try {
        payeeIdentifierTyped<payeeIdentifiers::ibanBic> iban = payeeIdentifierTyped<payeeIdentifiers::ibanBic>(
                    index.model()->data(index, payeeIdentifierModel::payeeIdentifier).value<payeeIdentifier>()
                );
        Q_EMIT highlightedIban(iban->electronicIban());
        Q_EMIT highlightedBic(iban->storedBic());
    } catch (const payeeIdentifier::empty &) {
    } catch (const payeeIdentifier::badCast &) {
    }
}

sepaCreditTransferEdit::sepaCreditTransferEdit(QWidget *parent, QVariantList args) :
    IonlineJobEdit(parent, args),
    ui(new Ui::sepaCreditTransferEdit),
    m_onlineJob(onlineJobTyped<sepaOnlineTransfer>()),
    m_requiredFields(new KMandatoryFieldGroup(this)),
    m_readOnly(false),
    m_showAllErrors(false)
{
    ui->setupUi(this);

    m_requiredFields->add(ui->beneficiaryIban);
    m_requiredFields->add(ui->value);
    // Other required fields are set in updateSettings()

    connect(m_requiredFields, &KMandatoryFieldGroup::stateChanged, this, &sepaCreditTransferEdit::requiredFieldsCompleted);

    // make sure to disconnect the textChanged() signals manually in our dtor
    // before we destruct the widgets
    connect(ui->beneficiaryName, &KLineEdit::textChanged, this, &sepaCreditTransferEdit::beneficiaryNameChanged);
    connect(ui->beneficiaryIban, &KIbanLineEdit::textChanged, this, &sepaCreditTransferEdit::beneficiaryIbanChanged);
    connect(ui->beneficiaryBankCode, &KBicEdit::textChanged, this, &sepaCreditTransferEdit::beneficiaryBicChanged);
    connect(ui->value, &AmountEdit::amountChanged, this, &sepaCreditTransferEdit::valueChanged);
    connect(ui->sepaReference, &KLineEdit::textChanged, this, &sepaCreditTransferEdit::endToEndReferenceChanged);
    connect(ui->purpose, &KMyMoneyTextEdit::textChanged, this, &sepaCreditTransferEdit::purposeChanged);

    connect(qApp, &QApplication::focusChanged, this, &sepaCreditTransferEdit::updateEveryStatus);

    // Connect signals for read only
    connect(this, &sepaCreditTransferEdit::readOnlyChanged, ui->beneficiaryName, &KLineEdit::setReadOnly);
    connect(this, &sepaCreditTransferEdit::readOnlyChanged, ui->beneficiaryIban, &KIbanLineEdit::setReadOnly);
    connect(this, &sepaCreditTransferEdit::readOnlyChanged, ui->beneficiaryBankCode, &KBicEdit::setReadOnly);
    connect(this, &sepaCreditTransferEdit::readOnlyChanged, ui->value, &AmountEdit::setReadOnly);
    connect(this, &sepaCreditTransferEdit::readOnlyChanged, ui->sepaReference, &KLineEdit::setReadOnly);
    connect(this, &sepaCreditTransferEdit::readOnlyChanged, ui->purpose, &KMyMoneyTextEdit::setReadOnly);

    // LH-F-20: transfer-type selector. The item order matches the
    // sepaOnlineTransfer::TransferType enum values (Standard=0, Instant=1,
    // Dated=2), so index and enum value are interchangeable.
    ui->transferType->addItem(i18nc("@item:inlistbox transfer type", "Standard credit transfer"));
    ui->transferType->addItem(i18nc("@item:inlistbox transfer type", "Instant transfer (SEPA Instant)"));
    ui->transferType->addItem(i18nc("@item:inlistbox transfer type", "Dated transfer (executed by your bank on the given date)"));
    connect(ui->transferType, &QComboBox::currentIndexChanged, this, &sepaCreditTransferEdit::transferTypeChanged);
    connect(ui->executionDate, &KMyMoneyDateEdit::dateChanged, this, &sepaCreditTransferEdit::executionDateChanged);
    // Combos have no setReadOnly; disable them instead when the job is read only.
    connect(this, &sepaCreditTransferEdit::readOnlyChanged, this, [this](bool readOnly) {
        ui->transferType->setEnabled(!readOnly);
        ui->executionDate->setEnabled(!readOnly);
    });
    // Hidden until updateSettings()/transferTypeChanged() reveal them based on
    // SimpleMode + the current selection (full mode keeps the dialog unchanged).
    ui->labelTransferType->setVisible(false);
    ui->transferType->setVisible(false);
    ui->labelExecutionDate->setVisible(false);
    ui->executionDate->setVisible(false);
    ui->feedbackExecutionDate->setVisible(false);
    ui->labelDatedCompatHint->setVisible(false);

    // Create models for completers
    payeeIdentifierModel* identModel = new payeeIdentifierModel(this);
    identModel->setTypeFilter(payeeIdentifiers::ibanBic::staticPayeeIdentifierIid());

    ibanBicFilterProxyModel* filterModel = new ibanBicFilterProxyModel(this);
    filterModel->setSourceModel(identModel);

    KDescendantsProxyModel* descendantsModel = new KDescendantsProxyModel(this);
    descendantsModel->setSourceModel(filterModel);

    // Set completers popup and bind them to the corresponding fields
    {
        // Beneficiary name field
        ibanBicCompleter* completer = new ibanBicCompleter(this);
        completer->setModel(descendantsModel);
        completer->setCompletionRole(payeeIdentifierModel::payeeName);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCaseSensitivity(Qt::CaseInsensitive);

        connect(completer, &ibanBicCompleter::activatedIban, ui->beneficiaryIban, &KIbanLineEdit::setText);
        connect(completer, &ibanBicCompleter::activatedBic, ui->beneficiaryBankCode, &KBicEdit::setText);

        ui->beneficiaryName->setCompleter(completer);

        QAbstractItemView *itemView = new payeeIdentifierCompleterPopup();
        completer->setPopup(itemView);

        // setPopup() resets the delegate
        itemView->setItemDelegate(new ibanBicCompleterDelegate(this));
    }
    {
        // IBAN field
        ibanBicCompleter* ibanCompleter = new ibanBicCompleter(this);
        ibanCompleter->setModel(descendantsModel);
        ibanCompleter->setCompletionRole(ibanBicFilterProxyModel::payeeIban);
        ibanCompleter->setCaseSensitivity(Qt::CaseInsensitive);

        connect(ibanCompleter, &ibanBicCompleter::activatedName, ui->beneficiaryName, &KLineEdit::setText);
        connect(ibanCompleter, &ibanBicCompleter::activatedBic, ui->beneficiaryBankCode, &KBicEdit::setText);

        ui->beneficiaryIban->setCompleter(ibanCompleter);

        QAbstractItemView *itemView = new payeeIdentifierCompleterPopup();
        ibanCompleter->setPopup(itemView);

        // setPopup() resets the delegate
        itemView->setItemDelegate(new ibanBicCompleterDelegate(this));
    }
}

sepaCreditTransferEdit::~sepaCreditTransferEdit()
{
    // disconnect the signals before the widgets are destroyed
    // because they could still emit signals as part of their
    // destruction which may bite us back in this object.
    // See https://discuss.kde.org/t/kmymoney-5-2-crash-after-saving-bank-transfer-via-aqbanking/38517/3
    // for an example
    disconnect(ui->beneficiaryName, &KLineEdit::textChanged, this, &sepaCreditTransferEdit::beneficiaryNameChanged);
    disconnect(ui->beneficiaryIban, &KIbanLineEdit::textChanged, this, &sepaCreditTransferEdit::beneficiaryIbanChanged);
    disconnect(ui->beneficiaryBankCode, &KBicEdit::textChanged, this, &sepaCreditTransferEdit::beneficiaryBicChanged);
    disconnect(ui->value, &AmountEdit::amountChanged, this, &sepaCreditTransferEdit::valueChanged);
    disconnect(ui->sepaReference, &KLineEdit::textChanged, this, &sepaCreditTransferEdit::endToEndReferenceChanged);
    disconnect(ui->purpose, &KMyMoneyTextEdit::textChanged, this, &sepaCreditTransferEdit::purposeChanged);
    // LH-F-20 widgets: same teardown precaution — their slots dereference ui->...
    disconnect(ui->transferType, &QComboBox::currentIndexChanged, this, &sepaCreditTransferEdit::transferTypeChanged);
    disconnect(ui->executionDate, &KMyMoneyDateEdit::dateChanged, this, &sepaCreditTransferEdit::executionDateChanged);
    delete ui;
}

void sepaCreditTransferEdit::showEvent(QShowEvent* event)
{
    updateEveryStatus();
    QWidget::showEvent(event);
}

void sepaCreditTransferEdit::showAllErrorMessages(const bool state)
{
    if (m_showAllErrors != state) {
        m_showAllErrors = state;
        updateEveryStatus();
    }
}

onlineJobTyped<sepaOnlineTransfer> sepaCreditTransferEdit::getOnlineJobTyped() const
{
    onlineJobTyped<sepaOnlineTransfer> sepaJob(m_onlineJob);

    sepaJob.task()->setValue(ui->value->value());
    sepaJob.task()->setPurpose(ui->purpose->toPlainText());
    sepaJob.task()->setEndToEndReference(ui->sepaReference->text());

    // LH-F-20: transfer type + execution date. The date is only meaningful for a
    // dated transfer; for any other type we store an invalid date.
    const auto transferType = static_cast<sepaOnlineTransfer::TransferType>(ui->transferType->currentIndex());
    sepaJob.task()->setTransferType(transferType);
    sepaJob.task()->setExecutionDate((transferType == sepaOnlineTransfer::TransferType::Dated) ? ui->executionDate->date() : QDate());

    payeeIdentifiers::ibanBic accIdent;
    accIdent.setOwnerName(ui->beneficiaryName->text());
    accIdent.setIban(ui->beneficiaryIban->text());
    accIdent.setBic(ui->beneficiaryBankCode->text());
    sepaJob.task()->setBeneficiary(accIdent);

    return sepaJob;
}

void sepaCreditTransferEdit::setOnlineJob(const onlineJobTyped<sepaOnlineTransfer>& job)
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

    // LH-F-20: restore transfer type + execution date (set the date first so the
    // validation triggered by the index change sees it). setCurrentIndex emits
    // currentIndexChanged -> transferTypeChanged(), which updates the row visibility.
    ui->executionDate->setDate(job.task()->executionDate());
    ui->transferType->setCurrentIndex(static_cast<int>(job.task()->transferType()));
    transferTypeChanged();
}

bool sepaCreditTransferEdit::setOnlineJob(const onlineJob& job)
{
    if (!job.isNull() && job.task()->taskName() == sepaOnlineTransfer::name()) {
        setOnlineJob(onlineJobTyped<sepaOnlineTransfer>(job));
        return true;
    }
    return false;
}

void sepaCreditTransferEdit::setOriginAccount(const QString& accountId)
{
    m_onlineJob.task()->setOriginAccount(accountId);
    updateSettings();
}

void sepaCreditTransferEdit::updateEveryStatus()
{
    beneficiaryNameChanged(ui->beneficiaryName->text());
    beneficiaryIbanChanged(ui->beneficiaryIban->text());
    beneficiaryBicChanged(ui->beneficiaryBankCode->text());
    purposeChanged();
    valueChanged();
    endToEndReferenceChanged(ui->sepaReference->text());
}

void sepaCreditTransferEdit::setReadOnly(const bool& readOnly)
{
    // Only set writeable if it changes something and if it is possible
    if (readOnly != m_readOnly && (readOnly == true || getOnlineJobTyped().isEditable())) {
        m_readOnly = readOnly;
        Q_EMIT readOnlyChanged(m_readOnly);
    }
}

void sepaCreditTransferEdit::updateSettings()
{
    QSharedPointer<const sepaOnlineTransfer::settings> settings = taskSettings();
    // Reference
    ui->sepaReference->setMaxLength(settings->endToEndReferenceLength());
    if (settings->endToEndReferenceLength() == 0)
        ui->sepaReference->setEnabled(false);
    else
        ui->sepaReference->setEnabled(true);

    // Purpose
    ui->purpose->setAllowedChars(settings->allowedChars());
    ui->purpose->setMaxLineLength(settings->purposeLineLength());
    ui->purpose->setMaxLines(settings->purposeMaxLines());
    if (settings->purposeMinLength())
        m_requiredFields->add(ui->purpose);
    else
        m_requiredFields->remove(ui->purpose);

    // Beneficiary Name
    ui->beneficiaryName->setValidator(new charValidator(ui->beneficiaryName, settings->allowedChars()));
    ui->beneficiaryName->setMaxLength(settings->recipientNameLineLength());

    if (settings->recipientNameMinLength() != 0)
        m_requiredFields->add(ui->beneficiaryName);
    else
        m_requiredFields->remove(ui->beneficiaryName);

    // LH-F-20: the transfer-type selector is shown only in SimpleMode, so in full
    // mode the dialog is pixel-identical to upstream and the task stays Standard.
    const bool simpleMode = MyMoneyFile::instance()->simpleMode();
    ui->labelTransferType->setVisible(simpleMode);
    ui->transferType->setVisible(simpleMode);

    // Enable the Instant/Dated items only when the backend advertises the
    // capability; the tooltips name AqBanking as the reason when unavailable.
    if (auto* model = qobject_cast<QStandardItemModel*>(ui->transferType->model())) {
        if (auto* instantItem = model->item(static_cast<int>(sepaOnlineTransfer::TransferType::Instant))) {
            instantItem->setEnabled(settings->supportsInstantTransfer());
            instantItem->setToolTip(settings->supportsInstantTransfer() ? QString() : i18n("Not supported by the installed banking backend (AqBanking)."));
        }
        if (auto* datedItem = model->item(static_cast<int>(sepaOnlineTransfer::TransferType::Dated))) {
            datedItem->setEnabled(settings->supportsDatedTransfer());
            datedItem->setToolTip(settings->supportsDatedTransfer()
                                      ? QString()
                                      : i18n("The banking backend (AqBanking) or your bank does not offer dated transfers for this account."));
        }
    }

    // Re-evaluate the dated-transfer row visibility for the current selection.
    transferTypeChanged();

    updateEveryStatus();
}

void sepaCreditTransferEdit::transferTypeChanged()
{
    // The execution-date row + compatibility hint appear only for a dated transfer
    // in SimpleMode (in full mode the selector is hidden and the type is Standard).
    const bool simpleMode = MyMoneyFile::instance()->simpleMode();
    const bool dated = simpleMode && (ui->transferType->currentIndex() == static_cast<int>(sepaOnlineTransfer::TransferType::Dated));
    ui->labelExecutionDate->setVisible(dated);
    ui->executionDate->setVisible(dated);
    ui->feedbackExecutionDate->setVisible(dated);
    ui->labelDatedCompatHint->setVisible(dated);

    executionDateChanged();
    Q_EMIT validityChanged(isValid());
}

bool sepaCreditTransferEdit::executionDateWithinLimits() const
{
    if (ui->transferType->currentIndex() != static_cast<int>(sepaOnlineTransfer::TransferType::Dated))
        return true;

    const auto settings = getOnlineJobTyped().constTask()->getSettings();
    const QDate date = ui->executionDate->date();
    const int minLead = qMax(1, settings->minDatedTransferLeadDays());
    const int maxLead = settings->maxDatedTransferLeadDays();

    if (!date.isValid() || date < QDate::currentDate().addDays(minLead))
        return false;
    if (maxLead > 0 && date > QDate::currentDate().addDays(maxLead))
        return false;
    return true;
}

void sepaCreditTransferEdit::executionDateChanged()
{
    // Only meaningful for a dated transfer; the date widget is hidden otherwise.
    if (ui->transferType->currentIndex() != static_cast<int>(sepaOnlineTransfer::TransferType::Dated)) {
        ui->feedbackExecutionDate->removeFeedback();
        Q_EMIT validityChanged(isValid());
        return;
    }

    QSharedPointer<const sepaOnlineTransfer::settings> settings = taskSettings();
    const QDate date = ui->executionDate->date();
    const int minLead = qMax(1, settings->minDatedTransferLeadDays());
    const int maxLead = settings->maxDatedTransferLeadDays();

    // Lead-time check in calendar days (an approximation of the FinTS bank-day
    // setup time; kbanking and the bank are the authoritative backstop). The
    // outcome also gates Send/Enqueue through isValid()/executionDateWithinLimits().
    if (!date.isValid() || date < QDate::currentDate().addDays(minLead)) {
        ui->feedbackExecutionDate->setFeedback(
            eWidgets::ValidationFeedback::MessageType::Error,
            i18np("The execution date must be at least one day in the future.", "The execution date must be at least %1 days in the future.", minLead));
    } else if (maxLead > 0 && date > QDate::currentDate().addDays(maxLead)) {
        ui->feedbackExecutionDate->setFeedback(
            eWidgets::ValidationFeedback::MessageType::Error,
            i18np("The execution date must be at most one day in the future.", "The execution date must be at most %1 days in the future.", maxLead));
    } else {
        ui->feedbackExecutionDate->removeFeedback();
    }

    Q_EMIT validityChanged(isValid());
}

void sepaCreditTransferEdit::beneficiaryIbanChanged(const QString& iban)
{
    // Check IBAN
    QPair<eWidgets::ValidationFeedback::MessageType, QString> answer = ibanValidator::validateWithMessage(iban);
    if (m_showAllErrors || iban.length() > 5 || (!ui->beneficiaryIban->hasFocus() && !iban.isEmpty()))
        ui->feedbackIban->setFeedback(answer.first, answer.second);
    else
        ui->feedbackIban->removeFeedback();

    // Check if BIC is mandatory
    QSharedPointer<const sepaOnlineTransfer::settings> settings = taskSettings();

    QString payeeIban;
    try {
        payeeIdentifier ident = getOnlineJobTyped().task()->originAccountIdentifier();
        payeeIban = ident.data<payeeIdentifiers::ibanBic>()->electronicIban();
    } catch (const payeeIdentifier::empty &) {
    } catch (const payeeIdentifier::badCast &) {
    }

    if (settings->isBicMandatory(payeeIban, iban)) {
        m_requiredFields->add(ui->beneficiaryBankCode);
        beneficiaryBicChanged(ui->beneficiaryBankCode->text());
    } else {
        m_requiredFields->remove(ui->beneficiaryBankCode);
        beneficiaryBicChanged(ui->beneficiaryBankCode->text());
    }
}

void sepaCreditTransferEdit::beneficiaryBicChanged(const QString& bic)
{
    if (bic.isEmpty() && !ui->beneficiaryIban->text().isEmpty()) {
        QSharedPointer<const sepaOnlineTransfer::settings> settings = taskSettings();

        const payeeIdentifier payee = getOnlineJobTyped().task()->originAccountIdentifier();
        QString iban;
        try {
            iban = payee.data<payeeIdentifiers::ibanBic>()->electronicIban();
        } catch (const payeeIdentifier::badCast &) {
        }

        if (settings->isBicMandatory(iban, ui->beneficiaryIban->text())) {
            ui->feedbackBic->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, i18n("For this beneficiary's country the BIC is mandatory."));
            return;
        }
    }

    QPair<eWidgets::ValidationFeedback::MessageType, QString> answer = bicValidator::validateWithMessage(bic);
    if (m_showAllErrors || bic.length() >= 8 || (!ui->beneficiaryBankCode->hasFocus() && !bic.isEmpty()))
        ui->feedbackBic->setFeedback(answer.first, answer.second);
    else
        ui->feedbackBic->removeFeedback();
}

void sepaCreditTransferEdit::beneficiaryNameChanged(const QString& name)
{
    QSharedPointer<const sepaOnlineTransfer::settings> settings = taskSettings();
    if (name.length() < settings->recipientNameMinLength() && (m_showAllErrors || (!ui->beneficiaryName->hasFocus() && !name.isEmpty()))) {
        ui->feedbackName->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, i18np("A beneficiary name is needed.", "The beneficiary name must be at least %1 characters long",
                                      settings->recipientNameMinLength()
                                                                                             ));
    } else {
        ui->feedbackName->removeFeedback();
    }
}

void sepaCreditTransferEdit::valueChanged()
{
    if ((!ui->value->isValid() && (m_showAllErrors || (!ui->value->hasFocus() && ui->value->value().toDouble() != 0))) || (!ui->value->value().isPositive() && ui->value->value().toDouble() != 0)) {
        ui->feedbackAmount->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, i18n("A positive amount to transfer is needed."));
        return;
    }

    if (!ui->value->isValid())
        return;

    const MyMoneyAccount account = getOnlineJob().responsibleMyMoneyAccount();
    const MyMoneyMoney expectedBalance = account.balance() - ui->value->value();

    if (expectedBalance < MyMoneyMoney(account.value("maxCreditAbsolute"))) {
        ui->feedbackAmount->setFeedback(eWidgets::ValidationFeedback::MessageType::Warning, i18n("After this credit transfer the account's balance will be below your credit limit."));
    } else if (expectedBalance < MyMoneyMoney(account.value("minBalanceAbsolute"))) {
        ui->feedbackAmount->setFeedback(eWidgets::ValidationFeedback::MessageType::Information, i18n("After this credit transfer the account's balance will be below the minimal balance."));
    } else {
        ui->feedbackAmount->removeFeedback();
    }
}

void sepaCreditTransferEdit::endToEndReferenceChanged(const QString& reference)
{
    QSharedPointer<const sepaOnlineTransfer::settings> settings = taskSettings();
    if (settings->checkEndToEndReferenceLength(reference) == validators::tooLong) {
        ui->feedbackReference->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, i18np("The end-to-end reference cannot contain more than one character.",
                                           "The end-to-end reference cannot contain more than %1 characters.",
                                           settings->endToEndReferenceLength()
                                                                                                  ));
    } else {
        ui->feedbackReference->removeFeedback();
    }
}

void sepaCreditTransferEdit::purposeChanged()
{
    const QString purpose = ui->purpose->toPlainText();
    QSharedPointer<const sepaOnlineTransfer::settings> settings = taskSettings();

    QString message;
    if (!settings->checkPurposeLineLength(purpose))
        message = i18np("The maximal line length of %1 character per line is exceeded.", "The maximal line length of %1 characters per line is exceeded.",
                        settings->purposeLineLength())
                  .append('\n');
    if (!settings->checkPurposeCharset(purpose))
        message.append(i18n("The purpose can only contain the letters A-Z, spaces and ':?.,-()+ and /")).append('\n');
    if (!settings->checkPurposeMaxLines(purpose)) {
        message.append(i18np("In the purpose only a single line is allowed.", "The purpose cannot contain more than %1 lines.",
                             settings->purposeMaxLines()))
        .append('\n');
    } else if (settings->checkPurposeLength(purpose) == validators::tooShort) {
        message.append(i18np("A purpose is needed.", "The purpose must be at least %1 characters long.", settings->purposeMinLength()))
        .append('\n');
    }

    // Remove the last '\n'
    message.chop(1);

    if (!message.isEmpty()) {
        ui->feedbackPurpose->setFeedback(eWidgets::ValidationFeedback::MessageType::Error, message);
    } else {
        ui->feedbackPurpose->removeFeedback();
    }
}

QSharedPointer< const sepaOnlineTransfer::settings > sepaCreditTransferEdit::taskSettings()
{
    return getOnlineJobTyped().constTask()->getSettings();
}

#include "sepacredittransferedit.moc"
