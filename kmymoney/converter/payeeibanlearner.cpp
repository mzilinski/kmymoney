/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "payeeibanlearner.h"

#include "mymoneypayee.h"
#include "payeeidentifier/ibanbic/ibanbic.h"
#include "payeeidentifier/payeeidentifiertyped.h"

bool learnPayeeIban(MyMoneyPayee& payee, const QString& iban, const QString& bic, const QString& ownerName)
{
    if (iban.isEmpty() || !payeeIdentifiers::ibanBic::isIbanValid(iban))
        return false;

    // Skip if the payee already knows this account. Dedup on the normalized
    // electronic IBAN only: ibanBic's value equality also compares BIC and owner
    // name, which would accrete near-duplicates as those vary across statements.
    const QString electronicIban = payeeIdentifiers::ibanBic::ibanToElectronic(iban);
    const auto identifiers = payee.payeeIdentifiers();
    for (const auto& ident : identifiers) {
        if (ident.iid() != payeeIdentifiers::ibanBic::staticPayeeIdentifierIid())
            continue;
        try {
            payeeIdentifierTyped<payeeIdentifiers::ibanBic> storedIban(ident);
            if (storedIban->electronicIban() == electronicIban)
                return false;
        } catch (const payeeIdentifier::badCast&) {
        } catch (const payeeIdentifier::empty&) {
        }
    }

    // Append a new identifier; never overwrite an existing one.
    payeeIdentifierTyped<payeeIdentifiers::ibanBic> learnedIban(new payeeIdentifiers::ibanBic);
    learnedIban->setIban(iban);
    learnedIban->setBic(bic);
    learnedIban->setOwnerName(ownerName);
    payee.addPayeeIdentifier(learnedIban);
    return true;
}
