/*
    SPDX-FileCopyrightText: 2021 Thomas Baumgart <tbaumgart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksearchtransactiondlg.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QAction>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KGuiItem>
#include <KLocalizedString>
#include <KStandardGuiItem>

// ----------------------------------------------------------------------------
// Project Includes

#include "journalmodel.h"
#include "kmymoneysettings.h"
#include "ktransactionfilter.h"
#include "ledgerjournalidfilter.h"
#include "ledgersearchfilter.h"
#include "ledgerview.h"
#include "menuenums.h"
#include "mymoneyfile.h"
#include "mymoneytransactionfilter.h"
#include "mymoneyutils.h"
#include "selectedobjects.h"
#include "specialdatesmodel.h"
#include "ui_ksearchtransactiondlg.h"

class KSearchTransactionDlgPrivate
{
    Q_DISABLE_COPY(KSearchTransactionDlgPrivate)
    Q_DECLARE_PUBLIC(KSearchTransactionDlg)

public:
    KSearchTransactionDlgPrivate(KSearchTransactionDlg* qq)
        : q_ptr(qq)
        , filterModel(new LedgerJournalIdFilter(qq, QVector<QAbstractItemModel*>{/*MyMoneyFile::instance()->specialDatesModel()*/}))
    {
    }

    void init()
    {
        Q_Q(KSearchTransactionDlg);

        ui.setupUi(q);
        filterTab = new KTransactionFilter(q);
        ui.m_tabWidget->insertTab(0, filterTab, i18nc("Criteria tab", "Criteria"));

        // disable access to result page until we have a result
        ui.m_tabWidget->setTabEnabled(ui.m_tabWidget->indexOf(ui.m_resultPage), false);

        // only allow searches when a selection has been made
        ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        ui.buttonBox->button(QDialogButtonBox::Apply)->setDefault(true);
        ui.buttonBox->button(QDialogButtonBox::Apply)->setAutoDefault(true);
        KGuiItem::assign(ui.buttonBox->button(QDialogButtonBox::Apply), KStandardGuiItem::find());
        ui.buttonBox->button(QDialogButtonBox::Apply)->setToolTip(i18nc("@info:tooltip for find transaction apply button", "Search transactions"));
        q->connect(filterTab, &KTransactionFilter::selectionNotEmpty, ui.buttonBox->button(QDialogButtonBox::Apply), &QWidget::setEnabled);

        // In the simplified mode an additional instant search tab covering
        // the transactions of all accounts is shown in front of the
        // criteria based search (LH-F-19). Created at runtime only, so
        // that the dialog stays untouched when the mode is off.
        if (KMyMoneySettings::simpleMode()) {
            searchPage = new QWidget(q);
            auto layout = new QVBoxLayout(searchPage);
            // match the .ui layoutdefault of the sibling tab pages
            layout->setContentsMargins(11, 11, 11, 11);
            searchLineEdit = new QLineEdit(searchPage);
            searchLineEdit->setPlaceholderText(i18nc("@info:placeholder", "Search all transactions"));
            searchLineEdit->setToolTip(i18nc("@info:tooltip", "Show transactions of all accounts matching the entered text"));
            searchLedgerView = new LedgerView(searchPage);
            searchFoundText = new QLabel(searchPage);
            layout->addWidget(searchLineEdit);
            layout->addWidget(searchLedgerView);
            layout->addWidget(searchFoundText);
            ui.m_tabWidget->insertTab(0, searchPage, i18nc("@title:tab transaction search", "Search"));
            ui.m_tabWidget->setCurrentIndex(0);

            searchFilter = new LedgerSearchFilter(q);
            searchFilter->attachLineEdit(searchLineEdit);
            // separate persisted column layout, like the simplified split editor
            searchLedgerView->setColumnSelectorGroupName(QLatin1String("GlobalSearchSimple"));
            searchFilter->setSourceModel(MyMoneyFile::instance()->journalModel());
            searchLedgerView->setModel(searchFilter);

            // mirror the column and read-only setup of the result page
            searchLedgerView->setColumnsHidden(QVector<int>{
                JournalModel::Column::Invisible,
                JournalModel::Column::Number,
                JournalModel::Column::Security,
                JournalModel::Column::CostCenter,
                JournalModel::Column::Quantity,
                JournalModel::Column::Price,
                JournalModel::Column::Amount,
                JournalModel::Column::Value,
                JournalModel::Column::Balance,
            });
            searchLedgerView->setColumnsShown(QVector<int>{
                JournalModel::Column::Date,
                JournalModel::Column::Account,
                JournalModel::Column::Detail,
                JournalModel::Column::Reconciliation,
                JournalModel::Column::Payment,
                JournalModel::Column::Deposit,
            });
            searchLedgerView->setEditTriggers(QAbstractItemView::NoEditTriggers);

            // the filterApplied signal also covers invalidations that don't
            // change any row (e.g. one no-match expression replacing another);
            // the row signals keep the count alive while the file changes
            const auto updateFoundText = [this]() {
                updateSearchFoundText();
            };
            q->connect(searchFilter, &LedgerSearchFilter::filterApplied, q, updateFoundText);
            q->connect(searchFilter, &QAbstractItemModel::rowsInserted, q, updateFoundText);
            q->connect(searchFilter, &QAbstractItemModel::rowsRemoved, q, updateFoundText);
            q->connect(searchFilter, &QAbstractItemModel::modelReset, q, updateFoundText);
            q->connect(searchLedgerView, &QAbstractItemView::doubleClicked, q, [this](const QModelIndex& idx) {
                selectTransaction(idx);
            });
            updateSearchFoundText();
        }
    }

    void updateSearchFoundText()
    {
        if (searchFilter->data(QModelIndex(), eMyMoney::Model::ActiveFilterTextRole).toBool()) {
            searchFoundText->setText(i18np("Found %1 matching transaction", "Found %1 matching transactions", searchFilter->rowCount()));
        } else {
            searchFoundText->setText(i18nc("@info:status", "Enter text to search all transactions"));
        }
    }

    void search()
    {
        // perform the search only if the button is enabled
        if (!ui.buttonBox->button(QDialogButtonBox::Apply)->isEnabled())
            return;

        filterModel->setSourceModel(MyMoneyFile::instance()->journalModel());
        ui.m_ledgerView->setModel(filterModel);

        // setup the filter from the dialog widgets
        filterModel->setFilterRole(eMyMoney::Model::Roles::IdRole);
        const auto filter = filterTab->setupFilter();
        const auto list = MyMoneyFile::instance()->journalEntryIds(filter);
        filterModel->setFilterFixedStrings(list);

        QVector<int> columns;
        columns = {
            JournalModel::Column::Invisible,
            JournalModel::Column::Number,
            JournalModel::Column::Security,
            JournalModel::Column::CostCenter,
            JournalModel::Column::Quantity,
            JournalModel::Column::Price,
            JournalModel::Column::Amount,
            JournalModel::Column::Value,
            JournalModel::Column::Balance,
        };
        ui.m_ledgerView->setColumnsHidden(columns);
        columns = {
            JournalModel::Column::Date,
            JournalModel::Column::Account,
            JournalModel::Column::Detail,
            JournalModel::Column::Reconciliation,
            JournalModel::Column::Payment,
            JournalModel::Column::Deposit,
        };
        ui.m_ledgerView->setColumnsShown(columns);

        ui.m_tabWidget->setTabEnabled(ui.m_tabWidget->indexOf(ui.m_resultPage), true);
        ui.m_tabWidget->setCurrentWidget(ui.m_resultPage);
        ui.m_ledgerView->setFocus();

        ui.m_foundText->setText(i18np("Found %1 matching splits", "Found %1 matching splits", filterModel->rowCount()));
    }

    void selectTransaction(const QModelIndex& idx)
    {
        Q_Q(KSearchTransactionDlg);

        const auto accountId = idx.data(eMyMoney::Model::JournalSplitAccountIdRole).toString();
        SelectedObjects selections;
        selections.setSelection(SelectedObjects::Account, accountId);
        selections.setSelection(SelectedObjects::JournalEntry, idx.data(eMyMoney::Model::IdRole).toString());

        // close myself and open the ledger of the account
        q->hide();

        Q_EMIT q->requestSelectionChange(selections);
        pActions[eMenu::Action::GoToAccount]->setData(accountId);
        MyMoneyUtils::triggerAction(pActions[eMenu::Action::GoToAccount]);
    }

    KSearchTransactionDlg* q_ptr;
    LedgerJournalIdFilter* filterModel;
    Ui_KSearchTransactionDlg ui;
    QPointer<KTransactionFilter> filterTab;
    // instant search tab, only created in simplified mode
    LedgerSearchFilter* searchFilter = nullptr;
    QLineEdit* searchLineEdit = nullptr;
    LedgerView* searchLedgerView = nullptr;
    QLabel* searchFoundText = nullptr;
    QWidget* searchPage = nullptr;
};

