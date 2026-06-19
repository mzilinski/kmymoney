/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sepastandingorder.h"

sepaStandingOrder::sepaStandingOrder()
    : onlineTask()
    , creditTransfer()
{
}

sepaStandingOrder::sepaStandingOrder(const sepaStandingOrder& other)
    : onlineTask(other)
    , creditTransfer(other)
{
}
