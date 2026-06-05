/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PAYEEIBANLEARNER_H
#define PAYEEIBANLEARNER_H

#include <QString>

class MyMoneyPayee;

/**
 * Append the counterparty account (@a iban, @a bic, @a ownerName) as an ibanBic
 * identifier to @a payee, so that later credit transfers can suggest it.
 *
 * The identifier is only added when @a iban is a syntactically valid IBAN and the
 * payee does not already know that account; deduplication is done on the
 * normalized (electronic) IBAN alone, and existing identifiers are never modified
 * (a payee may legitimately hold several accounts). An empty @a bic is accepted,
 * as the BIC is optional for intra-SEPA transfers.
 *
 * @return @c true if a new identifier was appended (the caller should persist the
 *         payee), @c false if nothing was changed.
 */
bool learnPayeeIban(MyMoneyPayee& payee, const QString& iban, const QString& bic, const QString& ownerName);

#endif // PAYEEIBANLEARNER_H