KSearchTransactionDlg::KSearchTransactionDlg(QWidget* parent)
    : QDialog(parent)
    , d_ptr(new KSearchTransactionDlgPrivate(this))
{
    Q_D(KSearchTransactionDlg);
    d->init();

    connect(d->ui.buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, [&]() {
        Q_D(KSearchTransactionDlg);
        d->search();
    });

    connect(d->ui.buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this, [&]() {
        Q_D(KSearchTransactionDlg);
        d->filterTab->slotReset();
    });

    connect(d->ui.buttonBox->button(QDialogButtonBox::Help), &QAbstractButton::clicked, this, [&]() {
        Q_D(KSearchTransactionDlg);
        // resolve by widget, the criteria tab is not at index 0 when the
        // simplified mode's search tab is present
        if (d->ui.m_tabWidget->currentWidget() == d->filterTab) {
            d->filterTab->slotShowHelp();
        }
    });

    connect(d->ui.buttonBox->button(QDialogButtonBox::Close), &QAbstractButton::clicked, this, &QObject::deleteLater);

    if (d->searchPage) {
        d->searchLineEdit->setFocus();
        d->searchLedgerView->installEventFilter(this);
        d->searchLineEdit->installEventFilter(this);
        // watch my own Hide/Show to suspend the global filter while invisible
        installEventFilter(this);
    } else {
        d->filterTab->setFocus();
    }

    // we don't allow editing here but double click selects the current
    // selected transaction in the ledger view
    d->ui.m_ledgerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(d->ui.m_ledgerView, &QAbstractItemView::doubleClicked, this, [&](const QModelIndex& idx) {
        Q_D(KSearchTransactionDlg);
        d->selectTransaction(idx);
    });

    d->ui.m_ledgerView->installEventFilter(this);
}

