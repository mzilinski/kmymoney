/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Separate plugin that provides ONLY the SEPA standing-order editor (LH-F-21).
// It must be its own KPluginFactory: the online-transfer form instantiates an
// editor via KPluginFactory::create<IonlineJobEdit>(), which cannot pick between
// two IonlineJobEdit implementations in one factory. The standing-order TASK and
// its SQL storage stay in the konlinetasks_sepa plugin.

#include <KPluginFactory>

#include "ui/sepastandingorderedit.h"

K_PLUGIN_FACTORY_WITH_JSON(konlinetasks_sepastandingorder_factory, "kmymoney-sepastandingorder.json", registerPlugin<sepaStandingOrderEdit>();)

#include "sepastandingorderloader.moc"
