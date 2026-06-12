/*
 *    SPDX-FileCopyrightText: 2022 Thomas Baumgart <tbaumgart@kde.org>
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

// ----------------------------------------------------------------------------
// QT Includes

#include <QAbstractButton>
#include <QLineEdit>
#include <QTimer>

#include <utility>

// ----------------------------------------------------------------------------
// KDE Headers

#include <KLocalizedString>
#include <KMessageBox>

// ----------------------------------------------------------------------------
// Project Includes

#include "accountcreator.h"
#include "accountsmodel.h"
#include "kmymoneyaccountcombo.h"
#include "knewaccountdlg.h"
#include "mymoneyaccount.h"
#include "mymoneyfile.h"

AccountCreator::AccountCreator(QObject* parent)
    : QObject(parent)
    , m_comboBox(nullptr)
    , m_accountType(eMyMoney::Account::Type::Unknown)
{
}

void AccountCreator::addButton(QAbstractButton* button)
{
    m_buttons.append(button);
}

void AccountCreator::setAccountType(eMyMoney::Account::Type type)
{
    m_accountType = type;
}

void AccountCreator::setComboBox(KMyMoneyAccountCombo* cb)
{
    m_comboBox = cb;
}

void AccountCreator::createAccount()
{
    QTimer::singleShot(150, this, [&]() {
        // wait another round if any of the buttons is pressed
        if (std::any_of(m_buttons.cbegin(), m_buttons.cend(), [&](QAbstractButton* b) -> bool {
                return b->isDown();
            })) {
            createAccount();
            return;
        }

        // Simplified mode: typing the plain name of exactly one existing
        // (open) account of the matching hierarchies selects that account
        // instead of running the creation flow, so a user entering
        // "Lebensmittel" is not asked to create what "Ausgabe:Lebensmittel"
        // already provides.
        if (MyMoneyFile::instance()->simpleMode()) {
            const auto typedName = m_comboBox->currentText();
            if (!typedName.contains(MyMoneyAccount::accountSeparator())) {
                const bool wantAccount = (m_accountType == eMyMoney::Account::Type::Asset) || (m_accountType == eMyMoney::Account::Type::Liability);
                QList<MyMoneyAccount> accounts;
                MyMoneyFile::instance()->accountList(accounts);
                QString matchedId;
                bool unique = true;
                for (const auto& acc : std::as_const(accounts)) {
                    // a security or investment account never makes a counter account
                    const bool inScope = wantAccount ? (acc.isAssetLiability() && !acc.isInvest() && acc.accountType() != eMyMoney::Account::Type::Investment)
                                                     : acc.isIncomeExpense();
                    if (inScope && !acc.isClosed() && (acc.name().compare(typedName, Qt::CaseInsensitive) == 0)) {
                        if (matchedId.isEmpty()) {
                            matchedId = acc.id();
                        } else if (acc.id() != matchedId) {
                            unique = false;
                            break;
                        }
                    }
                }
                if (unique && !matchedId.isEmpty()) {
                    if (m_comboBox->getSelected() == matchedId) {
                        // already selected, only the display is stale: normalize it
                        // to the full name so the editors' full-name comparison
                        // does not re-trigger the creator on every focus out
                        const auto idx = MyMoneyFile::instance()->accountsModel()->indexById(matchedId);
                        m_comboBox->lineEdit()->setText(idx.data(eMyMoney::Model::AccountFullNameRole).toString());
                        deleteLater();
                        return;
                    }
                    m_comboBox->setSelected(matchedId);
                    if (m_comboBox->getSelected() == matchedId) {
                        deleteLater();
                        return;
                    }
                    // the account is not selectable in this combo (e.g. its model
                    // only contains one hierarchy half) and setSelected() cleared
                    // the text: restore it and run the regular creation flow
                    m_comboBox->lineEdit()->setText(typedName);
                }
            }
        }

        // determine the top parent account
        MyMoneyAccount parent;
        if (m_accountType == eMyMoney::Account::Type::Asset) {
            parent = MyMoneyFile::instance()->asset();
        } else if (m_accountType == eMyMoney::Account::Type::Liability) {
            parent = MyMoneyFile::instance()->liability();
        } else if (m_accountType == eMyMoney::Account::Type::Expense) {
            parent = MyMoneyFile::instance()->expense();
        } else if (m_accountType == eMyMoney::Account::Type::Income) {
            parent = MyMoneyFile::instance()->income();
        }

        MyMoneyAccount account;

        const auto fullName = m_comboBox->currentText();
        QString newAccountName;
        if (fullName.contains(MyMoneyAccount::accountSeparator())) {
            const auto file = MyMoneyFile::instance();
            const auto lastSeparatorPos = fullName.lastIndexOf(MyMoneyAccount::accountSeparator());
            newAccountName = fullName.mid(lastSeparatorPos + 1);
            const auto newParentName = fullName.left(lastSeparatorPos);
            const auto topLevelAccountName = parent.name();
            const auto idx = file->accountsModel()->indexById(parent.id());

            // search in the preferred sub-tree based on amount entered
            parent = file->accountsModel()->itemByName(newParentName, idx);
            // if not found in the preferred sub-tree, we search all
            if (parent.id().isEmpty()) {
                parent = file->accountsModel()->itemByName(newParentName, QModelIndex());
            }
            // if still not found, we inform the user
            if (parent.id().isEmpty()) {
                KMessageBox::error(m_comboBox,
                                   i18nc("@info %1 selected parent, %2 top level account",
                                         "The selected parent account <b>%1</b> does not exist in the <b>%2</b> hierarchy.",
                                         newParentName,
                                         topLevelAccountName),
                                   i18n("Account/category creation problem"));
            }
        } else {
            newAccountName = m_comboBox->currentText();
        }

        if (!parent.id().isEmpty()) {
            account.setName(newAccountName);

            const bool isAccount = (m_accountType == eMyMoney::Account::Type::Asset) || (m_accountType == eMyMoney::Account::Type::Liability);
            const auto creator = isAccount ? &KNewAccountDlg::newAccount : &KNewAccountDlg::newCategory;
            const QString undoAction = isAccount ? i18nc("Create undo action", "Create account") : i18nc("Create undo action", "Create category");

            MyMoneyFileTransaction ft(undoAction, false);
            creator(account, parent);

            // if the creation worked, we move on
            if (!account.id().isEmpty()) {
                ft.commit();
                m_comboBox->setSelected(account.id());
                auto widget = m_comboBox->nextInFocusChain();
                widget->setFocus();
            }
        }

        // in case the account/category was not created
        // we set the focus back on the widget we came from
        if (account.id().isEmpty()) {
            m_comboBox->setSelected(QString());
            m_comboBox->clearSelection();
            m_comboBox->setFocus();
        }

        // suicide, we're done
        deleteLater();
    });
}