KSearchTransactionDlg::~KSearchTransactionDlg()
{
}

bool KSearchTransactionDlg::eventFilter(QObject* watched, QEvent* event)
{
    Q_D(KSearchTransactionDlg);
    if (watched == d->ui.m_ledgerView) {
        if (event->type() == QEvent::KeyPress) {
            const auto kev = static_cast<QKeyEvent*>(event);
            if ((kev->key() == Qt::Key_Enter) || (kev->key() == Qt::Key_Return)) {
                d->selectTransaction(d->ui.m_ledgerView->currentIndex());
            }
        }
    } else if (d->searchPage && (watched == d->searchLedgerView)) {
        if (event->type() == QEvent::KeyPress) {
            const auto kev = static_cast<QKeyEvent*>(event);
            if ((kev->key() == Qt::Key_Enter) || (kev->key() == Qt::Key_Return)) {
                const auto idx = d->searchLedgerView->currentIndex();
                if (idx.isValid()) {
                    d->selectTransaction(idx);
                }
                // consume, otherwise the dialog clicks the default (Find) button
                return true;
            }
        }
    } else if (d->searchPage && (watched == d->searchLineEdit)) {
        if (event->type() == QEvent::KeyPress) {
            const auto kev = static_cast<QKeyEvent*>(event);
            if ((kev->key() == Qt::Key_Enter) || (kev->key() == Qt::Key_Return)) {
                // don't act on results of the previous expression
                d->searchFilter->flushPendingFilter();
                if (!d->searchLedgerView->currentIndex().isValid() && (d->searchFilter->rowCount() > 0)) {
                    d->searchLedgerView->setCurrentIndex(d->searchFilter->index(0, 0));
                }
                d->searchLedgerView->setFocus();
                // consume, otherwise the dialog clicks the default (Find) button
                return true;
            } else if ((kev->key() == Qt::Key_Down) || (kev->key() == Qt::Key_Up)) {
                // let the arrow keys walk the result list while typing
                d->searchFilter->flushPendingFilter();
                if (!d->searchLedgerView->currentIndex().isValid()) {
                    if (d->searchFilter->rowCount() > 0) {
                        d->searchLedgerView->setCurrentIndex(d->searchFilter->index(0, 0));
                    }
                } else {
                    QCoreApplication::sendEvent(d->searchLedgerView, event);
                }
                return true;
            }
        }
    } else if (watched == this) {
        // never scan the whole journal (e.g. during imports) while invisible
        if (d->searchFilter) {
            if (event->type() == QEvent::Hide) {
                d->searchFilter->setSuspended(true);
            } else if (event->type() == QEvent::Show) {
                d->searchFilter->setSuspended(false);
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}
